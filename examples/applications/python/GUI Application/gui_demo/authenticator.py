try:
    from opendaq.authenticator import WheelModulesAuthenticator
except ImportError:
    WheelModulesAuthenticator = None


if WheelModulesAuthenticator is not None:
    class AppModuleAuthenticator(WheelModulesAuthenticator):
        pass
else:
    AppModuleAuthenticator = None
