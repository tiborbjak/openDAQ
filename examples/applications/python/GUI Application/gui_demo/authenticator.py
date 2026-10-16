import os
from tkinter import messagebox

try:
    from opendaq.authenticator import WheelModulesAuthenticator
except ImportError:
    WheelModulesAuthenticator = None


if WheelModulesAuthenticator is not None:
    class AppModuleAuthenticator(WheelModulesAuthenticator):
        def auth_wheel_module(self, path, hash):
            """Asks the user whether to load a wheel module that failed authentication."""
            ret = super().auth_wheel_module(path, hash)
            if not ret.get('BinaryValid', False):
                ret['BinaryValid'] = self.auth_failed_prompt(path)
            return ret

        def auth_failed_prompt(self, path):
            """Asks the user whether to load a module that failed authentication."""
            if os.path.basename(path) not in self.checksums:
                reason = ('This module was added to the openDAQ python wheel modules directory '
                          'and is not part of the original wheel build:')
            else:
                reason = ('This module in the openDAQ python wheel modules directory was modified '
                          'and does not match the one shipped with the wheel build:')
            return messagebox.askyesno(
                'Module authentication failed',
                '{}\n\n{}\n\nLoad it anyway?'.format(reason, path),
                icon='warning', default='no')
else:
    AppModuleAuthenticator = None
