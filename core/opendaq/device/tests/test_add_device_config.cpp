#include <gtest/gtest.h>
#include <opendaq/add_component_config_factory.h>
#include <opendaq/add_device_config_private_ptr.h>
#include <opendaq/component_type_builder_factory.h>
#include <coreobjects/property_object_factory.h>
#include <coreobjects/property_factory.h>
#include <coreobjects/property_object_internal_ptr.h>
#include <coretypes/json_serializer_factory.h>
#include <coretypes/json_deserializer_factory.h>
#include <coretypes/type_manager_factory.h>

BEGIN_NAMESPACE_OPENDAQ

class AddDeviceConfigTest : public testing::Test
{
protected:
    static DeviceTypePtr createDeviceType(DeviceKind kind, Int port = 7420)
    {
        auto defaultConfig = PropertyObject();
        defaultConfig.addProperty(IntProperty("Port", port));
        return DeviceTypeBuilder()
            .setId("dev")
            .setName("dev")
            .setConnectionStringPrefix("dev")
            .setDefaultConfig(defaultConfig)
            .setDeviceKind(kind)
            .build();
    }

    static StreamingTypePtr createStreamingType(const StringPtr& id,
                                                const StringPtr& linkedDeviceTypeId = "",
                                                const StringPtr& defaultId = nullptr)
    {
        auto defaultConfig = PropertyObject();
        defaultConfig.addProperty(StringProperty("Id", defaultId.assigned() ? defaultId : id));

        auto builder = StreamingTypeBuilder().setId(id).setName(id).setConnectionStringPrefix(id);
        if (linkedDeviceTypeId.getLength() == 0)
            return builder.setDefaultConfig(defaultConfig).build();

        const auto linkedDeviceType = DeviceTypeBuilder()
                                          .setId(linkedDeviceTypeId)
                                          .setConnectionStringPrefix(linkedDeviceTypeId)
                                          .setDefaultConfig(defaultConfig)
                                          .build();
        return builder.setLinkedDeviceType(linkedDeviceType).build();
    }

    static AddDeviceConfigPtr createLinkedConfig()
    {
        auto streamingTypes = Dict<IString, IStreamingType>();
        streamingTypes.set("other", createStreamingType("other", "otherDev"));
        streamingTypes.set("linked", createStreamingType("linked", "dev"));
        streamingTypes.set("linked2", createStreamingType("linked2", "dev"));
        return AddDeviceConfig(createDeviceType(DeviceKind::Mirrored), streamingTypes, true);
    }

    static AddDeviceConfigPtr createMirroredConfig()
    {
        auto streamingTypes = Dict<IString, IStreamingType>();
        streamingTypes.set("b", createStreamingType("b"));
        streamingTypes.set("a", createStreamingType("a"));
        return AddDeviceConfig(createDeviceType(DeviceKind::Mirrored), streamingTypes, true);
    }

    static std::string serialize(const PropertyObjectPtr& obj)
    {
        const auto serializer = JsonSerializer();
        obj.serialize(serializer);
        return serializer.getOutput();
    }

    static std::string serializeForUpdate(const PropertyObjectPtr& obj)
    {
        const auto serializer = JsonSerializer();
        obj.asPtr<IUpdatable>().serializeForUpdate(serializer);
        return serializer.getOutput();
    }

    static AddDeviceConfigPtr bindSaved(const std::string& json,
                                        const DictPtr<IString, IDeviceType>& deviceTypes,
                                        const DictPtr<IString, IStreamingType>& streamingTypes)
    {
        AddDeviceConfigPtr bound;
        JsonDeserializer().callCustomProc(
            [&](const SerializedObjectPtr& saved) { bound = BoundAddDeviceConfig(saved, deviceTypes, streamingTypes); }, json);
        return bound;
    }

    static BaseObjectPtr deserialize(const std::string& json, const BaseObjectPtr& context = nullptr)
    {
        return JsonDeserializer().deserialize(json, context, nullptr);
    }

    static void assertSameType(const StreamingTypePtr& actual, const StreamingTypePtr& expected)
    {
        ASSERT_EQ(actual, expected);
        ASSERT_EQ(actual.getLinkedDeviceType(), expected.getLinkedDeviceType());
    }

