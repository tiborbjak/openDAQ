/*
 * Copyright 2022-2026 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once
#include <coreobjects/property_object.h>
#include <coretypes/dictobject.h>
#include <opendaq/add_streaming_config.h>

BEGIN_NAMESPACE_OPENDAQ

/*!
 * @ingroup opendaq_add_device_config
 * @{
 */

/*!
 * @brief How nested devices of an added device stream.
 *
 * Disabled establishes no streaming.
 * MinHops serves every nested device that lists the selected Streaming type with its own streaming connection.
 * MinConnections streams every nested device through its parent's connection.
 */
enum class StreamingMode : EnumType
{
    Disabled = 0,
    MinHops,
    MinConnections
};

/*#
 * [templated(defaultAliasName: AddStreamingOptionsPtr)]
 * [interfaceSmartPtr(IAddStreamingOptions, GenericAddStreamingOptionsPtr)]
 * [interfaceLibrary(IPropertyObject, "coreobjects")]
 * [interfaceSmartPtr(IPropertyObject, GenericPropertyObjectPtr, "<coreobjects/property_object_ptr.h>", true)]
 */

/*!
 * @brief The streaming part of an add device config.
 *
 * Lists one streaming config per offered Streaming type as a child object keyed by the Streaming type ID.
 * The `StreamingType` selection holds the ID of the selected Streaming type, and is empty while none is selected.
 * The listed configs are created with the options and never removed or replaced.
 */
DECLARE_OPENDAQ_INTERFACE(IAddStreamingOptions, IPropertyObject)
{
    // [templateType(configs, IString, IAddStreamingConfig)]
    /*!
     * @brief Gets the listed streaming configs.
     * @param[out] configs The streaming configs keyed by Streaming type ID
     */
    virtual ErrCode INTERFACE_FUNC getStreamingConfigs(IDict** configs) = 0;

    /*!
     * @brief Selects the streaming config listed under a Streaming type ID.
     * @param typeId The Streaming type ID
     *
     * Null selects none.
     * Fails with OPENDAQ_ERR_NOTFOUND when no config is listed under the ID.
     */
    virtual ErrCode INTERFACE_FUNC setStreamingType(IString* typeId) = 0;

    /*!
     * @brief Gets the selected streaming config.
     * @param[out] config The selected streaming config, or null when none is selected
     */
    virtual ErrCode INTERFACE_FUNC getStreamingConfig(IAddStreamingConfig** config) = 0;

    /*!
     * @brief Gets the streaming mode.
     * @param[out] mode The streaming mode
     *
     * MinHops by default.
     */
    virtual ErrCode INTERFACE_FUNC getStreamingMode(StreamingMode* mode) = 0;

    /*!
     * @brief Sets the streaming mode.
     * @param mode The streaming mode
     */
    virtual ErrCode INTERFACE_FUNC setStreamingMode(StreamingMode mode) = 0;
};

/*!@}*/

END_NAMESPACE_OPENDAQ
