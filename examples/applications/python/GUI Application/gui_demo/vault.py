import json
import os
import platform
import tempfile


class VaultNode:
    """View of a node in the vault. Values are JSON-serializable; nested nodes are dicts."""

    def __init__(self, vault, path):
        self.vault = vault
        self.path = tuple(path)

    def node(self, *path):
        return VaultNode(self.vault, self.path + path)

    def get(self, key, default=None):
        return self._entries().get(key, default)

    def set(self, key, value):
        self._entries(create=True)[key] = value
        self.vault.save()

    def delete(self, key):
        entries = self._entries()
        if key in entries:
            del entries[key]
            self.vault.save()

    def keys(self):
        return list(self._entries().keys())

    def items(self):
        return list(self._entries().items())

    def __contains__(self, key):
        return key in self._entries()

    def _entries(self, create=False):
        entries = self.vault.data
        for i, name in enumerate(self.path):
            child = entries.get(name)
            if not isinstance(child, dict):
                if not create:
                    return {}
                if child is not None:
                    raise TypeError('Vault: {} is not a node'.format('/'.join(self.path[:i + 1])))
                child = entries[name] = {}
            entries = child
        return entries


class Vault(VaultNode):
    """User-scoped key-value store, persisted as a JSON file in the platform's standard config directory.
    Placeholder for a secure credential/secret store; the interface should stay the same."""

    file_name = 'vault.json'

    def __init__(self, path=None):
        VaultNode.__init__(self, self, ())
        self.file_path = path or self.default_path()
        self.data = self._load()

    @staticmethod
    def default_dir():
        if platform.system() == 'Windows':
            config_dir = os.environ.get('APPDATA', os.path.expanduser('~'))
        elif platform.system() == 'Darwin':
            config_dir = os.path.expanduser('~/Library/Application Support')
        else:
            config_dir = os.environ.get('XDG_CONFIG_HOME', os.path.expanduser('~/.config'))
        return os.path.join(config_dir, 'openDAQ')

    @classmethod
    def default_path(cls):
        return os.path.join(cls.default_dir(), cls.file_name)

    def reload(self):
        self.data = self._load()

    def save(self):
        directory = os.path.dirname(os.path.abspath(self.file_path))
        os.makedirs(directory, exist_ok=True)
        # write to a temp file and swap it in, so a crash never leaves a truncated vault
        fd, tmp_path = tempfile.mkstemp(dir=directory, prefix='.vault-', suffix='.tmp')
        try:
            with os.fdopen(fd, 'w') as f:
                json.dump(self.data, f, indent=4)
            if platform.system() != 'Windows':
                os.chmod(tmp_path, 0o600)
            os.replace(tmp_path, self.file_path)
        except BaseException:
            os.remove(tmp_path)
            raise

    def _load(self):
        if not os.path.exists(self.file_path):
            return {}
        with open(self.file_path) as f:
            data = json.load(f)
        if not isinstance(data, dict):
            raise ValueError('Vault: {} is not a JSON object'.format(self.file_path))
        return data