    static void assertLinkedShareTypeConfig(const AddDeviceConfigPtr& config)
    {
        const auto configs = config.getStreamingOptions().getStreamingConfigs();
        for (const auto& id : {"linked", "linked2"})
        {
            const auto linked = configs.get(id);
            ASSERT_FALSE(linked.hasProperty("TypeConfig"));
            ASSERT_EQ(linked.getTypeConfig().getObject(), config.getTypeConfig().getObject());
        }
    }
};

TEST_F(AddDeviceConfigTest, Defaults)
{
    const auto deviceType = createDeviceType(DeviceKind::External);
    const auto streamingTypes = Dict<IString, IStreamingType>({{"a", createStreamingType("a")}});
    const auto config = AddDeviceConfig(deviceType, streamingTypes, false);

    ASSERT_EQ(config.getDeviceType(), deviceType);
    ASSERT_EQ(config.getPropertyValue("DeviceType"), "dev");
    ASSERT_TRUE(config.getProperty("DeviceType").getReadOnly());
    ASSERT_EQ(config.getTypeConfig().getPropertyValue("Port"), 7420);
    ASSERT_EQ(config.getPrimaryAddressType(), AddressType::IPv4);
    ASSERT_FALSE(config.getScanOnAdd());
    ASSERT_EQ(config.getProperty("ScanOnAdd").getDefaultValue(), false);

    ASSERT_FALSE(config.hasProperty("Streaming"));
    ASSERT_FALSE(config.getStreamingOptions().assigned());
}

TEST_F(AddDeviceConfigTest, StreamingDefaults)
{
    const auto config = createMirroredConfig();
    const auto options = config.getStreamingOptions();

    ASSERT_EQ(options.getStreamingMode(), StreamingMode::MinHops);
    ASSERT_FALSE(options.getStreamingConfig().assigned());
    ASSERT_EQ(options.getPropertyValue("StreamingType"), "");
    ASSERT_EQ(options.getProperty("StreamingType").getSelectionValues(), List<IString>("", "b", "a"));

    const auto configs = options.getStreamingConfigs();
    ASSERT_EQ(configs.getKeyList(), List<IString>("b", "a"));
    ASSERT_EQ(configs.get("a").getStreamingType().getId(), "a");
    ASSERT_EQ(configs.get("a").getTypeConfig().getPropertyValue("Id"), "a");
}

TEST_F(AddDeviceConfigTest, MirroredWithoutStreamingTypes)
{
    const auto options = AddDeviceConfig(createDeviceType(DeviceKind::Mirrored), nullptr, true).getStreamingOptions();

    ASSERT_TRUE(options.assigned());
    ASSERT_EQ(options.getStreamingConfigs().getCount(), 0u);
    ASSERT_EQ(options.getProperty("StreamingType").getSelectionValues(), List<IString>(""));
    ASSERT_THROW(options.setStreamingType("a"), NotFoundException);
}

TEST_F(AddDeviceConfigTest, PropertyViewMatchesTypedGetters)
{
    const auto config = createMirroredConfig();
    const auto options = config.getStreamingOptions();
    const auto streamingConfig = options.getStreamingConfigs().get("a");

    ASSERT_EQ(config.getPropertyValue("TypeConfig").getObject(), config.getTypeConfig().getObject());
    ASSERT_EQ(config.getPropertyValue("Streaming").getObject(), options.getObject());
    ASSERT_EQ(options.getPropertyValue("a").getObject(), streamingConfig.getObject());
    ASSERT_EQ(streamingConfig.getPropertyValue("TypeConfig").getObject(), streamingConfig.getTypeConfig().getObject());

    config.setPrimaryAddressType(AddressType::IPv6);
    ASSERT_EQ(config.getPropertyValue("PrimaryAddressType"), static_cast<Int>(AddressType::IPv6));
    config.setPropertyValue("ScanOnAdd", false);
    ASSERT_FALSE(config.getScanOnAdd());
    options.setPropertyValue("StreamingMode", static_cast<Int>(StreamingMode::Disabled));
    ASSERT_EQ(options.getStreamingMode(), StreamingMode::Disabled);
}

