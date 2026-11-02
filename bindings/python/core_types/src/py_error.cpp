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

#include "py_core_types/py_error.h"

#include <cstdint>

#include <pybind11/eval.h>

#include <coretypes/errors.h>
#include <coreobjects/errors.h>
#include <opendaq/errors.h>
#include <opendaq/module_manager_errors.h>
#include <opendaq/function_block_errors.h>
#include <opendaq/reader_errors.h>
#include <opendaq/scheduler_errors.h>
#include <opendaq/utility_errors.h>
#include <opendaq/device_errors.h>
#include <opendaq/component_errors.h>
#include <opendaq/signal_errors.h>
#include <opendaq/logger_errors.h>

// Mirrors config_protocol/errors.h, which the bindings don't link.
#ifndef OPENDAQ_ERRTYPE_NATIVE
#define OPENDAQ_ERRTYPE_NATIVE 0x10u
#endif

#ifndef OPENDAQ_ERR_NATIVE_CLIENT_CALL_NOT_AVAILABLE
#define OPENDAQ_ERR_NATIVE_CLIENT_CALL_NOT_AVAILABLE OPENDAQ_ERROR_CODE(OPENDAQ_ERRTYPE_NATIVE, 0x001)
#endif

namespace
{

struct DaqErrorCategory
{
    uint32_t type;
    const char* name;
    const char* doc;
};

constexpr DaqErrorCategory errorCategories[] = {
    {OPENDAQ_ERRTYPE_OPENDAQ, "OpenDaqError", "Base class for openDAQ errors."},
    {OPENDAQ_ERRTYPE_FUCTION_BLOCK, "FunctionBlockError", "Base class for function block errors."},
    {OPENDAQ_ERRTYPE_MODULE_MANAGER, "ModuleManagerError", "Base class for module manager errors."},
    {OPENDAQ_ERRTYPE_SCHEDULER, "SchedulerError", "Base class for scheduler errors."},
    {OPENDAQ_ERRTYPE_COREOBJECTS, "CoreObjectsError", "Base class for core objects errors."},
    {OPENDAQ_ERRTYPE_LOGGER, "LoggerError", "Base class for logger errors."},
    {OPENDAQ_ERRTYPE_DEVICE, "DeviceError", "Base class for device errors."},
    {OPENDAQ_ERRTYPE_SIGNAL, "SignalError", "Base class for signal errors."},
    {OPENDAQ_ERRTYPE_UTILITY, "UtilityError", "Base class for utility errors."},
    {OPENDAQ_ERRTYPE_READER, "ReaderError", "Base class for reader errors."},
    {OPENDAQ_ERRTYPE_COMPONENT, "ComponentError", "Base class for component errors."},
    {OPENDAQ_ERRTYPE_NATIVE, "NativeError", "Base class for native configuration protocol errors."},
};

struct DaqErrorCodeInfo
{
    daq::ErrCode code;
    const char* macro;
    const char* name;
    const char* doc;
};

#define DAQ_PY_ERROR(code, cls, doc) {code, #code, #cls, doc}

constexpr DaqErrorCodeInfo errorCodes[] = {
    DAQ_PY_ERROR(OPENDAQ_ERR_NOMEMORY, NoMemoryError, "No memory"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALIDPARAMETER, InvalidParameterError, "Invalid parameter"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOINTERFACE, NoInterfaceError, "Invalid cast. The object does not implement this interface."),
    DAQ_PY_ERROR(OPENDAQ_ERR_SIZETOOSMALL, SizeTooSmallError, "Size too small"),
    DAQ_PY_ERROR(OPENDAQ_ERR_CONVERSIONFAILED, ConversionFailedError, "Conversion failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_OUTOFRANGE, OutOfRangeError, "Out of range"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOTFOUND, NotFoundError, "Not found"),
    DAQ_PY_ERROR(OPENDAQ_ERR_ALREADYEXISTS, AlreadyExistsError, "Already exists"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOTASSIGNED, NotAssignedError, "Not assigned"),
    DAQ_PY_ERROR(OPENDAQ_ERR_CALLFAILED, CallFailedError, "Call failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_PARSEFAILED, ParseFailedError, "Parse failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALIDVALUE, InvalidValueError, "Invalid value"),
    DAQ_PY_ERROR(OPENDAQ_ERR_RESOLVEFAILED, ResolveFailedError, "Resolve failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALIDTYPE, InvalidTypeError, "Invalid type"),
    DAQ_PY_ERROR(OPENDAQ_ERR_ACCESSDENIED, AccessDeniedError, "Access denied"),
    DAQ_PY_ERROR(OPENDAQ_ERR_DEVICE_LOCKED, DeviceLockedError, "Device locked"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOTENABLED, NotEnabledError, "Not enabled"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOTIMPLEMENTED, NotImplementedDaqError, "Not Implemented"),
    DAQ_PY_ERROR(OPENDAQ_ERR_FROZEN, FrozenError, "Object frozen"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOT_FROZEN, NotFrozenError, "Object is not frozen"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOT_SERIALIZABLE, NotSerializableError, "Not serializable"),
    DAQ_PY_ERROR(OPENDAQ_ERR_DESERIALIZE_PARSE_ERROR, DeserializeError, "Error when parsing or deserializing"),
    DAQ_PY_ERROR(OPENDAQ_ERR_DESERIALIZE_UNKNOWN_TYPE, DeserializeUnknownTypeError, "Unknown format of Serialize type id"),
    DAQ_PY_ERROR(OPENDAQ_ERR_DESERIALIZE_NO_TYPE, DeserializeNoTypeError, "Serialize type id not found"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALIDPROPERTY, InvalidPropertyError, "Invalid property"),
    DAQ_PY_ERROR(OPENDAQ_ERR_DUPLICATEITEM, DuplicateItemError, "Duplicate item"),
    DAQ_PY_ERROR(OPENDAQ_ERR_ARGUMENT_NULL, ArgumentNullError, "Argument must not be NULL."),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALID_OPERATION, InvalidOperationError, "Operation is not valid for the current type or state."),
    DAQ_PY_ERROR(OPENDAQ_ERR_UNINITIALIZED, UninitializedError, "The operation requires initialization"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALIDSTATE, InvalidStateError, "Invalid state"),
    DAQ_PY_ERROR(OPENDAQ_ERR_VALIDATE_FAILED, ValidateFailedError, "Validate failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOT_UPDATABLE, NotUpdatableError, "Not updatable"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NO_COMPATIBLE_VERSION, NotCompatibleVersionError, "Not compatible version"),
    DAQ_PY_ERROR(OPENDAQ_ERR_LOCKED, LockedError, "Locked"),
    DAQ_PY_ERROR(OPENDAQ_ERR_SIZETOOLARGE, SizeTooLargeError, "Size too large"),
    DAQ_PY_ERROR(OPENDAQ_ERR_BUFFERFULL, BufferFullError, "Buffer full"),
    DAQ_PY_ERROR(OPENDAQ_ERR_EMPTY_SCALING_TABLE, EmptyScalingTableError, "Scaling table must not be empty"),
    DAQ_PY_ERROR(OPENDAQ_ERR_EMPTY_RANGE, EmptyRangeError, "Scaling range must not be empty"),
    DAQ_PY_ERROR(OPENDAQ_ERR_CREATE_FAILED, CreateFailedError, "Failed to create object"),
    DAQ_PY_ERROR(OPENDAQ_ERR_GENERALERROR, GeneralError, "General error"),
    DAQ_PY_ERROR(OPENDAQ_ERR_DISCOVERY_FAILED, DiscoveryFailedError, "Device discovery failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_COERCE_FAILED, CoerceFailedError, "Coercing failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOT_SUPPORTED, NotSupportedError, "The operation or type is not supported"),
    DAQ_PY_ERROR(OPENDAQ_ERR_LIST_NOT_HOMOGENEOUS, ListNotHomogeneousError, "List is not homogeneous"),
    DAQ_PY_ERROR(OPENDAQ_ERR_FACTORY_NOT_REGISTERED, FactoryNotRegisteredError, "Factory not registered"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NO_DATA, NoDataError, "No data"),
    DAQ_PY_ERROR(OPENDAQ_ERR_RESERVED_TYPE_NAME, ReservedTypeNameError, "Type name is reserved and can not be used"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOT_CLONEABLE, NotCloneableError, "Not cloneable"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALID_ARGUMENT, InvalidArgumentError, "Invalid argument."),
    DAQ_PY_ERROR(OPENDAQ_ERR_NO_TYPE_MANAGER, NoTypeManagerError, "Type manager is not available."),

    DAQ_PY_ERROR(OPENDAQ_ERR_CALCFAILED, CalcFailedError, "Calculation failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_MANAGER_NOT_ASSIGNED, ManagerNotAssignedError, "Property object class manager is not assigned"),
    DAQ_PY_ERROR(OPENDAQ_ERR_AUTHENTICATION_FAILED, AuthenticationFailedError, "Authentication failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NO_OWNER, NoOwnerError, "Object has no owner."),
    DAQ_PY_ERROR(OPENDAQ_ERR_OWNER_EXPIRED, OwnerExpiredError, "Owner of the object no longer exists."),

    DAQ_PY_ERROR(OPENDAQ_ERR_CONNECTION_LOST, ConnectionLostError, "Lost connection to the server."),
    DAQ_PY_ERROR(OPENDAQ_ERR_CONNECTION_LIMIT_REACHED, ConnectionLimitReachedError, "Connection rejected - connections limit reached"),
    DAQ_PY_ERROR(OPENDAQ_ERR_SERVER_VERSION_TOO_LOW, ServerVersionTooLowError, "The client attempted to call a function that requires a newer version of the openDAQ server"),
    DAQ_PY_ERROR(OPENDAQ_ERR_CONTROL_CLIENT_REJECTED, ControlClientRejectedError, "Connection rejected - too many control clients"),
    DAQ_PY_ERROR(OPENDAQ_ERR_CYCLIC_REFERENCE, CyclicReferenceError, "Signal connection rejected - cyclic reference detected"),

    DAQ_PY_ERROR(OPENDAQ_ERR_MODULE_MANAGER_UNKNOWN, ModuleManagerUnknownError, "Unknown module-manager exception"),
    DAQ_PY_ERROR(OPENDAQ_ERR_MODULE_LOAD_FAILED, ModuleLoadFailedError, "Module failed to load"),
    DAQ_PY_ERROR(OPENDAQ_ERR_MODULE_NO_ENTRY_POINT, ModuleNoEntryPointError, "Module has no entry-point function to call"),
    DAQ_PY_ERROR(OPENDAQ_ERR_MODULE_ENTRY_POINT_FAILED, ModuleEntryPointFailedError, "Module entry-point function call failed"),
    DAQ_PY_ERROR(OPENDAQ_ERR_MODULE_INCOMPATIBLE_DEPENDENCIES, ModuleIncompatibleDependenciesError, "Module has incompatible dependencies"),

    DAQ_PY_ERROR(OPENDAQ_ERR_SCHEDULER_UNKNOWN, SchedulerUnknownError, "Unknown scheduler exception"),
    DAQ_PY_ERROR(OPENDAQ_ERR_SCHEDULER_STOPPED, SchedulerStoppedError, "Scheduler already stopped"),
    DAQ_PY_ERROR(OPENDAQ_ERR_NOT_ENOUGH_TASKS, NotEnoughTasksError, "Not enough tasks provided for this operation"),
    DAQ_PY_ERROR(OPENDAQ_ERR_EMPTY_AWAITABLE, EmptyAwaitableError, "Awaitable has no state"),

    DAQ_PY_ERROR(OPENDAQ_ERR_READER_UNKNOWN, ReaderUnknownError, "Unknown reader exception"),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALID_DATA, InvalidDataError, "Samples can't be read or converted to the wanted type"),

    DAQ_PY_ERROR(OPENDAQ_ERR_COMPONENT_REMOVED, ComponentRemovedError, "The operation failed because component is removed"),

    DAQ_PY_ERROR(OPENDAQ_ERR_RANGE_BOUNDARIES_INVALID, RangeBoundariesInvalidError, "The low and high boundaries of the range are invalid."),
    DAQ_PY_ERROR(OPENDAQ_ERR_DIMENSION_IMPLICIT, DimensionImplicitError, "Operation is invalid on implicit dimensions."),
    DAQ_PY_ERROR(OPENDAQ_ERR_DIMENSION_EXPLICIT, DimensionExplicitError, "Operation is invalid on explicit dimensions."),
    DAQ_PY_ERROR(OPENDAQ_ERR_CONFIGURATION_INCOMPLETE, ConfigurationIncompleteError, "Call not allowed until configuration is complete."),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALID_DIMENSION_LABEL_TYPES, InvalidLabelTypesError, "Dimension explicit labels are of an invalid type."),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALID_PARAMETERS, InvalidParametersError, "Parameters of rule/scaling are not valid."),
    DAQ_PY_ERROR(OPENDAQ_ERR_UNKNOWN_RULE_TYPE, UnknownRuleTypeError, "Rule is custom and cannot be interpreted by openDAQ."),
    DAQ_PY_ERROR(OPENDAQ_ERR_INVALID_SAMPLE_TYPE, InvalidSampleTypeError, "Provided sample type is not supported."),
    DAQ_PY_ERROR(OPENDAQ_ERR_SIGNAL_NOT_ACCEPTED, SignalNotAcceptedError, "Input port does not accept the provided signal."),
    DAQ_PY_ERROR(OPENDAQ_ERR_PACKET_MEMORY_ALLOCATION, MemoryAllocationFailedError, "Packet data buffer memory allocation failed."),
    DAQ_PY_ERROR(OPENDAQ_ERR_PACKET_MEMORY_DEALLOCATION, MemoryDeallocationFailedError, "Packet data buffer memory deallocation failed."),

    DAQ_PY_ERROR(OPENDAQ_ERR_NATIVE_CLIENT_CALL_NOT_AVAILABLE, NativeClientCallNotAvailableError, "This function call is not available/implemented for usage on connected-to Native servers."),
};

#undef DAQ_PY_ERROR

}

void registerDaqErrors(pybind11::module_ m)
{
    auto errors = m.def_submodule("errors");

    pybind11::module_::import("sys").attr("modules")[errors.attr("__name__")] = errors;

    pybind11::exec(R"py(
import operator as _operator

_by_code = {}
_by_type = {}
_categories = set()

def _describe(code):
    return f"{code.name} ({code:#010x})" if isinstance(code, ErrorCode) else f"{code:#010x}"

def _check_code(code):
    try:
        code = _operator.index(code)
    except TypeError:
        raise TypeError(f"code must be an int, not {type(code).__name__}") from None
    if not 0x80000000 <= code <= 0xFFFFFFFF:
        raise ValueError(f"{code:#x} is not an openDAQ error code")
    try:
        return ErrorCode(code)
    except ValueError:
        return code

def _lookup(code):
    cls = _by_code.get(code)
    if cls is None:
        cls = _by_type.get((code >> 16) & 0xFF, DaqError)
    return cls

class DaqError(RuntimeError):
    """Base class for openDAQ errors. `code` holds the openDAQ error code."""

    __module__ = "opendaq.errors"
    code = None

    def __new__(cls, message="", code=None):
        if code is None:
            if cls in _categories:
                raise TypeError(f"{cls.__name__}() requires a code")
            self = super().__new__(cls, message)
            self.code = cls.code
            return self
        if cls not in _categories:
            raise TypeError(f"{cls.__name__}() does not take a code")
        code = _check_code(code)
        target = _lookup(code)
        if not issubclass(target, cls):
            raise ValueError(f"{_describe(code)} is not a {cls.__name__} code")
        self = super().__new__(target, message)
        self.code = code
        return self

    def __init__(self, message="", code=None):
        super().__init__(message)
)py", errors.attr("__dict__"));

    pybind11::list members;
    for (const auto& info : errorCodes)
        members.append(pybind11::make_tuple(info.macro + sizeof("OPENDAQ_ERR_") - 1, pybind11::int_(info.code)));
    pybind11::object errorCode = pybind11::module_::import("enum").attr("IntEnum")(
        "ErrorCode", members, pybind11::arg("module") = "opendaq.errors");
    errors.attr("ErrorCode") = errorCode;

    pybind11::object daqError = errors.attr("DaqError");

    auto type = pybind11::module_::import("builtins").attr("type");
    pybind11::dict byType = errors.attr("_by_type");
    pybind11::set categories = errors.attr("_categories");
    categories.add(daqError);
    for (const auto& category : errorCategories)
    {
        pybind11::dict ns;
        ns["__module__"] = "opendaq.errors";
        ns["__doc__"] = category.doc;
        pybind11::object cls = type(category.name, pybind11::make_tuple(daqError), ns);
        errors.attr(category.name) = cls;
        byType[pybind11::int_(category.type)] = cls;
        categories.add(cls);
    }

    pybind11::dict byCode = errors.attr("_by_code");
    for (const auto& info : errorCodes)
    {
        const uint32_t typeBits = (info.code >> 16) & 0xFF;
        pybind11::dict ns;
        ns["__module__"] = "opendaq.errors";
        ns["__doc__"] = info.doc;
        ns["code"] = errorCode(info.code);
        pybind11::object parent = byType.attr("get")(pybind11::int_(typeBits), daqError);
        pybind11::object cls = type(info.name, pybind11::make_tuple(parent), ns);
        errors.attr(info.name) = cls;
        byCode[pybind11::int_(info.code)] = cls;
    }

    m.attr("DaqError") = daqError;
}
