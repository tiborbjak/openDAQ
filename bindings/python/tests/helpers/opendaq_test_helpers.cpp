#include <pybind11/pybind11.h>

#include <future>
#include <mutex>
#include <thread>

#include "py_core_types/py_core_types.h"

namespace py = pybind11;

// Test-only helpers for scenarios the public API cannot set up deterministically
PYBIND11_MODULE(opendaq_test_helpers, m)
{
    // A worker thread releases the last openDAQ reference to a wrapped Python object while holding a lock
    // that this thread waits for with the GIL held. Deadlocks if releasing the wrapper waits for the GIL.
    m.def("release_wrapped_object_while_gil_holder_waits",
        [](py::object obj)
        {
            auto wrapped = wrapPyObject(std::move(obj));
            std::mutex mutex;
            std::promise<void> locked;

            std::thread worker([&]
            {
                std::lock_guard lock(mutex);
                locked.set_value();
                wrapped.release();
            });

            locked.get_future().wait();
            {
                std::lock_guard lock(mutex);
            }
            worker.join();
        });
}