TEST_F(AddDeviceConfigTest, DeserializedStreamingTypeWithReservedIdFails)
{
    const std::string json = R"({"__type":"StreamingType","id":"StreamingMode"})";
    auto streamingTypes = Dict<IString, IStreamingType>();
    streamingTypes.set("StreamingMode", JsonDeserializer().deserialize(json, nullptr, nullptr));

    ASSERT_THROW(AddDeviceConfig(createDeviceType(DeviceKind::Mirrored), streamingTypes, true), AlreadyExistsException);
}

TEST_F(AddDeviceConfigTest, StreamingSelection)
{
    const auto options = createMirroredConfig().getStreamingOptions();
    const auto configs = options.getStreamingConfigs();

    options.setStreamingType("a");
    ASSERT_EQ(options.getStreamingConfig().getObject(), configs.get("a").getObject());
    ASSERT_EQ(options.getPropertyValue("StreamingType"), "a");

    options.setPropertyValue("StreamingType", "b");
    ASSERT_EQ(options.getStreamingConfig().getObject(), configs.get("b").getObject());

    ASSERT_THROW(options.setStreamingType("unknown"), NotFoundException);
    ASSERT_EQ(options.getPropertyValue("StreamingType"), "b");

    options.setStreamingType(nullptr);
    ASSERT_FALSE(options.getStreamingConfig().assigned());
    ASSERT_EQ(options.getPropertyValue("StreamingType"), "");
}

TEST_F(AddDeviceConfigTest, Clone)
{
    const auto config = createLinkedConfig();
    config.getStreamingOptions().setStreamingType("other");

    const AddDeviceConfigPtr cloned = config.asPtr<IPropertyObjectInternal>().clone().asPtr<IAddDeviceConfig>();
    ASSERT_EQ(cloned.getDeviceType(), config.getDeviceType());
    assertLinkedShareTypeConfig(cloned);

    cloned.getTypeConfig().setPropertyValue("Port", 1);
    ASSERT_EQ(config.getTypeConfig().getPropertyValue("Port"), 7420);

    const auto options = cloned.getStreamingOptions();
    ASSERT_NE(options.getObject(), config.getStreamingOptions().getObject());
    ASSERT_EQ(options.getStreamingConfigs().getKeyList(), List<IString>("other", "linked", "linked2"));

    const auto streamingConfig = options.getStreamingConfig();
    ASSERT_EQ(streamingConfig.getStreamingType().getId(), "other");
    ASSERT_NE(streamingConfig.getObject(), config.getStreamingOptions().getStreamingConfig().getObject());

    streamingConfig.getTypeConfig().setPropertyValue("Id", "changed");
    ASSERT_EQ(config.getStreamingOptions().getStreamingConfig().getTypeConfig().getPropertyValue("Id"), "other");
}

TEST_F(AddDeviceConfigTest, LinkedStreamingConfigSharesTypeConfig)
{
    const auto config = createLinkedConfig();
    assertLinkedShareTypeConfig(config);

    const auto other = config.getStreamingOptions().getStreamingConfigs().get("other");
    ASSERT_TRUE(other.hasProperty("TypeConfig"));
    ASSERT_EQ(other.getTypeConfig().getPropertyValue("Id"), "other");
}

TEST_F(AddDeviceConfigTest, LinkedStreamingConfigCloneOwnsTypeConfig)
{
    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    const auto linked = config.getStreamingOptions().getStreamingConfigs().get("linked");

    const AddStreamingConfigPtr cloned = linked.asPtr<IPropertyObjectInternal>().clone().asPtr<IAddStreamingConfig>();
    ASSERT_TRUE(cloned.hasProperty("TypeConfig"));
    ASSERT_EQ(cloned.getTypeConfig().getPropertyValue("Port"), 1);

    cloned.getTypeConfig().setPropertyValue("Port", 2);
    ASSERT_EQ(config.getTypeConfig().getPropertyValue("Port"), 1);

    config.getTypeConfig().setPropertyValue("Port", 3);
    ASSERT_EQ(cloned.getTypeConfig().getPropertyValue("Port"), 2);
}

