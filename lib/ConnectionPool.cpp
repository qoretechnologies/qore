/*
    ConnectionPool.cpp

    Qore Programming Language

    Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Note that the Qore library is released under a choice of three open-source
    licenses: MIT (as above), LGPL 2+, or GPL 2+; see README-LICENSE for more
    information.
*/

#include <qore/Qore.h>
#include "qore/intern/ConnectionPool.h"
#include "qore/intern/QC_ConnectionPool.h"

#include <algorithm>

const TypedHashDecl* hashdeclConnectionPoolOptions = nullptr;
const TypedHashDecl* hashdeclConnectionPoolStats = nullptr;

ConnectionPool::ConnectionPool(QoreObject& obj, int min, int max,
    int64 timeout, int64 idle_timeout)
    : obj(obj), min_size(min), max_size(max),
      timeout_ms(timeout), idle_timeout_ms(idle_timeout) {
    obj.tRef();
}

ConnectionPool::~ConnectionPool() {
    assert(!warning_callback);
    assert(!warning_arg);
}

void ConnectionPool::init(ExceptionSink* xsink) {
    for (int i = 0; i < min_size; ++i) {
        QoreObject* resource = createResourceImpl(xsink);
        if (*xsink) {
            return;
        }
        if (!resource) {
            xsink->raiseException("CONNECTIONPOOL-ERROR", "createResource() returned NOTHING");
            return;
        }
        pool.push_back(resource);
        last_released.push_back(q_clock_getmicros());
        free_list.push_back(i);
        ++live_count;
    }
    peak_size = live_count;
}

QoreObject* ConnectionPool::acquire(ExceptionSink* xsink) {
    int tid = q_gettid();

    SafeLocker sl(this);
    if (!valid) {
        xsink->raiseException("CONNECTIONPOOL-ERROR", "connection pool has been shut down");
        return nullptr;
    }

    // Check if this thread already has a resource
    auto it = tmap.find(tid);
    if (it != tmap.end()) {
        ++total_acquires;
        QoreObject* resource = pool[it->second];
        assert(resource);
        sl.unlock();
        return resource->objectRefSelf();
    }

    int64 start_us = q_clock_getmicros();

    while (true) {
        // Try to get a free resource (LIFO for cache warmth)
        while (!free_list.empty()) {
            int idx = free_list.back();
            free_list.pop_back();
            // Skip tombstones
            if (!pool[idx]) {
                continue;
            }
            QoreObject* resource = pool[idx];
            tmap[tid] = idx;
            ++total_acquires;
            sl.unlock();

            set_thread_resource(this);

            return resource->objectRefSelf();
        }

        // Try to create a new resource (if under max, including being-created resources)
        if (!max_size || live_count + creating_count < max_size) {
            ++creating_count;

            // Release lock during resource creation (may block in Qore code)
            sl.unlock();
            QoreObject* resource = createResourceImpl(xsink);
            sl.lock();
            --creating_count;
            // Signal a waiter -- creating_count decreased, so another thread may create
            if (wait_count_val > 0) {
                signal();
            }

            if (*xsink) {
                return nullptr;
            }
            if (!resource) {
                xsink->raiseException("CONNECTIONPOOL-ERROR", "createResource() returned NOTHING");
                return nullptr;
            }

            if (!valid) {
                sl.unlock();
                resource->deref(xsink);
                xsink->raiseException("CONNECTIONPOOL-ERROR",
                    "connection pool shut down during resource creation");
                return nullptr;
            }

            // Reuse a tombstone slot if available, otherwise append
            int idx = -1;
            for (int i = 0, n = (int)pool.size(); i < n; ++i) {
                if (!pool[i]) {
                    idx = i;
                    break;
                }
            }
            if (idx >= 0) {
                pool[idx] = resource;
                last_released[idx] = q_clock_getmicros();
            } else {
                idx = (int)pool.size();
                pool.push_back(resource);
                last_released.push_back(q_clock_getmicros());
            }
            ++live_count;
            tmap[tid] = idx;
            if (live_count > peak_size) {
                peak_size = live_count;
            }
            ++total_acquires;
            sl.unlock();

            set_thread_resource(this);

            return resource->objectRefSelf();
        }

        // Pool exhausted -- wait for a free resource
        ++wait_count_val;

        if (timeout_ms > 0) {
            int64 elapsed_us = q_clock_getmicros() - start_us;
            int64 remaining_ms = timeout_ms - elapsed_us / 1000;
            if (remaining_ms <= 0) {
                --wait_count_val;
                ++total_timeouts;
                xsink->raiseException("CONNECTIONPOOL-ERROR",
                    "timed out waiting for a free resource after " QLLD " ms "
                    "(pool size: %d, all in use)", timeout_ms, (int)pool.size());
                return nullptr;
            }
            wait(this, remaining_ms);
        } else {
            // timeout_ms == 0 means wait indefinitely
            wait(this);
        }
        --wait_count_val;

        if (!valid) {
            xsink->raiseException("CONNECTIONPOOL-ERROR",
                "connection pool shut down while waiting");
            return nullptr;
        }

        // Check if wait exceeded warning threshold
        if (warning_ms > 0 && warning_callback) {
            int64 elapsed_us = q_clock_getmicros() - start_us;
            int64 elapsed_ms = elapsed_us / 1000;
            if (elapsed_ms >= warning_ms) {
                ReferenceHolder<ResolvedCallReferenceNode> cb(
                    warning_callback->refRefSelf(), xsink);
                QoreValue cb_arg = warning_arg.refSelf();
                ReferenceHolder<QoreHashNode> stats(getStatsIntern(xsink), xsink);
                sl.unlock();

                // Invoke callback outside lock
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(stats.release(), xsink);
                args->push(cb_arg, xsink);
                (*cb)->execValue(*args, xsink).discard(xsink);

                if (*xsink) {
                    // Ignore callback errors
                    xsink->clear();
                }

                sl.lock();
            }
        }
    }
}

