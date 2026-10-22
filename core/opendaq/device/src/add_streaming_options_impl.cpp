#include <opendaq/add_streaming_options_impl.h>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_factory.h>

BEGIN_NAMESPACE_OPENDAQ

namespace add_streaming_options
{
static const char* StreamingType = "StreamingType";
static const char* StreamingMode = "StreamingMode";
}

AddStreamingOptionsImpl::AddStreamingOptionsImpl(const DictPtr<IString, IAddStreamingConfig>& streamingConfigs)
    : Super()
{
    auto selectionValues = List<IString>("");
    for (const auto& [typeId, config] : streamingConfigs)
    {
        streamingTypeIds.push_back(typeId);
        selectionValues.pushBack(typeId);
    }

    Super::addProperty(StringPropertyBuilder(add_streaming_options::StreamingType, "").setSelectionValues(selectionValues).build());
    Super::addProperty(SelectionProperty(add_streaming_options::StreamingMode,
                                         List<IString>("Disabled", "MinHops", "MinConnections"),
                                         static_cast<Int>(StreamingMode::MinHops)));

    for (const auto& [typeId, config] : streamingConfigs)
        checkErrorInfo(Super::addProperty(ObjectProperty(typeId, config)));
}

AddStreamingOptionsImpl::AddStreamingOptionsImpl(const std::vector<StringPtr>& streamingTypeIds, PassKey)
    : Super()
    , streamingTypeIds(streamingTypeIds)
{
}

ErrCode AddStreamingOptionsImpl::getStreamingConfigs(IDict** configs)
{
    OPENDAQ_PARAM_NOT_NULL(configs);

    return daqTry([&]
    {
        auto dict = Dict<IString, IAddStreamingConfig>();
        for (const auto& typeId : streamingTypeIds)
            dict.set(typeId, objPtr.getPropertyValue(typeId).asPtr<IAddStreamingConfig>());

        *configs = dict.detach();
    });
}

ErrCode AddStreamingOptionsImpl::setStreamingType(IString* typeId)
{
    const StringPtr value = typeId ? StringPtr(typeId) : StringPtr("");
    return Super::setPropertyValue(String(add_streaming_options::StreamingType), value);
}

ErrCode AddStreamingOptionsImpl::getStreamingConfig(IAddStreamingConfig** config)
{
    OPENDAQ_PARAM_NOT_NULL(config);

    return daqTry([&]
    {
        const StringPtr typeId = objPtr.getPropertyValue(add_streaming_options::StreamingType);
        if (typeId.getLength() == 0)
        {
            *config = nullptr;
            return;
        }

        *config = objPtr.getPropertyValue(typeId).asPtr<IAddStreamingConfig>().detach();
    });
}

ErrCode AddStreamingOptionsImpl::getStreamingMode(StreamingMode* mode)
{
    OPENDAQ_PARAM_NOT_NULL(mode);

    return daqTry([&]
    {
        const Int value = objPtr.getPropertyValue(add_streaming_options::StreamingMode);
        *mode = static_cast<StreamingMode>(value);
    });
}

ErrCode AddStreamingOptionsImpl::setStreamingMode(StreamingMode mode)
{
    return Super::setPropertyValue(String(add_streaming_options::StreamingMode), Integer(static_cast<Int>(mode)));
}

ErrCode AddStreamingOptionsImpl::getInterfaceIds(SizeT* idCount, IntfID** ids)
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

ErrCode AddStreamingOptionsImpl::clone(IPropertyObject** cloned)
{
    OPENDAQ_PARAM_NOT_NULL(cloned);

    return daqTry([&]
    {
        auto lock = getRecursiveConfigLock2();
        auto obj = createWithImplementation<IAddStreamingOptions, AddStreamingOptionsImpl>(streamingTypeIds, PassKey());
        dynamic_cast<AddStreamingOptionsImpl*>(obj.getObject())->configureClonedMembers(getCloneParameters());
        *cloned = obj.detach();
    });
}

ErrCode AddStreamingOptionsImpl::getSerializeId(ConstCharPtr* id) const
{
    OPENDAQ_PARAM_NOT_NULL(id);

    *id = SerializeId();
    return OPENDAQ_SUCCESS;
}

ConstCharPtr AddStreamingOptionsImpl::SerializeId()
{
    return "AddStreamingOptions";
}

ErrCode AddStreamingOptionsImpl::Deserialize(ISerializedObject* serialized,
                                             IBaseObject* context,
                                             IFunction* factoryCallback,
                                             IBaseObject** obj)
{
    OPENDAQ_PARAM_NOT_NULL(serialized);
    OPENDAQ_PARAM_NOT_NULL(obj);

    return daqTry([&]
    {
        const auto createOptions = [](const SerializedObjectPtr&, const BaseObjectPtr&, const StringPtr&)
        {
            return createWithImplementation<IAddStreamingOptions, AddStreamingOptionsImpl>(std::vector<StringPtr>{}, PassKey());
        };

        auto options = Super::DeserializePropertyObject(serialized, context, factoryCallback, createOptions);

        auto& streamingTypeIds = dynamic_cast<AddStreamingOptionsImpl*>(options.getObject())->streamingTypeIds;
        const ListPtr<IString> selectionValues = options.getProperty(add_streaming_options::StreamingType).getSelectionValues();
        for (const auto& typeId : selectionValues)
        {
            if (typeId.getLength() > 0)
                streamingTypeIds.push_back(typeId);
        }

        *obj = options.detach();
    });
}

OPENDAQ_REGISTER_DESERIALIZE_FACTORY(AddStreamingOptionsImpl)

END_NAMESPACE_OPENDAQ