TEST_F(AddDeviceConfigTest, WireFormRoundTrip)
{
    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    config.setPrimaryAddressType(AddressType::IPv6);
    config.setScanOnAdd(false);
    const auto options = config.getStreamingOptions();
    options.setStreamingType("other");
    options.setStreamingMode(StreamingMode::MinConnections);
    options.getStreamingConfig().getTypeConfig().setPropertyValue("Id", "changed");

    const AddDeviceConfigPtr deserialized = deserialize(serialize(config), TypeManager()).asPtr<IAddDeviceConfig>();
    ASSERT_EQ(deserialized.getDeviceType(), config.getDeviceType());
    ASSERT_EQ(deserialized.getDeviceType().getDeviceKind(), DeviceKind::Mirrored);
    ASSERT_EQ(deserialized.getPropertyValue("DeviceType"), "dev");
    ASSERT_EQ(deserialized.getTypeConfig().getPropertyValue("Port"), 1);
    ASSERT_EQ(deserialized.getPrimaryAddressType(), AddressType::IPv6);
    ASSERT_FALSE(deserialized.getScanOnAdd());

    const auto deserializedOptions = deserialized.getStreamingOptions();
    ASSERT_EQ(deserializedOptions.getStreamingMode(), StreamingMode::MinConnections);
    ASSERT_EQ(deserializedOptions.getStreamingConfig().getStreamingType().getId(), "other");
    ASSERT_EQ(deserializedOptions.getStreamingConfig().getTypeConfig().getPropertyValue("Id"), "changed");
    ASSERT_EQ(deserializedOptions.getProperty("StreamingType").getSelectionValues(), List<IString>("", "other", "linked", "linked2"));

    const auto configs = options.getStreamingConfigs();
    const auto deserializedConfigs = deserializedOptions.getStreamingConfigs();
    ASSERT_EQ(deserializedConfigs.getKeyList(), configs.getKeyList());
    for (const auto& [id, streamingConfig] : deserializedConfigs)
        assertSameType(streamingConfig.getStreamingType(), configs.get(id).getStreamingType());
    assertLinkedShareTypeConfig(deserialized);

    deserialized.clearPropertyValue("TypeConfig");
    deserialized.clearPropertyValue("ScanOnAdd");
    ASSERT_EQ(deserializedConfigs.get("linked").getTypeConfig().getObject(), deserialized.getTypeConfig().getObject());
    ASSERT_EQ(deserialized.getTypeConfig().getPropertyValue("Port"), 7420);
    ASSERT_TRUE(deserialized.getScanOnAdd());
}

TEST_F(AddDeviceConfigTest, WireFormWritesNoTypeConfigForLinkedConfigs)
{
    const auto readStreamingConfigKeys =
        [](const StringPtr& typeId, const SerializedObjectPtr& serialized, const BaseObjectPtr&, const FunctionPtr&)
    {
        if (typeId != "AddDeviceConfig")
            return BaseObjectPtr();

        const auto options =
            serialized.readSerializedObject("propValues").readSerializedObject("Streaming").readSerializedObject("propValues");
        auto keys = Dict<IString, IList>();
        for (const auto& id : {"linked", "linked2", "other"})
            keys.set(id, options.readSerializedObject(id).getKeys());
        return BaseObjectPtr(keys);
    };

    const DictPtr<IString, IList> keys =
        JsonDeserializer().deserialize(serialize(createLinkedConfig()), nullptr, Function(readStreamingConfigKeys));
    for (const auto& id : {"linked", "linked2"})
        ASSERT_EQ(keys.get(id), List<IString>("__type", "streamingType"));

    ASSERT_EQ(keys.get("other"), List<IString>("__type", "streamingType", "propValues", "properties"));
}

TEST_F(AddDeviceConfigTest, WireFormWithoutDeviceType)
{
    ASSERT_THROW(deserialize(R"({"__type":"AddDeviceConfig"})"), NotFoundException);
}

TEST_F(AddDeviceConfigTest, WireFormRoundTripNotMirrored)
{
    const auto config = AddDeviceConfig(createDeviceType(DeviceKind::External), nullptr, false);
    config.getTypeConfig().setPropertyValue("Port", 1);

    const AddDeviceConfigPtr deserialized = deserialize(serialize(config)).asPtr<IAddDeviceConfig>();
    ASSERT_EQ(deserialized.getDeviceType(), config.getDeviceType());
    ASSERT_EQ(deserialized.getDeviceType().getDeviceKind(), DeviceKind::External);
    ASSERT_EQ(deserialized.getTypeConfig().getPropertyValue("Port"), 1);
    ASSERT_FALSE(deserialized.getScanOnAdd());
    ASSERT_FALSE(deserialized.hasProperty("Streaming"));
    ASSERT_FALSE(deserialized.getStreamingOptions().assigned());
}

