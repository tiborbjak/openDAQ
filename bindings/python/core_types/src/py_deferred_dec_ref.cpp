#include "py_core_types/py_deferred_dec_ref.h"

// Created on first use, so it is valid during static initialization, and never destroyed,
// so worker threads can still defer releases during static destruction
DeferredDecRefs& deferredDecRefs()
{
    static auto* deferred = new DeferredDecRefs();
    return *deferred;
}

int DeferredDecRefs::drainPendingCall(void* self)
{
    auto& deferred = *static_cast<DeferredDecRefs*>(self);
    {
        std::lock_guard lock(deferred.mutex);
        deferred.drainScheduled = false;
    }

    deferred.drain();
    return 0;
}

void DeferredDecRefs::defer(pybind11::handle obj)
{
    std::lock_guard lock(mutex);
    objects.push_back(obj.ptr());
    hasObjects = true;

    // One pending call drains the whole queue; if scheduling fails, the next deferral retries
    if (!drainScheduled)
        drainScheduled = Py_AddPendingCall(drainPendingCall, this) == 0;
}

void DeferredDecRefs::drain()
{
    if (!hasObjects.load(std::memory_order_relaxed))
        return;

    std::vector<PyObject*> drained;
    {
        std::lock_guard lock(mutex);
        drained.swap(objects);
        hasObjects = false;
    }

    // Outside the lock; finalizers can defer further releases
    for (PyObject* obj : drained)
        Py_DECREF(obj);
}
