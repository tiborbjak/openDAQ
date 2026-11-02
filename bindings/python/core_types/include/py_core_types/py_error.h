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

#include <coretypes/ctutils.h>
#include <coretypes/exceptions.h>

#include <pybind11/pybind11.h>

#include "py_core_types/py_gil.h"

#include <utility>

// Runs func with the GIL held and turns any exception it throws, including pybind11::error_already_set, into
// an error code, or fails with OPENDAQ_ERR_INVALIDSTATE if Python is exiting. Use it in openDAQ interface
// methods that call into Python; it is the single place to hook into errors raised by Python code.
// Use daqTry where Python isn't involved.
template <class F>
daq::ErrCode pyDaqTry(const daq::IBaseObject* context, F&& func)
{
    PY_ACQUIRE_GIL_OR_RETURN();
    return daq::daqTry(context, std::forward<F>(func));
}

template <class F>
daq::ErrCode pyDaqTry(F&& func)
{
    return pyDaqTry(nullptr, std::forward<F>(func));
}

void registerDaqErrors(pybind11::module_ m);
