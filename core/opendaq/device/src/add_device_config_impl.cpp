#include <opendaq/add_device_config_impl.h>
#include <opendaq/add_streaming_config_impl.h>
#include <opendaq/add_streaming_options_impl.h>
#include <coreobjects/property_factory.h>
#include <coretypes/json_serializer_factory.h>
#include <coretypes/json_deserializer_factory.h>
#include <coretypes/serialized_list_ptr.h>

BEGIN_NAMESPACE_OPENDAQ

namespace add_device_config
{
static const char* DeviceType = "DeviceType";
static const char* TypeConfig = "TypeConfig";
static const char* PrimaryAddressType = "PrimaryAddressType";
static const char* ScanOnAdd = "ScanOnAdd";
static const char* Streaming = "Streaming";
}

namespace save_form
{
static const char* DeviceTypeId = "deviceTypeId";
static const char* TypeConfig = "typeConfig";
static const char* PrimaryAddressType = "primaryAddressType";
static const char* ScanOnAdd = "scanOnAdd";
static const char* Streaming = "streaming";
static const char* StreamingType = "streamingType";
static const char* StreamingMode = "streamingMode";
static const char* StreamingConfigs = "streamingConfigs";
static const char* StreamingTypeId = "streamingTypeId";
}

AddDeviceConfigImpl::AddDeviceConfigImpl(const DeviceTypePtr& deviceType,
                                         const DictPtr<IString, IStreamingType>& streamingTypes,
                                         Bool scanOnAdd)
    : AddDeviceConfigImpl(deviceType, PassKey())
{
    Super::addProperty(StringPropertyBuilder(add_device_config::DeviceType, deviceType.getId()).setReadOnly(true).build());
    Super::addProperty(ObjectProperty(add_device_config::TypeConfig, deviceType.createDefaultConfig()));
    Super::addProperty(
        SelectionProperty(add_device_config::PrimaryAddressType, List<IString>("IPv4", "IPv6"), static_cast<Int>(AddressType::IPv4)));
    Super::addProperty(BoolProperty(add_device_config::ScanOnAdd, scanOnAdd));

    if (deviceType.getDeviceKind() != DeviceKind::Mirrored)
        return;

    const PropertyObjectPtr typeConfig = objPtr.getPropertyValue(add_device_config::TypeConfig);
    auto streamingConfigs = Dict<IString, IAddStreamingConfig>();
    if (streamingTypes.assigned())
    {
        for (const auto& [typeId, streamingType] : streamingTypes)
        {
            const auto linkedTypeConfig = isLinked(streamingType) ? typeConfig : PropertyObjectPtr();
            streamingConfigs.set(typeId,
                                 createWithImplementation<IAddStreamingConfig, AddStreamingConfigImpl>(streamingType, linkedTypeConfig));
        }
    }

    const auto options = createWithImplementation<IAddStreamingOptions, AddStreamingOptionsImpl>(streamingConfigs);
    Super::addProperty(ObjectProperty(add_device_config::Streaming, options));
}

AddDeviceConfigImpl::AddDeviceConfigImpl(const DeviceTypePtr& deviceType, PassKey)
    : Super()
    , deviceType(deviceType)
{
}

ErrCode AddDeviceConfigImpl::getDeviceType(IDeviceType** type)
{
    OPENDAQ_PARAM_NOT_NULL(type);

    *type = deviceType.addRefAndReturn();
    return OPENDAQ_SUCCESS;
}

ErrCode AddDeviceConfigImpl::getTypeConfig(IPropertyObject** config)
{
    OPENDAQ_PARAM_NOT_NULL(config);

    return daqTry([&] { *config = objPtr.getPropertyValue(add_device_config::TypeConfig).asPtr<IPropertyObject>().detach(); });
}

ErrCode AddDeviceConfigImpl::getPrimaryAddressType(AddressType* type)
{
    OPENDAQ_PARAM_NOT_NULL(type);

    return daqTry([&]
    {
        const Int value = objPtr.getPropertyValue(add_device_config::PrimaryAddressType);
        *type = static_cast<AddressType>(value);
    });
}

ErrCode AddDeviceConfigImpl::setPrimaryAddressType(AddressType type)
{
    return Super::setPropertyValue(String(add_device_config::PrimaryAddressType), Integer(static_cast<Int>(type)));
}

ErrCode AddDeviceConfigImpl::getScanOnAdd(Bool* scanOnAdd)
{
    OPENDAQ_PARAM_NOT_NULL(scanOnAdd);

    return daqTry([&] { *scanOnAdd = static_cast<Bool>(objPtr.getPropertyValue(add_device_config::ScanOnAdd)); });
}