TEST_F(AddDeviceConfigTest, StandaloneStreamingConfig)
{
    const auto streamingType = createStreamingType("a", "dev");
    const auto standalone = AddStreamingConfig(streamingType);
    ASSERT_EQ(standalone.getStreamingType(), streamingType);
    ASSERT_EQ(standalone.getTypeConfig().getPropertyValue("Id"), "a");
    ASSERT_EQ(standalone.getPropertyValue("TypeConfig").getObject(), standalone.getTypeConfig().getObject());
    standalone.getTypeConfig().setPropertyValue("Id", "changed");

    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    serialize(config);
    const auto linked = config.getStreamingOptions().getStreamingConfigs().get("linked");

    for (const auto& [streamingConfig, property, value] :
         {std::make_tuple(standalone, "Id", BaseObjectPtr("changed")), std::make_tuple(linked, "Port", BaseObjectPtr(1))})
    {
        const AddStreamingConfigPtr deserialized = deserialize(serialize(streamingConfig)).asPtr<IAddStreamingConfig>();
        assertSameType(deserialized.getStreamingType(), streamingConfig.getStreamingType());
        ASSERT_TRUE(deserialized.hasProperty("TypeConfig"));
        ASSERT_EQ(deserialized.getTypeConfig().getPropertyValue(property), value);
    }

    assertLinkedShareTypeConfig(config);
}

TEST_F(AddDeviceConfigTest, StreamingOptionsWireFormRoundTrip)
{
    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);

    const AddStreamingOptionsPtr deserialized = deserialize(serialize(config.getStreamingOptions())).asPtr<IAddStreamingOptions>();
    ASSERT_FALSE(deserialized.getStreamingConfig().assigned());
    const auto configs = deserialized.getStreamingConfigs();
    ASSERT_EQ(configs.getKeyList(), List<IString>("other", "linked", "linked2"));
    for (const auto& id : {"linked", "linked2"})
    {
        const auto linked = configs.get(id);
        ASSERT_TRUE(linked.hasProperty("TypeConfig"));
        ASSERT_EQ(linked.getTypeConfig().getPropertyValue("Port"), 1);
    }

    ASSERT_NE(configs.get("linked").getTypeConfig().getObject(), configs.get("linked2").getTypeConfig().getObject());
    ASSERT_EQ(configs.get("other").getTypeConfig().getPropertyValue("Id"), "other");
}

TEST_F(AddDeviceConfigTest, SaveForm)
{
    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    config.getStreamingOptions().getStreamingConfigs().get("other").getTypeConfig().setPropertyValue("Id", "changed");

    ASSERT_EQ(serializeForUpdate(config),
              R"({"__type":"AddDeviceConfig","deviceTypeId":"dev","typeConfig":{"__type":"PropertyObject","propValues":{"Port":1}},)"
              R"("primaryAddressType":0,"scanOnAdd":true,"streaming":{"streamingType":"","streamingMode":1,"streamingConfigs":[)"
              R"({"streamingTypeId":"other","typeConfig":{"__type":"PropertyObject","propValues":{"Id":"changed"}}},)"
              R"({"streamingTypeId":"linked"},{"streamingTypeId":"linked2"}]}})");
}

TEST_F(AddDeviceConfigTest, SaveFormIsNotReadable)
{
    ASSERT_THROW(deserialize(serializeForUpdate(createMirroredConfig())), NotFoundException);
}

