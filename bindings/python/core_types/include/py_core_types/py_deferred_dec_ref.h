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

#include <pybind11/pybind11.h>

#include <atomic>
#include <mutex>
#include <vector>

// Releases Python objects in a pending call on the main thread, without waiting for the GIL
class DeferredDecRefs
{
public:
    void defer(pybind11::handle obj);

    // Requires the GIL. Also called from other threads, as pending calls do not run
    // while the main thread executes no bytecode.
    void drain();

private:
    static int drainPendingCall(void* self);

    std::mutex mutex;
    std::vector<PyObject*> objects;
    std::atomic<bool> hasObjects = false;
    bool drainScheduled = false;
};

DeferredDecRefs& deferredDecRefs();

// Use only where arbitrary Python code runs next anyway, as draining can run finalizers
class GilAcquireAndDrain
{
public:
    GilAcquireAndDrain()
    {
        deferredDecRefs().drain();
    }

private:
    pybind11::gil_scoped_acquire gil;
};
