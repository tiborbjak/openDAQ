#include <opendaq/add_streaming_config_impl.h>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_internal_ptr.h>

BEGIN_NAMESPACE_OPENDAQ

namespace add_streaming_config
{
static const char* TypeConfig = "TypeConfig";
}

AddStreamingConfigImpl::AddStreamingConfigImpl(const StreamingTypePtr& streamingType, const PropertyObjectPtr& linkedTypeConfig)
    : AddStreamingConfigImpl(streamingType, PassKey())
{
    this->linkedTypeConfig = linkedTypeConfig;
    if (!linkedTypeConfig.assigned())
        Super::addProperty(ObjectProperty(add_streaming_config::TypeConfig, streamingType.createDefaultConfig()));
}

AddStreamingConfigImpl::AddStreamingConfigImpl(const StreamingTypePtr& streamingType, PassKey)
    : Super()
    , streamingType(streamingType)
    , serializedWithDeviceConfig(false)
{
}

ErrCode AddStreamingConfigImpl::getStreamingType(IStreamingType** type)
{
    OPENDAQ_PARAM_NOT_NULL(type);

    *type = streamingType.addRefAndReturn();
    return OPENDAQ_SUCCESS;
}

ErrCode AddStreamingConfigImpl::getTypeConfig(IPropertyObject** config)
{
    OPENDAQ_PARAM_NOT_NULL(config);

    if (linkedTypeConfig.assigned())
    {
        *config = linkedTypeConfig.addRefAndReturn();
        return OPENDAQ_SUCCESS;
    }

    return daqTry([&] { *config = objPtr.getPropertyValue(add_streaming_config::TypeConfig).asPtr<IPropertyObject>().detach(); });
}

ErrCode AddStreamingConfigImpl::getInterfaceIds(SizeT* idCount, IntfID** ids)
{
    OPENDAQ_PARAM_NOT_NULL(idCount);

    *idCount = InterfaceIds::Count() + 1;
    if (ids == nullptr)
        return OPENDAQ_SUCCESS;

    **ids = IPropertyObject::Id;
    (*ids)++;

    InterfaceIds::AddInterfaceIds(*ids);
    return OPENDAQ_SUCCESS;
}

ErrCode AddStreamingConfigImpl::clone(IPropertyObject** cloned)
{
    OPENDAQ_PARAM_NOT_NULL(cloned);

    return daqTry([&]
    {
        auto lock = getRecursiveConfigLock2();
        auto obj = createWithImplementation<IAddStreamingConfig, AddStreamingConfigImpl>(streamingType, PassKey());
        dynamic_cast<AddStreamingConfigImpl*>(obj.getObject())->configureClonedMembers(getCloneParameters());
        if (linkedTypeConfig.assigned())
            obj.addProperty(ObjectProperty(add_streaming_config::TypeConfig, linkedTypeConfig.asPtr<IPropertyObjectInternal>().clone()));
        *cloned = obj.detach();
    });
}

ErrCode AddStreamingConfigImpl::serialize(ISerializer* serializer)
{
    OPENDAQ_PARAM_NOT_NULL(serializer);

    return daqTry([&]
    {
        auto lock = getRecursiveConfigLock2();
        if (!linkedTypeConfig.assigned() || serializedWithDeviceConfig)
        {
            checkErrorInfo(Super::serialize(serializer));
            return;
        }

        PropertyObjectPtr standalone;
        checkErrorInfo(clone(&standalone));
        standalone.serialize(serializer);
    });
}

ErrCode AddStreamingConfigImpl::getSerializeId(ConstCharPtr* id) const
{
    OPENDAQ_PARAM_NOT_NULL(id);

    *id = SerializeId();
    return OPENDAQ_SUCCESS;
}

ConstCharPtr AddStreamingConfigImpl::SerializeId()
{
    return "AddStreamingConfig";
}

ErrCode AddStreamingConfigImpl::Deserialize(ISerializedObject* serialized,
                                            IBaseObject* context,
                                            IFunction* factoryCallback,
                                            IBaseObject** obj)
{
    OPENDAQ_PARAM_NOT_NULL(serialized);
    OPENDAQ_PARAM_NOT_NULL(obj);

    return daqTry([&]
    {
        const auto createConfig =
            [&factoryCallback](const SerializedObjectPtr& serialized, const BaseObjectPtr& context, const StringPtr& /*className*/)
        {
            const StreamingTypePtr streamingType = serialized.readObject("streamingType", context, factoryCallback);
            return createWithImplementation<IAddStreamingConfig, AddStreamingConfigImpl>(streamingType, PassKey());
        };

        *obj = Super::DeserializePropertyObject(serialized, context, factoryCallback, createConfig).detach();
    });
}

ErrCode AddStreamingConfigImpl::serializeCustomValues(ISerializer* serializer, bool forUpdate)
{
    OPENDAQ_RETURN_IF_FAILED(Super::serializeCustomValues(serializer, forUpdate));
    if (forUpdate)
        return OPENDAQ_SUCCESS;

    return daqTry([&]
    {
        const auto serializerPtr = SerializerPtr::Borrow(serializer);
        serializerPtr.key("streamingType");
        streamingType.asPtr<ISerializable>().serialize(serializerPtr);
    });
}

void AddStreamingConfigImpl::setLinkedTypeConfig(const PropertyObjectPtr& typeConfig)
{
    if (objPtr.hasProperty(add_streaming_config::TypeConfig))
        objPtr.removeProperty(add_streaming_config::TypeConfig);

    linkedTypeConfig = typeConfig;
}

void AddStreamingConfigImpl::setSerializedWithDeviceConfig(bool withDeviceConfig)
{
    serializedWithDeviceConfig = withDeviceConfig;
}

#if !defined(BUILDING_STATIC_LIBRARY)

extern "C" ErrCode PUBLIC_EXPORT createAddStreamingConfig(IAddStreamingConfig** objTmp, IStreamingType* streamingType)
{
    return daq::createObject<IAddStreamingConfig, AddStreamingConfigImpl>(objTmp, streamingType);
}

#endif

OPENDAQ_REGISTER_DESERIALIZE_FACTORY(AddStreamingConfigImpl)

END_NAMESPACE_OPENDAQ