TEST_F(AddDeviceConfigTest, BindSaveForm)
{
    const auto config = createMirroredConfig();
    config.setPrimaryAddressType(AddressType::IPv6);
    config.setScanOnAdd(false);
    const auto options = config.getStreamingOptions();
    options.setStreamingType("a");
    options.setStreamingMode(StreamingMode::MinConnections);
    options.getStreamingConfig().getTypeConfig().setPropertyValue("Id", "changed");

    const auto deviceType = createDeviceType(DeviceKind::Mirrored, 7000);
    auto streamingTypes = Dict<IString, IStreamingType>();
    streamingTypes.set("a", createStreamingType("a"));
    streamingTypes.set("b", createStreamingType("b", "", "newDefault"));

    const auto bound = bindSaved(serializeForUpdate(config), Dict<IString, IDeviceType>({{"dev", deviceType}}), streamingTypes);
    ASSERT_EQ(bound.getDeviceType().getObject(), deviceType.getObject());
    ASSERT_EQ(bound.getTypeConfig().getPropertyValue("Port"), 7000);
    ASSERT_EQ(bound.getPrimaryAddressType(), AddressType::IPv6);
    ASSERT_FALSE(bound.getScanOnAdd());
    ASSERT_EQ(bound.getProperty("ScanOnAdd").getDefaultValue(), false);

    const auto boundOptions = bound.getStreamingOptions();
    ASSERT_EQ(boundOptions.getStreamingMode(), StreamingMode::MinConnections);
    ASSERT_EQ(boundOptions.getStreamingConfigs().getKeyList(), List<IString>("b", "a"));
    ASSERT_EQ(boundOptions.getStreamingConfig().getStreamingType().getObject(), streamingTypes.get("a").getObject());
    ASSERT_EQ(boundOptions.getStreamingConfig().getTypeConfig().getPropertyValue("Id"), "changed");
    ASSERT_EQ(boundOptions.getStreamingConfigs().get("b").getTypeConfig().getPropertyValue("Id"), "newDefault");
}

TEST_F(AddDeviceConfigTest, BindWireForm)
{
    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    config.getStreamingOptions().setStreamingType("linked");

    const auto deviceType = createDeviceType(DeviceKind::Mirrored);
    auto streamingTypes = Dict<IString, IStreamingType>();
    for (const auto& id : {"linked", "linked2"})
        streamingTypes.set(id, createStreamingType(id, "dev"));
    streamingTypes.set("other", createStreamingType("other", "otherDev"));

    const AddDeviceConfigPtr deserialized = deserialize(serialize(config)).asPtr<IAddDeviceConfig>();
    const auto bound =
        deserialized.asPtr<IAddDeviceConfigPrivate>().bind(Dict<IString, IDeviceType>({{"dev", deviceType}}), streamingTypes);
    ASSERT_EQ(bound.getDeviceType().getObject(), deviceType.getObject());
    ASSERT_EQ(bound.getTypeConfig().getPropertyValue("Port"), 1);

    const auto boundOptions = bound.getStreamingOptions();
    ASSERT_EQ(boundOptions.getStreamingConfig().getStreamingType().getObject(), streamingTypes.get("linked").getObject());
    assertLinkedShareTypeConfig(bound);
}

TEST_F(AddDeviceConfigTest, BindUnknownDeviceType)
{
    const auto config = createMirroredConfig();
    const auto deviceTypes = Dict<IString, IDeviceType>({{"otherDev", createDeviceType(DeviceKind::Mirrored)}});

    ASSERT_THROW(config.asPtr<IAddDeviceConfigPrivate>().bind(deviceTypes, nullptr), NotFoundException);
    ASSERT_THROW(bindSaved(serializeForUpdate(config), deviceTypes, nullptr), NotFoundException);
}

TEST_F(AddDeviceConfigTest, BindDeviceKindMismatch)
{
    const auto mirrored = Dict<IString, IDeviceType>({{"dev", createDeviceType(DeviceKind::Mirrored)}});
    const auto external = Dict<IString, IDeviceType>({{"dev", createDeviceType(DeviceKind::External)}});
    const auto externalConfig = AddDeviceConfig(createDeviceType(DeviceKind::External), nullptr, true);

    ASSERT_THROW(bindSaved(serializeForUpdate(createMirroredConfig()), external, nullptr), InvalidValueException);
    ASSERT_THROW(bindSaved(serializeForUpdate(externalConfig), mirrored, nullptr), InvalidValueException);
}

