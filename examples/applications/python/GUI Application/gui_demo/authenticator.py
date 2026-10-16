import os

try:
    from opendaq.authenticator import WheelModulesAuthenticator
except ImportError:
    WheelModulesAuthenticator = None


if WheelModulesAuthenticator is not None:
    class AppModuleAuthenticator(WheelModulesAuthenticator):
        vault_node_path = ('trusted_modules',)

        def __init__(self, parent, context):
            super().__init__()
            self.parent = parent
            self.context = context
            # module path -> list of trusted hashes (several allowed, e.g. module versions)
            self.trusted_modules = context.vault.node(*self.vault_node_path)

        def __call__(self, method, params):
            result = super().__call__(method, params)
            if method != 'authenticateModuleBinary' or result.get('BinaryValid', False):
                return result
            if not self.is_wheel_module(params['BinaryPath']):
                return result
            if self.auth_failed_prompt(params['BinaryPath']):
                result['BinaryValid'] = True
            return result

        def is_non_wheel_module_valid(self, path):
            return self.ask_trust(
                path, 'Module not authenticated',
                'This module is not part of the openDAQ python wheel and has not been authorized:')

        def auth_failed_prompt(self, path):
            """Asks the user whether to load a wheel module that failed authentication."""
            if os.path.basename(path) not in self.checksums:
                reason = ('This module was added to the openDAQ python wheel modules directory '
                          'and is not part of the original wheel build:')
            else:
                reason = ('This module in the openDAQ python wheel modules directory was modified '
                          'and does not match the one shipped with the wheel build:')
            return self.ask_trust(path, 'Module authentication failed', reason)

        def ask_trust(self, path, title, reason):
            """Loads the module if its hash is trusted in the vault, otherwise asks the user.
            Answering "Always" adds the hash to the vault."""
            key = self.vault_key(path)
            module_hash = self.hash_of(path)
            if module_hash in self.trusted_modules.get(key, []):
                return True
            # imported here, components import app_context which imports this module
            from .components.authorize_module_dialog import AuthorizeModuleDialog
            answer = AuthorizeModuleDialog(self.parent, self.context, path, title, reason).ask()
            if answer == AuthorizeModuleDialog.ALWAYS:
                self.trusted_modules.set(key, self.trusted_modules.get(key, []) + [module_hash])
            return answer != AuthorizeModuleDialog.NO

        @staticmethod
        def vault_key(path):
            return os.path.normcase(os.path.realpath(path))
else:
    AppModuleAuthenticator = None
