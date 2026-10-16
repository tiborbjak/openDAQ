import os
import json
import hashlib

OPENDAQ_ERR_NOTIMPLEMENTED = 0x80000016

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
        if method != 'authenticateModuleBinary':
            return {'ErrorCode': OPENDAQ_ERR_NOTIMPLEMENTED, 'ErrorMessage': 'Authenticator: {} not implemented'.format(method)}
        path = params['BinaryPath']
        if not self.is_wheel_module(path):
            return {'BinaryValid' : self.is_non_wheel_module_valid(path)}
        entry = self.checksums.get(os.path.basename(path))
        if entry is None:
            return {}
        return {'BinaryValid': entry.get(self.hash_name) == self.hash_of(path), 'VendorKey': entry.get('vendor', '')}

    def is_non_wheel_module_valid(self, path):
        return True

    def modules_abs_path(self):
        return os.path.join(os.path.abspath(os.path.join(__file__, os.pardir)), self.modules_rel_path)

    def is_wheel_module(self, path):
        modules_abs_path = self.modules_abs_path()
        return os.path.commonpath([modules_abs_path, os.path.abspath(path)]) == modules_abs_path

    @classmethod
    def hash_of(cls, path):
        with open(path, 'rb') as f:
            return cls.hash(f.read())

    @classmethod
    def hash(cls, data):
        return hashlib.new(cls.hash_name, data).hexdigest()

    @classmethod
    def checksums_hash_var(cls):
        return 'CHECKSUM_FILE_{}'.format(cls.hash_name.upper())

    @classmethod
    def checksums_file_rel_path(cls):
        return os.path.join(cls.modules_rel_path, 'module_checksums.json')