ErrCode AddDeviceConfigImpl::setScanOnAdd(Bool scanOnAdd)
{
    return Super::setPropertyValue(String(add_device_config::ScanOnAdd), Boolean(scanOnAdd));
}

ErrCode AddDeviceConfigImpl::getStreamingOptions(IAddStreamingOptions** options)
{
    OPENDAQ_PARAM_NOT_NULL(options);

    return daqTry([&]
    {
        if (deviceType.getDeviceKind() != DeviceKind::Mirrored)
        {
            *options = nullptr;
            return;
        }

        *options = objPtr.getPropertyValue(add_device_config::Streaming).asPtr<IAddStreamingOptions>().detach();
    });
}

ErrCode AddDeviceConfigImpl::getInterfaceIds(SizeT* idCount, IntfID** ids)
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

ErrCode AddDeviceConfigImpl::clone(IPropertyObject** cloned)
{
    OPENDAQ_PARAM_NOT_NULL(cloned);

    return daqTry([&]
    {
        auto lock = getRecursiveConfigLock2();
        auto obj = createWithImplementation<IAddDeviceConfig, AddDeviceConfigImpl>(deviceType, PassKey());
        const auto impl = dynamic_cast<AddDeviceConfigImpl*>(obj.getObject());
        impl->configureClonedMembers(getCloneParameters());
        impl->linkStreamingConfigs();
        *cloned = obj.detach();
    });
}

ErrCode AddDeviceConfigImpl::serialize(ISerializer* serializer)
{
    OPENDAQ_PARAM_NOT_NULL(serializer);

    return serializeWithStreamingConfigs([&] { return Super::serialize(serializer); });
}

ErrCode AddDeviceConfigImpl::serializeForUpdate(ISerializer* serializer)
{
    OPENDAQ_PARAM_NOT_NULL(serializer);

    return daqTry([&]
    {
        auto lock = getRecursiveConfigLock2();
        const auto self = borrowPtr<AddDeviceConfigPtr>();
        const auto serializerPtr = SerializerPtr::Borrow(serializer);

        serializerPtr.startTaggedObject(self);

        serializerPtr.key(save_form::DeviceTypeId);
        serializerPtr.writeString(deviceType.getId());
        serializerPtr.key(save_form::TypeConfig);
        self.getTypeConfig().asPtr<IUpdatable>().serializeForUpdate(serializerPtr);
        serializerPtr.key(save_form::PrimaryAddressType);
        serializerPtr.writeInt(static_cast<Int>(self.getPrimaryAddressType()));
        serializerPtr.key(save_form::ScanOnAdd);
        serializerPtr.writeBool(self.getScanOnAdd());

        if (const auto options = self.getStreamingOptions(); options.assigned())
        {
            serializerPtr.key(save_form::Streaming);
            serializerPtr.startObject();

            const auto selected = options.getStreamingConfig();
            serializerPtr.key(save_form::StreamingType);
            serializerPtr.writeString(selected.assigned() ? selected.getStreamingType().getId() : StringPtr(""));
            serializerPtr.key(save_form::StreamingMode);
            serializerPtr.writeInt(static_cast<Int>(options.getStreamingMode()));

            serializerPtr.key(save_form::StreamingConfigs);
            serializerPtr.startList();
            for (const auto& [typeId, streamingConfig] : options.getStreamingConfigs())
            {
                serializerPtr.startObject();
                serializerPtr.key(save_form::StreamingTypeId);
                serializerPtr.writeString(typeId);
                if (!isLinked(streamingConfig.getStreamingType()))
                {
                    serializerPtr.key(save_form::TypeConfig);
                    streamingConfig.getTypeConfig().asPtr<IUpdatable>().serializeForUpdate(serializerPtr);
                }
                serializerPtr.endObject();
            }
            serializerPtr.endList();

            serializerPtr.endObject();
        }

        serializerPtr.endObject();
    });
}

