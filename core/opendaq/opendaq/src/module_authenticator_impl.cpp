#include <opendaq/module_authenticator_impl.h>

BEGIN_NAMESPACE_OPENDAQ

FunctionModuleAuthenticatorImpl::FunctionModuleAuthenticatorImpl(const FunctionPtr& f)
    : f(f)
{
}

Bool FunctionModuleAuthenticatorImpl::onAuthenticateModuleBinary(StringPtr& vendorKey, const StringPtr& binaryPath)
{
    auto params = Dict<IString, IBaseObject>();
    params["BinaryPath"] = binaryPath;
    DictPtr<IString, IBaseObject> ret = f("authenticateModuleBinary", params);
    if (auto errCode = ret.getOrDefault("ErrorCode", OPENDAQ_SUCCESS); errCode != OPENDAQ_SUCCESS)
        throwExceptionFromErrorCode(errCode, ret.getOrDefault("ErrorMessage", ""));
    vendorKey = ret.getOrDefault("VendorKey", "");
    return BoolPtr(ret.getOrDefault("BinaryValid", false));
}

OPENDAQ_DEFINE_CLASS_FACTORY_WITH_INTERFACE(LIBRARY_FACTORY, FunctionModuleAuthenticator, IModuleAuthenticator, IFunction*, function)

END_NAMESPACE_OPENDAQ