void ConnectionPool::release(ExceptionSink* xsink) {
    int tid = q_gettid();

    int idx;
    QoreObject* resource;
    {
        SafeLocker sl(this);
        auto it = tmap.find(tid);
        if (it == tmap.end()) {
            return;
        }
        idx = it->second;
        assert(idx >= 0 && idx < (int)pool.size());
        resource = pool[idx];
        assert(resource);
        tmap.erase(it);
    }

    remove_thread_resource(this);

    // Reset resource outside the lock
    resetResourceImpl(resource, xsink);
    if (*xsink) {
        // Ignore reset errors
        xsink->clear();
    }

    {
        AutoLocker al(this);
        last_released[idx] = q_clock_getmicros();
        free_list.push_back(idx);
        if (wait_count_val > 0) {
            signal();
        }
    }
}

void ConnectionPool::cleanup(ExceptionSink* xsink) {
    release(xsink);
}

void ConnectionPool::shutdown(ExceptionSink* xsink) {
    AutoLocker al(this);
    if (!valid) {
        return;
    }
    valid = false;
    broadcast();
}

void ConnectionPool::destructor(ExceptionSink* xsink) {
    std::vector<QoreObject*> to_destroy;
    ResolvedCallReferenceNode* cb = nullptr;
    QoreValue cb_arg;

    {
        AutoLocker al(this);
        valid = false;
        broadcast();

        // Move pool contents under lock so woken waiters cannot access them
        to_destroy = std::move(pool);
        pool.clear();
        last_released.clear();
        free_list.clear();
        tmap.clear();

        cb = warning_callback;
        warning_callback = nullptr;
        cb_arg = warning_arg;
        warning_arg = QoreValue();
    }

    // Destroy resources outside lock — call destroyResourceImpl() first
    // to give resources a chance to close deterministically (matching
    // cleanupIdle()'s behavior), then deref to release the reference
    for (QoreObject* resource : to_destroy) {
        if (resource) {
            destroyResourceImpl(resource, xsink);
            if (*xsink) {
                xsink->clear();
            }
            resource->deref(xsink);
        }
    }

    // Release warning callback outside lock
    if (cb) {
        cb->deref(xsink);
        cb_arg.discard(xsink);
    }

    obj.tDeref();
}

QoreHashNode* ConnectionPool::getStats(ExceptionSink* xsink) const {
    AutoLocker al(const_cast<ConnectionPool*>(this));
    return getStatsIntern(xsink);
}

