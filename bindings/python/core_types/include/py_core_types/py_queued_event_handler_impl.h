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
#include "py_core_types/py_queued_event_handler.h"
#include "py_base_object.h"
#include "py_core_types/py_event_queue.h"
#include "py_core_types/py_converter.h"


template <class F>
class PyQueuedEventHandlerImpl : public PyObjectImpl<pybind11::object, daq::IPythonQueuedEventHandler>
{
public:
    using Super = PyObjectImpl<pybind11::object, daq::IPythonQueuedEventHandler>;
    using Super::Super;

    // IEventHandler
    daq::ErrCode INTERFACE_FUNC handleEvent(daq::IBaseObject* sender, daq::IEventArgs* eventArgs) override;

    // IPythonQueuedEventHandler
    daq::ErrCode INTERFACE_FUNC dispatch(daq::IBaseObject* sender, daq::IEventArgs* eventArgs) override;

    daq::ErrCode INTERFACE_FUNC toString(daq::CharPtr* str) override;

};

template <class F>
daq::ErrCode PyQueuedEventHandlerImpl<F>::handleEvent(daq::IBaseObject* sender, daq::IEventArgs* eventArgs)
{
    return daq::daqTry([&]()
    {
        auto queueWeak = PyEventQueue::GetWeak();
        if (auto queue = queueWeak.lock())
            queue->enqueueEvent(this->borrowInterface<daq::IPythonQueuedEventHandler>(), sender, eventArgs);
    });
}

template <class F>
daq::ErrCode PyQueuedEventHandlerImpl<F>::dispatch(daq::IBaseObject* sender, daq::IEventArgs* eventArgs)
{
    return pyDaqTry([&]()
    {
        pyObject(baseObjectToPyObject(sender), baseObjectToPyObject(eventArgs));
    });
}

template <class F>
daq::ErrCode PyQueuedEventHandlerImpl<F>::toString(daq::CharPtr* str)
{
    OPENDAQ_PARAM_NOT_NULL(str);
    return pyDaqTry([&]()
    {
        std::ostringstream os;
        os << "PyQueuedEventHandler<" << std::string(pybind11::repr(pyObject)) << ">(" << this->getReferenceCount() << " references)";
        return daqDuplicateCharPtr(os.str().c_str(), str);
    });
}

template <class F>
daq::IPythonQueuedEventHandler* PyQueuedEventHandler_Create(const pybind11::object& obj)
{
    auto procObj = new PyQueuedEventHandlerImpl<F>(obj);
    procObj->addRef();
    return procObj;
}

template <class F>
daq::IPythonQueuedEventHandler* PyQueuedEventHandler_Create(pybind11::object&& obj)
{
    auto procObj = new PyQueuedEventHandlerImpl<F>(std::move(obj));
    procObj->addRef();
    return procObj;
}