TEST_F(AddDeviceConfigTest, BindMalformedSaveForm)
{
    const auto deviceTypes = Dict<IString, IDeviceType>({{"dev", createDeviceType(DeviceKind::Mirrored)}});
    const auto valid = serializeForUpdate(createMirroredConfig());

    const auto replaced = [&valid](const std::string& from, const std::string& to)
    {
        auto json = valid;
        const auto pos = json.find(from);
        EXPECT_NE(pos, std::string::npos);
        return json.replace(pos, from.size(), to);
    };

    ASSERT_NO_THROW(bindSaved(valid, deviceTypes, nullptr));
    ASSERT_THROW(bindSaved(replaced(R"("scanOnAdd":true)", R"("scanOnAdd":"yes")"), deviceTypes, nullptr), InvalidValueException);
    ASSERT_THROW(bindSaved(replaced(R"("scanOnAdd":true,)", ""), deviceTypes, nullptr), InvalidValueException);
    ASSERT_THROW(bindSaved(replaced(R"("streamingMode":1)", R"("streamingMode":99)"), deviceTypes, nullptr), InvalidValueException);
}

TEST_F(AddDeviceConfigTest, BindDropsUnknownStreamingType)
{
    const auto config = createMirroredConfig();
    config.getStreamingOptions().setStreamingType("a");

    const auto deviceTypes = Dict<IString, IDeviceType>({{"dev", createDeviceType(DeviceKind::Mirrored)}});
    const auto streamingTypes = Dict<IString, IStreamingType>({{"b", createStreamingType("b")}, {"c", createStreamingType("c")}});

    const auto options = config.asPtr<IAddDeviceConfigPrivate>().bind(deviceTypes, streamingTypes).getStreamingOptions();
    ASSERT_EQ(options.getStreamingConfigs().getKeyList(), List<IString>("b"));
    ASSERT_EQ(options.getProperty("StreamingType").getSelectionValues(), List<IString>("", "b"));
    ASSERT_FALSE(options.getStreamingConfig().assigned());
}

TEST_F(AddDeviceConfigTest, BindFollowsTargetLinks)
{
    const auto config = createLinkedConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    config.getStreamingOptions().getStreamingConfigs().get("other").getTypeConfig().setPropertyValue("Id", "changed");

    auto streamingTypes = Dict<IString, IStreamingType>();
    streamingTypes.set("linked", createStreamingType("linked"));
    streamingTypes.set("other", createStreamingType("other", "dev"));

    const auto bound = config.asPtr<IAddDeviceConfigPrivate>().bind(
        Dict<IString, IDeviceType>({{"dev", createDeviceType(DeviceKind::Mirrored)}}), streamingTypes);
    const auto configs = bound.getStreamingOptions().getStreamingConfigs();
    ASSERT_EQ(configs.get("linked").getTypeConfig().getPropertyValue("Id"), "linked");
    ASSERT_EQ(configs.get("other").getTypeConfig().getObject(), bound.getTypeConfig().getObject());
    ASSERT_EQ(configs.get("other").getTypeConfig().getPropertyValue("Port"), 1);
}

TEST_F(AddDeviceConfigTest, UpdateFromSaveForm)
{
    const auto saved = createMirroredConfig();
    saved.getStreamingOptions().setStreamingType("a");
    saved.getStreamingOptions().setStreamingMode(StreamingMode::Disabled);

    const auto config = createMirroredConfig();
    config.getTypeConfig().setPropertyValue("Port", 1);
    config.getStreamingOptions().getStreamingConfigs().get("b").getTypeConfig().setPropertyValue("Id", "changed");

    JsonDeserializer().update(config, serializeForUpdate(saved));
    ASSERT_EQ(config.getTypeConfig().getPropertyValue("Port"), 7420);
    ASSERT_EQ(config.getStreamingOptions().getStreamingConfigs().get("b").getTypeConfig().getPropertyValue("Id"), "b");
    ASSERT_EQ(config.getStreamingOptions().getStreamingConfig().getStreamingType().getId(), "a");
    ASSERT_EQ(config.getStreamingOptions().getStreamingMode(), StreamingMode::Disabled);
}

TEST_F(AddDeviceConfigTest, FrozenIgnoresUpdate)
{
    const auto saved = createMirroredConfig();
    saved.setScanOnAdd(false);

    const auto config = createMirroredConfig();
    config.freeze();

    JsonDeserializer().callCustomProc([&](const SerializedObjectPtr& obj) { config.asPtr<IUpdatable>().updateInternal(obj, nullptr); },
                                      serializeForUpdate(saved));
    ASSERT_TRUE(config.getScanOnAdd());
}

END_NAMESPACE_OPENDAQ
