import os
import sys
import base64
import hashlib
import importlib.metadata

class WheelModulesAuthenticator:
    modules_rel_path = 'modules'

    def __init__(self):
        self.checksums = self.load_checksums()

    def __call__(self, method, params):
        # TODO: raising errors from the authenticator function does not propagate correctly yet, enable once it is fixed
        # if method != 'authenticateModuleBinary':
        #     raise NotImplementedError('Authenticator: {} not implemented'.format(method))
        path = params['BinaryPath']
        modules_dir = self.modules_dir()
        module_dir = os.path.normcase(os.path.realpath(os.path.dirname(os.path.abspath(path))))
        try:
            is_wheel_module = os.path.commonpath([modules_dir, module_dir]) == modules_dir
        except ValueError:  # different drives on Windows
            is_wheel_module = False
        if is_wheel_module:
            return self.auth_wheel_module(path)
        return self.auth_nonwheel_module(path)

    def auth_wheel_module(self, path):
        """Authenticates a module in the wheel modules directory against the hashes in the wheel RECORD."""
        valid, _ = self.check_wheel_module(path)
        return {'BinaryValid': valid, 'VendorKey': ''}

    def check_wheel_module(self, path):
        """Returns (valid, (hash algorithm, hex digest of the module)); the second item is None
        for a module that is not in the wheel RECORD, it is not hashed."""
        entry = self.checksums.get(os.path.basename(path))
        if entry is None:
            return False, None
        hash_name, expected_hash = entry
        module_hash = self.hash_of(path, hash_name)
        return module_hash == expected_hash, (hash_name, module_hash)

    def auth_nonwheel_module(self, path):
        """Authenticates a module outside the wheel modules directory."""
        return {'BinaryValid': True, 'VendorKey': ''}

    @classmethod
    def modules_dir(cls):
        return os.path.normcase(os.path.realpath(os.path.join(os.path.dirname(__file__), cls.modules_rel_path)))

    @classmethod
    def load_checksums(cls):
        """Maps the file names of the wheel modules to (hash algorithm, hex digest), read from the RECORD of the installed wheel.
        Modules without a usable hash are left out, so they fail authentication."""
        try:
            files = importlib.metadata.distribution(__package__).files
        except importlib.metadata.PackageNotFoundError:
            files = None
        if files is None:
            raise RuntimeError('No RECORD found for the {} distribution, it should be installed from the wheel'.format(__package__))
        modules_dir = cls.modules_dir()
        checksums = {}
        for file in files:
            # skip files of another installation of the package than the one this module is from
            if os.path.normcase(os.path.realpath(os.path.dirname(file.locate()))) != modules_dir:
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
            print('The {} wheel RECORD lists no modules in {}'.format(__package__, modules_dir), file=sys.stderr)
        return checksums

    @classmethod
    def hash_of(cls, path, hash_name):
        h = hashlib.new(hash_name)
        # read in chunks, modules can be large
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        return h.hexdigest()
