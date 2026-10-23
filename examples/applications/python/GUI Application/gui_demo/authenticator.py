import datetime
import enum
import hashlib
import json
import os
import platform
import sys
import tempfile

try:
    from opendaq import authenticator as wheel_authenticator
except ImportError:
    wheel_authenticator = None

OPENDAQ_ERR_NOTIMPLEMENTED = 0x80000016


class ModuleAuth(enum.Enum):
    NONE = 'none'    # load all modules without authentication
    WHEEL = 'wheel'  # authenticate the wheel modules, load all other modules
    ALL = 'all'      # authenticate all modules


# the wheel mode only when running from the wheel, there are no wheel modules otherwise
MODULE_AUTH_MODES = [mode for mode in ModuleAuth if mode != ModuleAuth.WHEEL or wheel_authenticator is not None]
DEFAULT_MODULE_AUTH = ModuleAuth.WHEEL if wheel_authenticator is not None else ModuleAuth.NONE


class ModuleAllowlist:
    """User-scoped list of module hashes the user chose to always load, persisted as JSON:
    a list of {<hash_name>: ..., 'timestamp': ..., 'file': ...} entries, one per allowed file;
    several entries can have the same hash."""

    file_name = 'module_allowlist.json'

    class Status(enum.Enum):
        ALLOWED = 'allowed'            # there is an entry for the file with this hash
        ALLOWED_NEW_FILE = 'new file'  # the hash is allowed, but there is no entry for the file with it
        CHANGED = 'changed'            # there is an entry for the file, but with a different hash
        UNKNOWN = 'unknown'            # there is no entry for the hash or for the file

    def __init__(self, hash_name, path):
        self.hash_name = hash_name
        self.path = path
        # an unreadable list is not fatal: start empty, and keep the file as a backup on the first save
        self.backup_on_save = False
        try:
            self.entries = self._load()
        except (OSError, ValueError) as e:
            print('Reading the module allowlist {} failed, starting with an empty one: {}'.format(self.path, e), file=sys.stderr)
            self.entries = []
            self.backup_on_save = True
        self.hashes = {entry[self.hash_name] for entry in self.entries}

    def check(self, module_hash, file_path):
        file_key = self._file_key(file_path)
        file_entries = [entry for entry in self.entries if entry['file'] == file_key]
        if any(entry[self.hash_name] == module_hash for entry in file_entries):
            return self.Status.ALLOWED
        if module_hash in self.hashes:
            return self.Status.ALLOWED_NEW_FILE
        if file_entries:
            return self.Status.CHANGED
        return self.Status.UNKNOWN

    def allow(self, module_hash, file_path):
        """Adds an entry for the file with the hash, replacing the file's existing entries, and saves the list."""
        file_key = self._file_key(file_path)
        self.entries = [entry for entry in self.entries if entry['file'] != file_key]
        self.entries.append({
            self.hash_name: module_hash,
            'timestamp': datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds'),
            'file': file_key,
        })
        self.hashes = {entry[self.hash_name] for entry in self.entries}
        self.save()

    def save(self):
        directory = os.path.dirname(os.path.abspath(self.path))
        os.makedirs(directory, exist_ok=True)
        if self.backup_on_save and os.path.exists(self.path):
            os.replace(self.path, self.path + '.bak')
        self.backup_on_save = False
        # write to a temp file and swap it in, so a crash never leaves a truncated allowlist
        fd, tmp_path = tempfile.mkstemp(dir=directory, prefix='.module_allowlist-', suffix='.tmp')
        try:
            with os.fdopen(fd, 'w') as f:
                json.dump(self.entries, f, indent=4)
            if platform.system() != 'Windows':
                os.chmod(tmp_path, 0o600)
            os.replace(tmp_path, self.path)
        except BaseException:
            os.remove(tmp_path)
            raise

    @staticmethod
    def _file_key(file_path):
        return os.path.normcase(os.path.abspath(file_path))

    def _load(self):
        if not os.path.exists(self.path):
            return []
        with open(self.path) as f:
            entries = json.load(f)
        if not isinstance(entries, list) or not all(
                isinstance(entry, dict) and isinstance(entry.get(self.hash_name), str) and isinstance(entry.get('file'), str)
                for entry in entries):
            raise ValueError('{} is not a list of module entries'.format(self.path))
        # paths edited by hand may not be normalized
        for entry in entries:
            entry['file'] = self._file_key(entry['file'])
        return entries


