import base64
import datetime
import enum
import hashlib
import importlib.metadata
import json
import os
import platform
import sys
import tempfile

import opendaq as daq

# the wheel's modules directory; only the wheel defines OPENDAQ_MODULES_DIR, None otherwise
WHEEL_MODULES_DIR = (os.path.normcase(os.path.realpath(os.path.join(daq.OPENDAQ_MODULES_DIR, 'modules')))
                     if hasattr(daq, 'OPENDAQ_MODULES_DIR') else None)


class ModuleAuth(enum.Enum):
    NONE = 'none'    # load all modules without authentication
    WHEEL = 'wheel'  # authenticate the wheel modules, load all other modules
    ALL = 'all'      # authenticate all modules


# the wheel mode only when running from the wheel, there are no wheel modules otherwise
MODULE_AUTH_MODES = [mode for mode in ModuleAuth if mode != ModuleAuth.WHEEL or WHEEL_MODULES_DIR is not None]
DEFAULT_MODULE_AUTH = ModuleAuth.WHEEL if WHEEL_MODULES_DIR is not None else ModuleAuth.NONE


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


class ModuleAuthenticator:
    """Authenticates the wheel modules against the hashes in the wheel RECORD, and with the ALL mode
    also the modules outside the wheel against the allowlist; asks the user about modules that fail."""

    hash_name = 'sha256'

    def __init__(self, mode, parent, context):
        self.parent = parent
        self.context = context
        self.allowlist = ModuleAllowlist(
            self.hash_name, os.path.join(context.config_dir, ModuleAllowlist.file_name))
        self.wheel_checksums = load_wheel_checksums() if WHEEL_MODULES_DIR is not None else None
        self.auth_other_modules = mode == ModuleAuth.ALL

    def __call__(self, method, params):
        # TODO: raising errors from the authenticator function does not propagate correctly yet, enable once it is fixed
        # if method != 'authenticateModuleBinary':
        #     raise NotImplementedError('Authenticator: {} not implemented'.format(method))
        return {'BinaryValid': self.auth(params['BinaryPath']), 'VendorKey': ''}

    def auth(self, path):
        if self.wheel_checksums is not None and is_wheel_module(path):
            return self.auth_wheel_module(path)
        if not self.auth_other_modules:
            return True
        return self.ask_trust(path, hash_of(path, self.hash_name), 'Module not authenticated',
                              'This module has not been authorized:')

    def auth_wheel_module(self, path):
        """Checks the module against the wheel RECORD, asks the user if it fails."""
        entry = self.wheel_checksums.get(os.path.basename(path))
        if entry is None:
            reason = ('This module was added to the openDAQ python wheel modules directory '
                      'and is not part of the original wheel build:')
            module_hash = hash_of(path, self.hash_name)
        else:
            record_hash_name, expected_hash = entry
            module_hash = hash_of(path, record_hash_name)
            if module_hash == expected_hash:
                return True
            reason = ('This module in the openDAQ python wheel modules directory was modified '
                      'and does not match the one shipped with the wheel build:')
            # reuse the RECORD check's hash if it is the allowlist algorithm
            if record_hash_name != self.hash_name:
                module_hash = hash_of(path, self.hash_name)
        return self.ask_trust(path, module_hash, 'Module authentication failed', reason)

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


def create_module_authenticator(mode, parent, context):
    """The authenticator for the mode; None if modules are not authenticated,
    or if there are no wheel modules to authenticate (not running from the wheel)."""
    if mode == ModuleAuth.NONE or (mode == ModuleAuth.WHEEL and WHEEL_MODULES_DIR is None):
        return None
    return ModuleAuthenticator(mode, parent, context)


def is_wheel_module(path):
    module_dir = os.path.normcase(os.path.realpath(os.path.dirname(os.path.abspath(path))))
    try:
        return os.path.commonpath([WHEEL_MODULES_DIR, module_dir]) == WHEEL_MODULES_DIR
    except ValueError:  # different drives on Windows
        return False


def hash_of(path, hash_name):
    h = hashlib.new(hash_name)
    # read in chunks, modules can be large
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def load_wheel_checksums():
    """Maps the file names of the wheel modules to (hash algorithm, hex digest), read from the RECORD of the installed wheel.
    Modules without a usable hash are left out, so they fail authentication."""
    try:
        files = importlib.metadata.distribution('opendaq').files
    except importlib.metadata.PackageNotFoundError:
        files = None
    if files is None:
        raise RuntimeError('No RECORD found for the opendaq distribution, it should be installed from the wheel')
    checksums = {}
    for file in files:
        # skip files of another installation of the package than the one in use
        if os.path.normcase(os.path.realpath(os.path.dirname(file.locate()))) != WHEEL_MODULES_DIR:
            continue
        if file.hash is None:
            print('No hash for {} in the wheel RECORD'.format(file), file=sys.stderr)
            continue
        if file.hash.mode not in hashlib.algorithms_available:
            print('Unsupported hash algorithm {} for {} in the wheel RECORD'.format(file.hash.mode, file), file=sys.stderr)
            continue
        # RECORD stores urlsafe base64 digests without padding
        digest = base64.urlsafe_b64decode(file.hash.value + '=' * (-len(file.hash.value) % 4))
        checksums[file.name] = (file.hash.mode, digest.hex())
    if not checksums:
        print('The opendaq wheel RECORD lists no modules in {}'.format(WHEEL_MODULES_DIR), file=sys.stderr)
    return checksums