QoreHashNode* ConnectionPool::getStatsIntern(ExceptionSink* xsink) const {
    ReferenceHolder<QoreHashNode> h(new QoreHashNode(hashdeclConnectionPoolStats, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    h->setKeyValue("pool_size", live_count, xsink);
    h->setKeyValue("in_use", (int64)tmap.size(), xsink);
    h->setKeyValue("free", live_count - (int64)tmap.size(), xsink);
    h->setKeyValue("wait_count", (int64)wait_count_val, xsink);
    h->setKeyValue("peak_size", (int64)peak_size, xsink);
    h->setKeyValue("total_acquires", total_acquires, xsink);
    h->setKeyValue("total_timeouts", total_timeouts, xsink);
    return h.release();
}

int ConnectionPool::cleanupIdle(ExceptionSink* xsink) {
    if (!idle_timeout_ms) {
        return 0;
    }

    int64 cutoff_us = q_clock_getmicros() - idle_timeout_ms * 1000;

    std::vector<QoreObject*> to_destroy;

    {
        AutoLocker al(this);
        int in_use = (int)tmap.size();
        int free_count = live_count - in_use;
        int must_keep = std::max(0, min_size - in_use);
        int removable = free_count - must_keep;
        if (removable <= 0) {
            return 0;
        }

        int removed = 0;
        std::deque<int> surviving;
        for (int idx : free_list) {
            if (!pool[idx]) {
                continue;
            }
            if (removed < removable && last_released[idx] < cutoff_us) {
                to_destroy.push_back(pool[idx]);
                pool[idx] = nullptr;
                --live_count;
                ++removed;
            } else {
                surviving.push_back(idx);
            }
        }
        free_list = std::move(surviving);
    }

    // Destroy resources outside lock
    for (QoreObject* resource : to_destroy) {
        destroyResourceImpl(resource, xsink);
        if (*xsink) {
            xsink->clear();
        }
        resource->deref(xsink);
    }

    return (int)to_destroy.size();
}

void ConnectionPool::setWarningCallback(int64 wms, ResolvedCallReferenceNode* cb,
    QoreValue arg, ExceptionSink* xsink) {
    AutoLocker al(this);
    if (warning_callback) {
        warning_callback->deref(xsink);
    }
    warning_arg.discard(xsink);
    warning_ms = wms;
    warning_callback = cb;
    warning_arg = arg;
}

void ConnectionPool::clearWarningCallback(ExceptionSink* xsink) {
    AutoLocker al(this);
    warning_ms = 0;
    if (warning_callback) {
        warning_callback->deref(xsink);
        warning_callback = nullptr;
    }
    warning_arg.discard(xsink);
    warning_arg = QoreValue();
}

// --- QoreConnectionPool: bridges back to Qore ---

QoreConnectionPool::QoreConnectionPool(QoreObject& obj, int min, int max,
    int64 timeout, int64 idle_timeout)
    : ConnectionPool(obj, min, max, timeout, idle_timeout) {
}

QoreObject* QoreConnectionPool::createResourceImpl(ExceptionSink* xsink) {
    QoreValue rv = obj.evalMethod("createResource", nullptr, xsink);
    if (*xsink) {
        rv.discard(xsink);
        return nullptr;
    }
    AbstractQoreNode* n = rv.takeNode();
    if (!n) {
        return nullptr;
    }
    QoreObject* resource = dynamic_cast<QoreObject*>(n);
    if (!resource) {
        n->deref(xsink);
        xsink->raiseException("CONNECTIONPOOL-ERROR",
            "createResource() did not return an object");
        return nullptr;
    }
    return resource;
}

void QoreConnectionPool::resetResourceImpl(QoreObject* resource, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(resource->objectRefSelf(), xsink);
    obj.evalMethod("resetResource", *args, xsink).discard(xsink);
}

void QoreConnectionPool::destroyResourceImpl(QoreObject* resource, ExceptionSink* xsink) {
    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
    args->push(resource->objectRefSelf(), xsink);
    obj.evalMethod("destroyResource", *args, xsink).discard(xsink);
}
