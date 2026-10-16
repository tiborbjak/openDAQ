import os
import json
import hashlib

class WheelModulesAuthenticator:
    hash_name = 'sha256'
    modules_rel_path = 'modules'

    def __init__(self):
        checksum_file_path = os.path.join(os.path.abspath(os.path.join(__file__, os.pardir)), self.checksums_file_rel_path())
        expected_hash = globals().get(self.checksums_hash_var())
        if expected_hash is None:
            raise RuntimeError('{} is not defined. It should be harcoded during wheel build'.format(self.checksums_hash_var()))
        if self.hash_of(checksum_file_path) != expected_hash:
            raise RuntimeError('Fatal: {} has been modified'.format(self.checksums_file_rel_path()))
        with open(checksum_file_path) as f:
            self.checksums = json.load(f)

    def __call__(self, method, params):
        # TODO: raising errors from the authenticator function does not propagate correctly yet, enable once it is fixed
        # if method != 'authenticateModuleBinary':
        #     raise NotImplementedError('Authenticator: {} not implemented'.format(method))
        path = params['BinaryPath']
        modules_dir = os.path.normcase(os.path.realpath(os.path.join(os.path.dirname(__file__), self.modules_rel_path)))
        module_dir = os.path.normcase(os.path.realpath(os.path.dirname(os.path.abspath(path))))
        try:
            is_wheel_module = os.path.commonpath([modules_dir, module_dir]) == modules_dir
        except ValueError:  # different drives on Windows
            is_wheel_module = False
        if is_wheel_module:
            return self.auth_wheel_module(path, self.hash_of(path))
        return self.auth_nonwheel_module(path)

    def auth_wheel_module(self, path, hash):
        """Authenticates a module in the wheel modules directory against the checksum list."""
        entry = self.checksums.get(os.path.basename(path))
        valid = entry is not None and entry.get(self.hash_name) == hash
        return {'BinaryValid': valid, 'VendorKey': entry.get('vendor', '') if valid else ''}

    def auth_nonwheel_module(self, path):
        """Authenticates a module outside the wheel modules directory."""
        return {'BinaryValid': True, 'VendorKey': ''}

    @classmethod
    def hash_of(cls, path):
        h = hashlib.new(cls.hash_name)
        # read in chunks, modules can be large
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        return h.hexdigest()

    @classmethod
    def checksums_hash_var(cls):
        return 'CHECKSUM_FILE_{}'.format(cls.hash_name.upper())

    @classmethod
    def checksums_file_rel_path(cls):
        return os.path.join(cls.modules_rel_path, 'module_checksums.json')