ErrCode AddDeviceConfigImpl::updateInternal(ISerializedObject* obj, IBaseObject* context)
{
    OPENDAQ_PARAM_NOT_NULL(obj);

    Bool isFrozenObj;
    OPENDAQ_RETURN_IF_FAILED(isFrozen(&isFrozenObj));
    if (isFrozenObj)
        return OPENDAQ_IGNORED;

    return daqTry([&]
    {
        const auto saved = SerializedObjectPtr::Borrow(obj);
        const auto self = borrowPtr<AddDeviceConfigPtr>();

        self.getTypeConfig().asPtr<IUpdatable>().updateInternal(saved.readSerializedObject(save_form::TypeConfig), context);
        self.setPrimaryAddressType(static_cast<AddressType>(saved.readInt(save_form::PrimaryAddressType)));
        self.setScanOnAdd(saved.readBool(save_form::ScanOnAdd));

        const auto options = self.getStreamingOptions();
        if (!options.assigned() || !saved.hasKey(save_form::Streaming))
            return;

        const auto savedOptions = saved.readSerializedObject(save_form::Streaming);
        const auto streamingConfigs = options.getStreamingConfigs();
        const StringPtr selected = savedOptions.readString(save_form::StreamingType);
        options.setStreamingType(streamingConfigs.hasKey(selected) ? selected : StringPtr(""));
        options.setStreamingMode(static_cast<StreamingMode>(savedOptions.readInt(save_form::StreamingMode)));

        const auto savedConfigs = savedOptions.readSerializedList(save_form::StreamingConfigs);
        for (SizeT i = 0; i < savedConfigs.getCount(); ++i)
        {
            const auto savedConfig = savedConfigs.readSerializedObject().asPtr<ISerializedObject, SerializedObjectPtr>();
            const StringPtr typeId = savedConfig.readString(save_form::StreamingTypeId);
            if (!streamingConfigs.hasKey(typeId) || !savedConfig.hasKey(save_form::TypeConfig))
                continue;

            const auto streamingConfig = streamingConfigs.get(typeId);
            if (isLinked(streamingConfig.getStreamingType()))
                continue;

            streamingConfig.getTypeConfig().asPtr<IUpdatable>().updateInternal(savedConfig.readSerializedObject(save_form::TypeConfig),
                                                                               context);
        }
    });
}

ErrCode AddDeviceConfigImpl::bind(IDict* deviceTypes, IDict* streamingTypes, IAddDeviceConfig** bound)
{
    OPENDAQ_PARAM_NOT_NULL(bound);

    return daqTry([&]
    {
        const auto serializer = JsonSerializer();
        checkErrorInfo(serializeForUpdate(serializer));

        AddDeviceConfigPtr config;
        JsonDeserializer().callCustomProc([&](const SerializedObjectPtr& saved) { config = Bind(saved, deviceTypes, streamingTypes); },
                                          serializer.getOutput());
        *bound = config.detach();
    });
}

ErrCode AddDeviceConfigImpl::getSerializeId(ConstCharPtr* id) const
{
    OPENDAQ_PARAM_NOT_NULL(id);

    *id = SerializeId();
    return OPENDAQ_SUCCESS;
}

ConstCharPtr AddDeviceConfigImpl::SerializeId()
{
    return "AddDeviceConfig";
}

ErrCode AddDeviceConfigImpl::Deserialize(ISerializedObject* serialized, IBaseObject* context, IFunction* factoryCallback, IBaseObject** obj)
{
    OPENDAQ_PARAM_NOT_NULL(serialized);
    OPENDAQ_PARAM_NOT_NULL(obj);

    return daqTry([&]
    {
        const auto createConfig =
            [&factoryCallback](const SerializedObjectPtr& serialized, const BaseObjectPtr& context, const StringPtr& /*className*/)
        {
            const DeviceTypePtr deviceType = serialized.readObject("deviceType", context, factoryCallback);
            return createWithImplementation<IAddDeviceConfig, AddDeviceConfigImpl>(deviceType, PassKey());
        };

        auto config = Super::DeserializePropertyObject(serialized, context, factoryCallback, createConfig);

        dynamic_cast<AddDeviceConfigImpl*>(config.getObject())->linkStreamingConfigs();
        *obj = config.detach();
    });
}

AddDeviceConfigPtr AddDeviceConfigImpl::Bind(const SerializedObjectPtr& saved,
                                             const DictPtr<IString, IDeviceType>& deviceTypes,
                                             const DictPtr<IString, IStreamingType>& streamingTypes)
{
    const StringPtr deviceTypeId = saved.readString(save_form::DeviceTypeId);
    if (!deviceTypes.assigned() || !deviceTypes.hasKey(deviceTypeId))
        DAQ_THROW_EXCEPTION(NotFoundException, "Device type \"{}\" is not available", deviceTypeId);

    try
    {
        const auto deviceType = deviceTypes.get(deviceTypeId);
        if (saved.hasKey(save_form::Streaming) != (deviceType.getDeviceKind() == DeviceKind::Mirrored))
            DAQ_THROW_EXCEPTION(InvalidValueException, "Streaming section does not match the kind of device type \"{}\"", deviceTypeId);

        auto boundStreamingTypes = Dict<IString, IStreamingType>();
        if (saved.hasKey(save_form::Streaming) && streamingTypes.assigned())
        {
            const auto savedConfigs = saved.readSerializedObject(save_form::Streaming).readSerializedList(save_form::StreamingConfigs);
            for (SizeT i = 0; i < savedConfigs.getCount(); ++i)
            {
                const auto savedConfig = savedConfigs.readSerializedObject().asPtr<ISerializedObject, SerializedObjectPtr>();
                const StringPtr typeId = savedConfig.readString(save_form::StreamingTypeId);
                if (streamingTypes.hasKey(typeId))
                    boundStreamingTypes.set(typeId, streamingTypes.get(typeId));
            }
        }

        auto config = createWithImplementation<IAddDeviceConfig, AddDeviceConfigImpl>(
            deviceType, boundStreamingTypes, saved.readBool(save_form::ScanOnAdd));
        config.asPtr<IUpdatable>().updateInternal(saved, nullptr);
        return config;
    }
    catch (const DaqException& e)
    {
        DAQ_THROW_EXCEPTION(InvalidValueException, "{}", e.what());
    }
}