class AppModuleAuthenticator:
    """Asks the user about modules that are not in the allowlist."""

    hash_name = 'sha256'

    def __init__(self, parent, context):
        self.parent = parent
        self.context = context
        self.allowlist = ModuleAllowlist(
            self.hash_name, os.path.join(context.config_dir, ModuleAllowlist.file_name))

    def __call__(self, method, params):
        if method != 'authenticateModuleBinary':
            return {'ErrorCode': OPENDAQ_ERR_NOTIMPLEMENTED, 'ErrorMessage': 'Authenticator: {} not implemented'.format(method)}
        return self.auth_module(params['BinaryPath'])

    def auth_module(self, path, reason='This module has not been authorized:'):
        return {'BinaryValid': self.ask_trust(path, self.hash_of(path), 'Module not authenticated', reason)}

    def ask_trust(self, path, module_hash, title, reason):
        """Loads the module if its hash is in the allowlist, otherwise asks the user.
        Answering "Always" adds the module to the allowlist."""
        status = self.allowlist.check(module_hash, path)
        if status == ModuleAllowlist.Status.ALLOWED:
            return True
        if status == ModuleAllowlist.Status.ALLOWED_NEW_FILE:
            self.allow(module_hash, path)
            return True
        if status == ModuleAllowlist.Status.CHANGED:
            title = 'Module has been modified'
            reason = 'This module was allowed before, but it has changed since:'
        # imported here, components import app_context which imports this module
        from .components.authorize_module_dialog import AuthorizeModuleDialog
        answer = AuthorizeModuleDialog(self.parent, self.context, path, title, reason).ask()
        if answer == AuthorizeModuleDialog.ALWAYS:
            self.allow(module_hash, path)
        return answer != AuthorizeModuleDialog.NO

    def allow(self, module_hash, path):
        """Adds the module to the allowlist. A failed save only warns: the module is still
        allowed for this session, and an exception here would reject it."""
        try:
            self.allowlist.allow(module_hash, path)
        except OSError as e:
            print('Saving the module allowlist failed:', e, file=sys.stderr)

    @classmethod
    def hash_of(cls, path):
        h = hashlib.new(cls.hash_name)
        # read in chunks, modules can be large
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        return h.hexdigest()


if wheel_authenticator is not None:
    # the wheel authenticator comes first: its __call__ tells wheel modules from the others,
    # and its auth_nonwheel_module loads all modules outside the wheel
    class WheelModulesAuthenticator(wheel_authenticator.WheelModulesAuthenticator, AppModuleAuthenticator):
        """Authenticates the wheel modules, asking the user about those that fail; loads all other modules."""

        def __init__(self, parent, context):
            wheel_authenticator.WheelModulesAuthenticator.__init__(self)
            AppModuleAuthenticator.__init__(self, parent, context)

        def auth_wheel_module(self, path, hash):
            """Asks the user whether to load a wheel module that failed authentication."""
            ret = super().auth_wheel_module(path, hash)
            if ret['BinaryValid']:
                return ret
            if os.path.basename(path) not in self.checksums:
                reason = ('This module was added to the openDAQ python wheel modules directory '
                          'and is not part of the original wheel build:')
            else:
                reason = ('This module in the openDAQ python wheel modules directory was modified '
                          'and does not match the one shipped with the wheel build:')
            ret['BinaryValid'] = self.ask_trust(path, hash, 'Module authentication failed', reason)
            return ret
else:
    WheelModulesAuthenticator = None


class AllModulesAuthenticator(WheelModulesAuthenticator or AppModuleAuthenticator):
    """Also asks the user about modules outside the wheel that are not in the allowlist."""

    def auth_nonwheel_module(self, path):
        """Called by the wheel authenticator only, without the wheel all modules go through auth_module."""
        return self.auth_module(path, 'This module is not part of the openDAQ python wheel and has not been authorized:')


def create_module_authenticator(mode, parent, context):
    """The authenticator for the mode; None if modules are not authenticated,
    or if there are no wheel modules to authenticate (not running from the wheel)."""
    if mode == ModuleAuth.NONE or (mode == ModuleAuth.WHEEL and WheelModulesAuthenticator is None):
        return None
    if mode == ModuleAuth.ALL:
        return AllModulesAuthenticator(parent, context)
    return WheelModulesAuthenticator(parent, context)