ErrCode AddDeviceConfigImpl::serializeCustomValues(ISerializer* serializer, bool forUpdate)
{
    OPENDAQ_RETURN_IF_FAILED(Super::serializeCustomValues(serializer, forUpdate));

    return daqTry([&]
    {
        const auto serializerPtr = SerializerPtr::Borrow(serializer);
        serializerPtr.key("deviceType");
        deviceType.asPtr<ISerializable>().serialize(serializerPtr);
    });
}

bool AddDeviceConfigImpl::isLinked(const StreamingTypePtr& streamingType) const
{
    const DeviceTypePtr linkedDeviceType = streamingType.getLinkedDeviceType();
    return linkedDeviceType.assigned() && linkedDeviceType.getId() == deviceType.getId();
}

void AddDeviceConfigImpl::linkStreamingConfigs()
{
    const auto self = borrowPtr<AddDeviceConfigPtr>();
    const auto options = self.getStreamingOptions();
    if (!options.assigned())
        return;

    const auto typeConfig = self.getTypeConfig();
    for (const auto& [typeId, streamingConfig] : options.getStreamingConfigs())
    {
        if (isLinked(streamingConfig.getStreamingType()))
            dynamic_cast<AddStreamingConfigImpl*>(streamingConfig.getObject())->setLinkedTypeConfig(typeConfig);
    }
}

template <class F>
ErrCode AddDeviceConfigImpl::serializeWithStreamingConfigs(F&& serializeFunc)
{
    return daqTry([&]
    {
        auto lock = getRecursiveConfigLock2();

        std::vector<AddStreamingConfigPtr> streamingConfigs;
        if (const auto options = borrowPtr<AddDeviceConfigPtr>().getStreamingOptions(); options.assigned())
        {
            for (const auto& [typeId, streamingConfig] : options.getStreamingConfigs())
                streamingConfigs.push_back(streamingConfig);
        }

        for (const auto& streamingConfig : streamingConfigs)
            dynamic_cast<AddStreamingConfigImpl*>(streamingConfig.getObject())->setSerializedWithDeviceConfig(true);

        Finally clearSerializedWithDeviceConfig([&streamingConfigs]
        {
            for (const auto& streamingConfig : streamingConfigs)
                dynamic_cast<AddStreamingConfigImpl*>(streamingConfig.getObject())->setSerializedWithDeviceConfig(false);
        });

        checkErrorInfo(serializeFunc());
    });
}

#if !defined(BUILDING_STATIC_LIBRARY)

extern "C" ErrCode PUBLIC_EXPORT createAddDeviceConfig(IAddDeviceConfig** objTmp,
                                                       IDeviceType* deviceType,
                                                       IDict* streamingTypes,
                                                       Bool scanOnAdd)
{
    return daq::createObject<IAddDeviceConfig, AddDeviceConfigImpl>(objTmp, deviceType, streamingTypes, scanOnAdd);
}

extern "C" ErrCode PUBLIC_EXPORT createBoundAddDeviceConfig(IAddDeviceConfig** objTmp,
                                                            ISerializedObject* saved,
                                                            IDict* deviceTypes,
                                                            IDict* streamingTypes)
{
    OPENDAQ_PARAM_NOT_NULL(objTmp);
    OPENDAQ_PARAM_NOT_NULL(saved);

    return daqTry([&] { *objTmp = AddDeviceConfigImpl::Bind(saved, deviceTypes, streamingTypes).detach(); });
}

#endif

OPENDAQ_REGISTER_DESERIALIZE_FACTORY(AddDeviceConfigImpl)

END_NAMESPACE_OPENDAQ
