/* -*- indent-tabs-mode: nil -*- */
/*
    AsyncIoControllerPriv.cpp

    Qore Programming Language

    Copyright (C) 2003 - 2026 Qore Technologies, s.r.o.

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
#include <qore/QoreSocketObject.h>
#include "qore/intern/AsyncIoControllerPriv.h"
#include "qore/intern/Http2Session.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QoreLibIntern.h"
#include "qore/intern/QoreAsyncIoLogger.h"

#include <cstdarg>

extern qore_classid_t CID_QUEUE;
extern QoreClass* QC_QUEUE;

//! Returns the current time in microseconds since the epoch
static int64 get_epoch_us() {
    int us;
    int64 secs = q_epoch_us(us);
    return secs * 1000000LL + us;
}

// --- PollInfo implementation ---

void AsyncIoControllerPriv::PollInfo::cleanup(ExceptionSink* xsink) {
    if (sock_obj) {
        sock_obj->deref(xsink);
        sock_obj = nullptr;
    }
    if (sock) {
        sock->deref(xsink);
        sock = nullptr;
    }
    if (spop_obj) {
        spop_obj->deref(xsink);
        spop_obj = nullptr;
    }
    if (poll_info) {
        poll_info->deref(xsink);
        poll_info = nullptr;
    }
    if (other) {
        other->deref(xsink);
        other = nullptr;
    }
    if (queue) {
        queue->deref(xsink);
        queue = nullptr;
    }
    if (callback) {
        callback->deref(xsink);
        callback = nullptr;
    }
}

// --- QoreCallDispatcher implementation ---

QoreCallDispatcher::QoreCallDispatcher(int max_workers) : max_workers(max_workers) {
}

QoreCallDispatcher::~QoreCallDispatcher() {
}

QoreHashNode* QoreCallDispatcher::dispatchContinuePoll(QoreObject* spop_obj, ExceptionSink* caller_xsink) {
    WorkItem item;
    item.spop_obj = spop_obj;

    {
        AutoLocker al(m);

        if (stopping) {
            caller_xsink->raiseException("ASYNC-IO-ERROR", "call dispatcher is shutting down");
            return nullptr;
        }

        // Lazily spawn a worker if needed and below max
        if (active_workers < max_workers) {
            ++active_workers;
            int tid = q_start_thread(caller_xsink, workerEntry, this);
            if (tid == -1) {
                --active_workers;
                // If no workers exist at all, we cannot dispatch
                if (active_workers == 0) {
                    return nullptr;
                }
                // Otherwise fall through — an existing worker will pick up the item
            }
        }

        queue.push_back(&item);
        work_avail.signal();
    }

    // Block until the worker completes our item
    {
        AutoLocker al(m);
        while (!item.completed) {
            item.done.wait(m);
        }
    }

    // Transfer any exceptions from the worker
    if (item.xsink) {
        caller_xsink->assimilate(item.xsink);
    }

    return item.result;
}

void QoreCallDispatcher::stop(ExceptionSink* xsink) {
    AutoLocker al(m);
    stopping = true;
    work_avail.broadcast();

    // Wait for all workers to exit before returning, since the caller may
    // delete this object immediately after stop() returns
    while (active_workers > 0) {
        workers_done.wait(m);
    }
}

void QoreCallDispatcher::workerEntry(ExceptionSink* xsink, void* arg) {
    QoreCallDispatcher* self = static_cast<QoreCallDispatcher*>(arg);
    self->workerLoop(xsink);
    if (*xsink) {
        // Safety net: workerLoop should capture all exceptions in WorkItem::xsink.
        // If something leaked here, we can't log (no controller reference), so
        // we clear to prevent an unhandled-exception crash on thread exit.
        xsink->clear();
    }
}

void QoreCallDispatcher::workerLoop(ExceptionSink* xsink) {
    while (true) {
        WorkItem* item = nullptr;

        {
            AutoLocker al(m);
            while (queue.empty() && !stopping) {
                work_avail.wait(m);
            }
            if (stopping && queue.empty()) {
                --active_workers;
                if (active_workers == 0) {
                    workers_done.broadcast();
                }
                return;
            }
            item = queue.front();
            queue.pop_front();
        }

        // Execute the continuePoll call in this Qore-capable thread
        ValueHolder rv(item->spop_obj->evalMethod("continuePoll", nullptr, &item->xsink), &item->xsink);
        if (!item->xsink && rv->getType() == NT_HASH) {
            item->result = rv.release().get<QoreHashNode>();
        }

        // Mark exceptions as externally managed: they were created on this worker
        // thread (incrementing this thread's active_exceptions counter), but will be
        // consumed by the caller thread via assimilate().  markExternallyManaged()
        // decrements this thread's counter so the thread can exit cleanly.
        if (item->xsink) {
            item->xsink.markExternallyManaged();
        }

        // Signal completion
        {
            AutoLocker al(m);
            item->completed = true;
            item->done.signal();
        }
    }
}

// --- AsyncIoControllerPriv implementation ---

AsyncIoControllerPriv::AsyncIoControllerPriv(bool autostop, ExceptionSink* xsink)
    : tid(0), autostop_flag(autostop), shutting_down(false), force_poll(false),
      io_waiting(false), io_exiting(false), ready_flag(false),
      submit_seq(0), processed_seq(0), logger(nullptr), timer_callback(nullptr),
      loop(nullptr), notifier(nullptr) {
    loop = new QoreEventLoop(xsink);
    if (*xsink) {
        return;
    }
    notifier = new QoreEventNotifier(xsink);
    if (*xsink) {
        return;
    }
}

AsyncIoControllerPriv::~AsyncIoControllerPriv() {
    delete loop;
    ExceptionSink xsink;
    if (notifier) {
        notifier->deref(&xsink);
        notifier = nullptr;
    }
    if (xsink) {
        xsink.clear();
    }
}

void AsyncIoControllerPriv::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        stop(xsink);
        if (call_dispatcher) {
            call_dispatcher->stop(xsink);
            delete call_dispatcher;
            call_dispatcher = nullptr;
        }
        // Process any remaining deferred deletes
        for (auto* d : deferred_dti_deletes) {
            delete d;
        }
        deferred_dti_deletes.clear();
        if (logger) {
            logger->deref(xsink);
            logger = nullptr;
        }
        if (timer_callback) {
            timer_callback->deref(xsink);
            timer_callback = nullptr;
        }
        for (auto& [id, tinfo] : timer_info_map) {
            tinfo.udata.discard(xsink);
        }
        timer_info_map.clear();
        delete this;
    }
}

// --- Public API ---

QoreObject* AsyncIoControllerPriv::submit(QoreObject* self, QoreHashNode* info, bool replace,
        ExceptionSink* xsink) {
    // Extract fields from info hash
    QoreValue v = info->getKeyValue("sock");
    QoreObject* sock_obj = v.getType() == NT_OBJECT ? v.get<QoreObject>() : nullptr;
    if (!sock_obj) {
        xsink->raiseException("ASYNC-IO-ERROR", "missing 'sock' field in SocketPollOperationInfo");
        return nullptr;
    }

    QoreSocketObject* sock = static_cast<QoreSocketObject*>(
        sock_obj->getReferencedPrivateData(CID_SOCKET, xsink));
    if (!sock) {
        if (!*xsink) {
            xsink->raiseException("ASYNC-IO-ERROR", "invalid Socket object in 'sock' field");
        }
        return nullptr;
    }
    ReferenceHolder<QoreSocketObject> sock_holder(sock, xsink);

    v = info->getKeyValue("spop");
    QoreObject* spop_obj = v.getType() == NT_OBJECT ? v.get<QoreObject>() : nullptr;
    if (!spop_obj) {
        xsink->raiseException("ASYNC-IO-ERROR", "missing 'spop' field in SocketPollOperationInfo");
        return nullptr;
    }

    v = info->getKeyValue("owner");
    std::string owner;
    if (v.getType() == NT_STRING) {
        owner = v.get<const QoreStringNode>()->c_str();
    }
    if (owner.empty()) {
        xsink->raiseException("ASYNC-IO-ERROR", "missing 'owner' field in SocketPollOperationInfo; "
            "required for proper shutdown via cancelByOwner()");
        return nullptr;
    }

    // Get the key: custom key or socket unique hash
    v = info->getKeyValue("key");
    std::string uh;
    if (v.getType() == NT_STRING && v.get<const QoreStringNode>()->size()) {
        uh = v.get<const QoreStringNode>()->c_str();
    } else {
        uh = getSocketHash(sock);
    }

    // Get timeout
    v = info->getKeyValue("to");
    int64 timeout_us = DEFAULT_IO_TIMEOUT_US;
    if (v.getType() == NT_DATE) {
        timeout_us = v.get<const DateTimeNode>()->getRelativeSecondsDouble() * 1000000.0;
    } else if (v.getType() == NT_INT) {
        // timeout in milliseconds
        timeout_us = v.getAsBigInt() * 1000LL;
    }

    // Get callback — wrap in RAII immediately
    v = info->getKeyValue("callback");
    ResolvedCallReferenceNode* callback = nullptr;
    if (v.getType() == NT_RUNTIME_CLOSURE || v.getType() == NT_FUNCREF) {
        callback = v.get<ResolvedCallReferenceNode>();
        callback->ref();
    }

    // RAII cleanup for refcounted resources acquired below.
    // Pointers are nulled as ownership transfers to PollInfo or the caller.
    struct SubmitResources {
        ResolvedCallReferenceNode* callback;
        Queue* result_queue;
        QoreHashNode* other_hash;
        QoreHashNode* poll_info_hash;
        QoreObject* new_queue_obj;
        ExceptionSink* xsink;

        ~SubmitResources() {
            if (callback) { callback->deref(xsink); }
            if (result_queue) { result_queue->deref(xsink); }
            if (other_hash) { other_hash->deref(xsink); }
            if (poll_info_hash) { poll_info_hash->deref(xsink); }
            if (new_queue_obj) { new_queue_obj->deref(xsink); }
        }
    } res{callback, nullptr, nullptr, nullptr, nullptr, xsink};

    // Get result queue
    v = info->getKeyValue("resultQueue");
    QoreObject* result_queue_obj = nullptr;
    if (v.getType() == NT_OBJECT) {
        QoreObject* qobj = v.get<QoreObject>();
        res.result_queue = static_cast<Queue*>(qobj->getReferencedPrivateData(CID_QUEUE, xsink));
        if (*xsink) {
            return nullptr;
        }
        if (res.result_queue) {
            result_queue_obj = qobj;
        }
    }

    // Get 'other' data
    v = info->getKeyValue("other");
    if (v.getType() == NT_HASH) {
        res.other_hash = v.get<QoreHashNode>();
        res.other_hash->ref();
    }

    // Get poll_info
    v = info->getKeyValue("poll_info");
    if (v.getType() == NT_HASH) {
        res.poll_info_hash = v.get<QoreHashNode>();
        res.poll_info_hash->ref();
    }

    // Check dedicated_thread flag
    v = info->getKeyValue("dedicated_thread");
    bool dedicated = v.getAsBool();

    // If no callback and no shared queue, create a new result queue
    if (!callback && !res.result_queue) {
        res.result_queue = new Queue();
        res.new_queue_obj = new QoreObject(QC_QUEUE, getProgram(), res.result_queue);
        res.result_queue->ref();  // extra ref for PollInfo storage
    }

    // --- Dedicated thread path ---
    if (dedicated) {
        // Check for Qore-language continuePoll() override
        const QoreClass* cls = spop_obj->getClass();
        const QoreMethod* meth = cls->findMethod("continuePoll");
        bool has_qore_override = meth && !meth->isBuiltin();

        // Get C++ poll operation for direct calls (if no Qore override)
        SocketPollOperationBase* spop_base = nullptr;
        if (!has_qore_override) {
            spop_base = static_cast<SocketPollOperationBase*>(
                spop_obj->getReferencedPrivateData(CID_SOCKETPOLLOPERATIONBASE, xsink));
            if (!spop_base) {
                if (!*xsink) {
                    xsink->raiseException("ASYNC-IO-ERROR",
                        "dedicated_thread requires a SocketPollOperationBase C++ implementation; "
                        "object of class '%s' has no C++ continuePoll()", cls->getName());
                }
                return nullptr;
            }
        }

        // Cancel any existing dedicated thread with same key
        {
            AutoLocker al(m);
            if (shutting_down) {
                if (spop_base) {
                    spop_base->deref(xsink);
                }
                xsink->raiseException("ASYNC-IO-ERROR", "controller is shutting down");
                return nullptr;
            }

            // Also check both maps for existing key
            if (dedicated_threads.count(uh) || cache.count(uh)) {
                if (!replace) {
                    if (spop_base) {
                        spop_base->deref(xsink);
                    }
                    xsink->raiseException("ASYNC-IO-ERROR",
                        "operation with key '%s' already exists; use replace=True to replace", uh.c_str());
                    return nullptr;
                }
            }
        }

        // Cancel existing (outside lock)
        if (replace) {
            cancelDedicatedThread(uh, xsink);
        }

        // Build DedicatedThreadInfo
        DedicatedThreadInfo* dti = new DedicatedThreadInfo();
        dti->controller = this;
        dti->key = uh;
        dti->has_qore_override = has_qore_override;
        dti->spop_base = spop_base;  // Takes ownership of ref (or nullptr)

        // Populate PollInfo
        dti->pinfo.sock_obj = sock_obj;
        sock_obj->ref();
        dti->pinfo.sock = sock;
        sock_holder.release();
        dti->pinfo.spop_obj = spop_obj;
        spop_obj->ref();
        dti->pinfo.poll_info = res.poll_info_hash; res.poll_info_hash = nullptr;
        dti->pinfo.timeout_us = timeout_us;
        dti->pinfo.owner = owner;
        dti->pinfo.other = res.other_hash; res.other_hash = nullptr;
        dti->pinfo.queue = res.result_queue; res.result_queue = nullptr;
        dti->pinfo.callback = res.callback; res.callback = nullptr;
        dti->pinfo.timeout_date_us = 0;

        // Create call dispatcher lazily if needed
        if (has_qore_override) {
            AutoLocker al(m);
            if (!call_dispatcher) {
                call_dispatcher = new QoreCallDispatcher();
            }
        }

        spawnDedicatedThread(dti, xsink);
        if (*xsink) {
            dti->pinfo.cleanup(xsink);
            if (dti->spop_base) {
                dti->spop_base->deref(xsink);
            }
            delete dti;
            return nullptr;
        }

        log(QORE_LOG_LEVEL_DEBUG, "submit: dedicated thread operation '%s' submitted (owner: '%s'%s)",
            uh.c_str(), owner.c_str(), has_qore_override ? ", Qore override" : "");

        // Return queue to caller
        if (res.new_queue_obj) {
            QoreObject* rv = res.new_queue_obj;
            res.new_queue_obj = nullptr;
            return rv;
        }
        if (result_queue_obj) {
            result_queue_obj->ref();
            return result_queue_obj;
        }
        return nullptr;
    }

    // --- Normal (main I/O thread) path ---

    bool do_signal = false;
    bool need_cancel = false;
    PollInfo direct_pinfo;
    bool direct_cancel = false;

    {
        AutoLocker al(m);

        if (shutting_down) {
            xsink->raiseException("ASYNC-IO-ERROR", "controller is shutting down");
            return nullptr;
        }

        // Check for existing operation with same key
        auto it = cache.find(uh);
        if (it != cache.end()) {
            if (!replace) {
                xsink->raiseException("ASYNC-IO-ERROR",
                    "operation with key '%s' already exists; use replace=True to replace", uh.c_str());
                return nullptr;
            }
            if (q_gettid() == tid) {
                // On the I/O thread — cancel directly to avoid deadlock
                direct_pinfo = it->second;
                it->second = PollInfo();
                cache.erase(it);
                direct_cancel = true;
            } else {
                // Queue cancel for existing operation
                if (!cancel_cond_map.count(uh)) {
                    cancel_cond_map[uh] = new QoreCondition();
                }
                do_signal = enqueueCmdLocked(IoCommand::Cancel, uh);
                need_cancel = true;
            }
        }
    }

    // Signal notifier outside lock
    if (do_signal) {
        notifier->notify();
    }

    // Handle direct cancel (on I/O thread) or wait for enqueued cancel
    if (direct_cancel) {
        doCancelIntern(direct_pinfo, xsink);
        direct_pinfo.cleanup(xsink);
    } else if (need_cancel) {
        waitCancel(uh);
    }

    // Now add the new operation
    {
        AutoLocker al(m);

        if (shutting_down) {
            xsink->raiseException("ASYNC-IO-ERROR", "controller is shutting down");
            return nullptr;
        }

        PollInfo& pinfo = cache[uh];
        pinfo.sock_obj = sock_obj;
        sock_obj->ref();
        pinfo.sock = sock;
        sock_holder.release();  // Transfer ownership to pinfo
        pinfo.spop_obj = spop_obj;
        spop_obj->ref();
        // Transfer refcounted resources from RAII struct to pinfo
        pinfo.poll_info = res.poll_info_hash; res.poll_info_hash = nullptr;
        pinfo.timeout_us = timeout_us;
        pinfo.owner = owner;
        pinfo.other = res.other_hash; res.other_hash = nullptr;
        pinfo.queue = res.result_queue; res.result_queue = nullptr;
        pinfo.callback = res.callback; res.callback = nullptr;
        pinfo.timeout_date_us = 0;  // Will be set in I/O thread

        // Start I/O thread if needed
        if (io_exiting || !tid) {
            startIntern(xsink);
            if (*xsink) {
                // Copy PollInfo out before erasing — pinfo is a reference into cache
                PollInfo failed_pinfo = pinfo;
                pinfo = PollInfo();
                cache.erase(uh);
                failed_pinfo.cleanup(xsink);
                // res still owns new_queue_obj — destructor will clean it up
                return nullptr;
            }
        }

        ++submit_seq;
        do_signal = enqueueCmdLocked(IoCommand::Add, uh);
    }

    if (do_signal) {
        notifier->notify();
    }

    log(QORE_LOG_LEVEL_DEBUG, "submit: operation '%s' submitted (owner: '%s')", uh.c_str(), owner.c_str());

    // Return queue to caller — transfer ownership out of RAII struct
    if (res.new_queue_obj) {
        QoreObject* rv = res.new_queue_obj;
        res.new_queue_obj = nullptr;
        return rv;
    }
    if (result_queue_obj) {
        result_queue_obj->ref();
        return result_queue_obj;
    }
    return nullptr;
}

QoreHashNode* AsyncIoControllerPriv::exec(QoreObject* self, QoreHashNode* info, bool replace,
        ExceptionSink* xsink) {
    // Submit the operation - this creates a Queue for us
    QoreObject* q_obj = submit(self, info, replace, xsink);
    if (*xsink) {
        return nullptr;
    }

    if (!q_obj) {
        xsink->raiseException("ASYNC-IO-ERROR", "exec() cannot be used with a callback");
        return nullptr;
    }

    ReferenceHolder<QoreObject> q_holder(q_obj, xsink);
    Queue* q = static_cast<Queue*>(q_obj->getReferencedPrivateData(CID_QUEUE, xsink));
    if (!q || *xsink) {
        return nullptr;
    }
    ReferenceHolder<Queue> q_ref(q, xsink);

    // Wait for result (blocking)
    QoreValue result = q->shift(xsink);
    if (*xsink) {
        return nullptr;
    }
    if (result.getType() == NT_HASH) {
        return result.get<QoreHashNode>();
    }
    result.discard(xsink);
    return nullptr;
}

bool AsyncIoControllerPriv::cancel(QoreSocketObject* sock, ExceptionSink* xsink) {
    std::string uh = getSocketHash(sock);
    SimpleRefHolder<QoreStringNode> key(new QoreStringNode(uh));
    return cancelByKey(*key, xsink);
}

bool AsyncIoControllerPriv::cancelByKey(const QoreStringNode* key, ExceptionSink* xsink) {
    std::string uh(key->c_str());

    // Check dedicated threads first
    if (cancelDedicatedThread(uh, xsink)) {
        return true;
    }

    bool rv = false;
    bool do_signal = false;
    bool stopped = false;
    PollInfo direct_pinfo;
    bool direct_cancel = false;

    {
        AutoLocker al(m);
        auto it = cache.find(uh);
        if (it != cache.end()) {
            rv = true;
            int current_tid = q_gettid();
            printd(2, "cancelByKey '%s': current_tid=%d io_tid=%d io_exiting=%d\n",
                uh.c_str(), current_tid, tid, (int)io_exiting);
            if (tid && !io_exiting && current_tid != tid) {
                // I/O thread is running and we're NOT on it — enqueue the cancel command
                if (!cancel_cond_map.count(uh)) {
                    cancel_cond_map[uh] = new QoreCondition();
                }
                do_signal = enqueueCmdLocked(IoCommand::Cancel, uh);
            } else {
                // I/O thread not running OR we ARE the I/O thread — handle cancel directly
                // (enqueuing+waiting from the I/O thread would deadlock since the I/O thread
                // is the one that processes cancel commands)
                direct_cancel = true;
                direct_pinfo = it->second;
                it->second = PollInfo();
                cache.erase(it);
            }
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    if (direct_cancel) {
        doCancelIntern(direct_pinfo, xsink);
        direct_pinfo.cleanup(xsink);
    } else if (rv) {
        waitCancel(uh);
    }

    // Check autostop
    {
        AutoLocker al(m);
        if (autostop_flag && rv && cache.empty() && dedicated_threads.empty() && tid) {
            do_signal = enqueueCmdLocked(IoCommand::Quit);
            stopped = true;
        } else {
            do_signal = false;
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    if (stopped) {
        waitStop(xsink);
    }

    return rv;
}

int AsyncIoControllerPriv::cancelByOwner(const QoreStringNode* owner, ExceptionSink* xsink) {
    std::string owner_str(owner->c_str());
    bool do_signal = false;
    int count = 0;
    std::vector<PollInfo> direct_pinfos;

    // Cancel matching dedicated threads first
    std::vector<std::string> dedicated_keys;
    {
        AutoLocker al(m);
        for (auto& [key, dti] : dedicated_threads) {
            if (dti->pinfo.owner == owner_str) {
                dedicated_keys.push_back(key);
            }
        }
    }
    for (auto& key : dedicated_keys) {
        if (cancelDedicatedThread(key, xsink)) {
            ++count;
        }
    }

    // Now handle cache operations separately
    int cache_count = 0;
    QoreCondition done_cond;
    bool cancel_done = false;

    {
        AutoLocker al(m);

        // Count matching operations in main cache
        for (auto& it : cache) {
            if (it.second.owner == owner_str) {
                ++cache_count;
            }
        }

        if (cache_count > 0) {
            if (tid && !io_exiting && q_gettid() != tid) {
                // I/O thread is running and we're NOT on it — enqueue the cancel command
                do_signal = enqueueCmdLocked(IoCommand::CancelOwner, std::string(), owner_str, &done_cond,
                    &cancel_done);
            } else {
                // I/O thread not running OR we ARE the I/O thread — handle cancel directly
                std::vector<std::string> keys;
                for (auto& [key, pinfo] : cache) {
                    if (pinfo.owner == owner_str) {
                        keys.push_back(key);
                    }
                }
                for (auto& key : keys) {
                    auto it = cache.find(key);
                    if (it != cache.end()) {
                        direct_pinfos.push_back(it->second);
                        it->second = PollInfo();
                        cache.erase(it);
                    }
                }
            }
        }
    }

    count += cache_count;

    if (do_signal) {
        notifier->notify();
    }

    if (!direct_pinfos.empty()) {
        // Deliver cancel results directly (I/O thread not running)
        for (auto& pinfo : direct_pinfos) {
            doCancelIntern(pinfo, xsink);
            pinfo.cleanup(xsink);
        }
    } else if (do_signal) {
        // Wait for I/O thread to complete the cancel (only when we actually enqueued a command)
        {
            AutoLocker al(m);
            while (!cancel_done) {
                done_cond.wait(m);
            }

            // If the I/O thread exited without processing the CancelOwner command
            // (e.g., via autostop), it sets cancel_done in exit cleanup but ops may
            // still remain in cache — collect them for direct cancel
            std::vector<std::string> keys;
            for (auto& [key, pinfo] : cache) {
                if (pinfo.owner == owner_str) {
                    keys.push_back(key);
                }
            }
            for (auto& key : keys) {
                auto it = cache.find(key);
                if (it != cache.end()) {
                    direct_pinfos.push_back(it->second);
                    it->second = PollInfo();
                    cache.erase(it);
                }
            }
        }

        // Deliver cancel results for any remaining ops after I/O thread exit
        for (auto& pinfo : direct_pinfos) {
            doCancelIntern(pinfo, xsink);
            pinfo.cleanup(xsink);
        }
    }

    // Check autostop
    bool stopped = false;
    {
        AutoLocker al(m);
        if (autostop_flag && count > 0 && cache.empty() && dedicated_threads.empty() && tid) {
            do_signal = enqueueCmdLocked(IoCommand::Quit);
            stopped = true;
        } else {
            do_signal = false;
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    if (stopped) {
        waitStop(xsink);
    }

    return count;
}

void AsyncIoControllerPriv::wake() {
    bool do_signal = false;
    // Stack buffer for dedicated thread notifiers (avoids heap allocation)
    // 8 slots covers typical use; falls back to heap for more
    QoreEventNotifier* dt_buf[8];
    int dt_count = 0;
    {
        AutoLocker al(m);
        if (tid && !io_exiting) {
            do_signal = enqueueCmdLocked(IoCommand::Wake);
        }
        // Also wake all dedicated threads so they check for new work
        for (auto& [key, dti] : dedicated_threads) {
            if (dti->notifier && !dti->stop_requested.load(std::memory_order_relaxed)) {
                if (dt_count < 8) {
                    dti->notifier->ref();
                    dt_buf[dt_count++] = dti->notifier;
                }
                // If > 8 dedicated threads, the extras will be woken on next poll timeout
            }
        }
    }
    if (do_signal) {
        notifier->notify();
    }
    // Signal dedicated thread notifiers outside lock
    for (int i = 0; i < dt_count; ++i) {
        dt_buf[i]->notify();
        ExceptionSink xsink;
        dt_buf[i]->deref(&xsink);
    }
}

void AsyncIoControllerPriv::start(ExceptionSink* xsink) {
    AutoLocker al(m);
    if (!tid || io_exiting) {
        startIntern(xsink);
    }
}

void AsyncIoControllerPriv::stop(ExceptionSink* xsink) {
    // Stop dedicated threads first
    stopDedicatedThreads(xsink);

    bool do_signal = false;
    {
        AutoLocker al(m);
        shutting_down = true;
        if (tid && !io_exiting) {
            do_signal = enqueueCmdLocked(IoCommand::Quit);
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    waitStop(xsink);

    {
        AutoLocker al(m);
        shutting_down = false;
    }
}

void AsyncIoControllerPriv::stopClear(ExceptionSink* xsink) {
    stop(xsink);
}

bool AsyncIoControllerPriv::waitStop(ExceptionSink* xsink) {
    AutoLocker al(m);
    while (tid) {
        io_waiting = true;
        io_cond.wait(m);
        io_waiting = false;
    }
    return true;
}

bool AsyncIoControllerPriv::waitReady(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);
    if (ready_flag) {
        return true;
    }
    while (!ready_flag && tid && !io_exiting) {
        if (timeout_ms > 0) {
            int rc = io_cond.wait2(m, (int64)timeout_ms);
            if (rc) {
                return ready_flag;
            }
        } else {
            io_cond.wait(m);
        }
    }
    return ready_flag;
}

bool AsyncIoControllerPriv::waitForProcessing(int timeout_ms, ExceptionSink* xsink) {
    AutoLocker al(m);
    if (!tid) {
        return false;
    }
    int target = submit_seq;
    if (processed_seq >= target) {
        return true;
    }
    if (timeout_ms > 0) {
        int64 deadline_us = get_epoch_us() + (int64)timeout_ms * 1000;
        while (processed_seq < target && tid) {
            int64 remaining_us = deadline_us - get_epoch_us();
            if (remaining_us <= 0) {
                return false;
            }
            int remaining_ms = (int)((remaining_us + 999) / 1000);
            int rc = processed_cond.wait2(m, remaining_ms);
            if (rc) {
                return processed_seq >= target;
            }
        }
        return processed_seq >= target;
    }
    // No timeout - wait indefinitely
    while (processed_seq < target && tid) {
        processed_cond.wait(m);
    }
    return processed_seq >= target;
}

bool AsyncIoControllerPriv::running() const {
    AutoLocker al(m);
    return tid != 0 && !io_exiting;
}

int AsyncIoControllerPriv::getCacheSize() const {
    AutoLocker al(m);
    return (int)(cache.size() + dedicated_threads.size());
}

void AsyncIoControllerPriv::setAutostop(bool autostop) {
    AutoLocker al(m);
    autostop_flag = autostop;
}

bool AsyncIoControllerPriv::getAutostop() const {
    AutoLocker al(m);
    return autostop_flag;
}

QoreHashNode* AsyncIoControllerPriv::getInfo(ExceptionSink* xsink) const {
    AutoLocker al(m);
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("running", tid != 0 && !io_exiting, xsink);
    rv->setKeyValue("cache_size", (int64)(cache.size() + dedicated_threads.size()), xsink);
    rv->setKeyValue("autostop", autostop_flag, xsink);
    rv->setKeyValue("shutting_down", shutting_down, xsink);
    rv->setKeyValue("tid", (int64)tid, xsink);
    rv->setKeyValue("dedicated_thread_count", (int64)dedicated_threads.size(), xsink);

    // Build cache key list (includes both main cache and dedicated thread keys)
    ReferenceHolder<QoreListNode> keys(new QoreListNode(stringTypeInfo), xsink);
    for (auto& it : cache) {
        keys->push(new QoreStringNode(it.first), xsink);
    }
    for (auto& [key, dti] : dedicated_threads) {
        keys->push(new QoreStringNode(key), xsink);
    }
    rv->setKeyValue("cache_keys", keys.release(), xsink);

    return rv.release();
}

void AsyncIoControllerPriv::setLogger(QoreObject* logger_obj, ExceptionSink* xsink) {
    if (logger_obj) {
        // Validate the logger object has the required methods
        const QoreClass* cls = logger_obj->getClass();
        if (!cls->findMethod("logArgs")) {
            xsink->raiseException("ASYNC-IO-ERROR",
                "logger object of class '%s' does not implement logArgs() method",
                cls->getName());
            return;
        }
        if (!cls->findMethod("isEnabledFor")) {
            xsink->raiseException("ASYNC-IO-ERROR",
                "logger object of class '%s' does not implement isEnabledFor() method",
                cls->getName());
            return;
        }
    }

    QoreLoggerBridge* old_logger;
    {
        AutoLocker al(m);
        old_logger = logger;
        logger = logger_obj ? new QoreLoggerBridge(logger_obj) : nullptr;
    }
    // Deref old bridge outside lock — destructor may call Qore user code
    if (old_logger) {
        old_logger->deref(xsink);
    }
}

int64_t AsyncIoControllerPriv::addTimer(const DateTimeNode* deadline, QoreValue udata,
        ExceptionSink* xsink) {
    // Pre-allocate ID from atomic counter (no lock needed)
    int64_t id = next_ctrl_timer_id.fetch_add(1);

    // Convert deadline to absolute microseconds since epoch
    int64_t deadline_us = deadline->getEpochMicrosecondsUTC();

    bool do_signal = false;
    {
        AutoLocker al(m);

        if (shutting_down) {
            xsink->raiseException("ASYNC-IO-ERROR", "controller is shutting down");
            return -1;
        }

        // Store user data under lock (add a reference for our storage)
        TimerInfo tinfo;
        tinfo.udata = udata.refSelf();
        timer_info_map[id] = tinfo;

        // Enqueue command
        Command cmd;
        cmd.cmd = IoCommand::AddTimer;
        cmd.timer_deadline_us = deadline_us;
        cmd.timer_id = id;
        cmd.done_cond = nullptr;

        if (io_exiting || !tid) {
            startIntern(xsink);
            if (*xsink) {
                // Clean up our reference on error
                timer_info_map[id].udata.discard(xsink);
                timer_info_map.erase(id);
                return -1;
            }
        }
        cmdq.push_back(cmd);
        do_signal = true;
    }

    if (do_signal) {
        notifier->notify();
    }

    return id;
}

bool AsyncIoControllerPriv::cancelTimer(int64_t id, ExceptionSink* xsink) {
    bool do_signal = false;
    bool found = false;
    QoreValue discard_udata;

    {
        AutoLocker al(m);

        auto it = timer_info_map.find(id);
        if (it == timer_info_map.end()) {
            return false;
        }
        found = true;

        // Always remove from timer_info_map immediately (so a second cancel returns False)
        discard_udata = it->second.udata;
        timer_info_map.erase(it);

        if (tid && !io_exiting) {
            // I/O thread running — enqueue cancel command to remove from EventLoop
            Command cmd;
            cmd.cmd = IoCommand::CancelTimer;
            cmd.timer_id = id;
            cmd.done_cond = nullptr;
            cmd.timer_deadline_us = 0;
            cmdq.push_back(cmd);
            do_signal = true;
        }
    }

    discard_udata.discard(xsink);

    if (do_signal) {
        notifier->notify();
    }

    return found;
}

void AsyncIoControllerPriv::setTimerCallback(ResolvedCallReferenceNode* cb, ExceptionSink* xsink) {
    AutoLocker al(m);
    if (timer_callback) {
        timer_callback->deref(xsink);
    }
    // Takes ownership of the caller's reference
    timer_callback = cb;
}

// --- Internal methods ---

void AsyncIoControllerPriv::startIntern(ExceptionSink* xsink) {
    // Caller must hold lock
    // Wait for any exiting thread to finish
    while (io_exiting) {
        io_waiting = true;
        io_cond.wait(m);
        io_waiting = false;
    }

    // Another thread may have already restarted the I/O thread while we were waiting
    // in the while(io_exiting) loop above (both threads released the lock via io_cond.wait,
    // and the first one to re-acquire it after the broadcast restarted the thread).
    if (tid) {
        return;
    }

    // Validate EventLoop and EventNotifier
    if (!loop || !loop->isValid()) {
        xsink->raiseException("ASYNC-IO-ERROR", "event loop is not valid");
        return;
    }

    if (!notifier || !notifier->isValid()) {
        xsink->raiseException("ASYNC-IO-ERROR", "event notifier is not valid");
        return;
    }

    // Register notifier with event loop
    int ev_flags = QORE_EV_READ;
#ifdef DARWIN
    // macOS: use optimized EVFILT_USER
    int rc = notifier->bindToKqueue(loop->getKqueueFd(), xsink);
    if (rc < 0) {
        return;
    }
    rc = loop->addUserEvent(notifier->getUserIdent(), nullptr, xsink);
    if (rc < 0) {
        notifier->unbindFromKqueue();
        return;
    }
#else
    // Linux/other: register notifier fd
    int rc = loop->add(notifier->fd(), ev_flags, nullptr, xsink);
    if (rc < 0) {
        return;
    }
#endif

    ready_flag = false;
    io_exiting = false;

    // Start I/O thread
    ref();  // Reference for the I/O thread
    tid = q_start_thread(xsink, ioThreadEntry, this);
    if (tid == -1) {
        tid = 0;
        // Cannot call deref() here because the caller holds the lock;
        // deref() -> stop() -> AutoLocker(m) -> deadlock.
        // Since the caller always holds a reference (via QoreObject), this
        // ROdereference() will never be the last ref, so we can safely
        // just undo the ref() above without triggering destruction.
        ROdereference();
        // Remove the notifier from the event loop to undo the loop->add() above.
        // If we don't clean up here, the next startIntern() call will attempt to
        // re-register the same fd and fail with EEXIST.
        ExceptionSink cleanup_xsink;
#ifdef DARWIN
        loop->removeUserEvent(notifier->getUserIdent(), &cleanup_xsink);
        notifier->unbindFromKqueue();
#else
        loop->remove(notifier->fd(), &cleanup_xsink);
#endif
        cleanup_xsink.clear();
        return;
    }
    // NOTE: do not call log() here — caller holds the lock.
    // The I/O thread itself logs "I/O thread ready" when it starts.
}

void AsyncIoControllerPriv::ioThreadEntry(ExceptionSink* xsink, void* arg) {
    AsyncIoControllerPriv* self = static_cast<AsyncIoControllerPriv*>(arg);
    self->ioThread(xsink);
    if (*xsink) {
        // Log exception
        self->log(QORE_LOG_LEVEL_ERROR, "I/O thread exception");
        xsink->clear();
    }
    self->deref(xsink);
}

void AsyncIoControllerPriv::ioThread(ExceptionSink* xsink) {
    // Signal ready
    {
        AutoLocker al(m);
        ready_flag = true;
        io_cond.broadcast();
    }

    log(QORE_LOG_LEVEL_DEBUG, "I/O thread ready");

    // ready_socket_hashes must persist across iterations:
    // Phase 1 checks this set to decide which operations to continuePoll();
    // it is populated at the bottom of each iteration after EventLoop::poll().
    std::unordered_set<std::string> ready_socket_hashes;

    while (true) {
        // Process commands
        if (processCommands(xsink)) {
            break;  // Quit command received
        }

        // --- PHASE 1: Snapshot under lock ---
        struct OpToPoll {
            std::string key;
            QoreObject* spop_obj;  // Not refed - just pointer for Phase 2
            bool timed_out;
        };
        std::vector<OpToPoll> ops_to_poll;
        int64 poll_deadline_us = 0;
        bool forced = false;

        // Helper: apply protocol-level poll timeout hint (e.g., QUIC timer expiry)
        // Returns true if timer already expired (caller should force immediate continuePoll)
        auto apply_poll_timeout = [&poll_deadline_us](const QoreHashNode* poll_info, int64 now_us) -> bool {
            QoreValue ptv = poll_info->getKeyValue("poll_timeout_ms");
            if (ptv.isNullOrNothing()) {
                return false;
            }
            int64 pt_ms = ptv.getAsBigInt();
            if (pt_ms > 0) {
                int64 timer_deadline_us = now_us + pt_ms * 1000;
                if (poll_deadline_us == 0 || timer_deadline_us < poll_deadline_us) {
                    poll_deadline_us = timer_deadline_us;
                }
                return false;
            }
            // pt_ms <= 0: timer already expired
            return true;
        };

        {
            AutoLocker al(m);

            if (force_poll) {
                force_poll = false;
                forced = true;
            }

            int64 now_us = get_epoch_us();

            for (auto& [key, pinfo] : cache) {
                // Initialize timeout_date if not set
                if (pinfo.timeout_date_us == 0 && pinfo.timeout_us >= 0) {
                    pinfo.timeout_date_us = now_us + pinfo.timeout_us;
                }

                // Check for timeout (timeout_us >= 0 means timeout is enabled; < 0 means no timeout)
                if (pinfo.timeout_us >= 0 && pinfo.timeout_date_us > 0 && pinfo.timeout_date_us <= now_us) {
                    ops_to_poll.push_back({key, pinfo.spop_obj, true});
                    continue;
                }

                // Check if we should call continuePoll
                if (pinfo.poll_info && !forced) {
                    // Get the socket from poll_info
                    std::string sock_hash;
                    int events = 0;
                    ExceptionSink poll_xsink;
                    QoreObject* poll_sock = getSocketFromPollInfo(pinfo.poll_info, sock_hash, events,
                        &poll_xsink);
                    if (poll_xsink) {
                        poll_xsink.clear();
                    }

                    // Check if socket was marked ready by the EventLoop
                    if (poll_sock && !ready_socket_hashes.count(sock_hash)) {
                        // Socket not ready - skip continuePoll, update deadline
                        if (pinfo.timeout_us >= 0 && pinfo.timeout_date_us > 0) {
                            if (poll_deadline_us == 0 || pinfo.timeout_date_us < poll_deadline_us) {
                                poll_deadline_us = pinfo.timeout_date_us;
                            }
                        }

                        // Check for protocol-level poll timeout hint (e.g., QUIC timer expiry)
                        if (apply_poll_timeout(pinfo.poll_info, now_us)) {
                            // Timer already expired; force immediate continuePoll
                            ops_to_poll.push_back({key, pinfo.spop_obj, false});
                            continue;
                        }

                        continue;
                    }
                }

                ops_to_poll.push_back({key, pinfo.spop_obj, false});
            }

            // Update processed sequence counter
            processed_seq = submit_seq;
            processed_cond.broadcast();
        }

        // --- PHASE 2: continuePoll outside lock ---
        struct PollResult {
            std::string key;
            QoreHashNode* new_poll_info;  // Refed or nullptr
            QoreHashNode* ex_hash;        // Refed or nullptr
            bool timed_out;
            bool completed;
        };
        std::vector<PollResult> poll_results;

        for (auto& op : ops_to_poll) {
            PollResult result;
            result.key = op.key;
            result.new_poll_info = nullptr;
            result.ex_hash = nullptr;
            result.timed_out = op.timed_out;
            result.completed = false;

            if (op.timed_out) {
                // Create timeout exception hash
                ReferenceHolder<QoreHashNode> ex(new QoreHashNode(hashdeclExceptionInfo, xsink), xsink);
                if (!*xsink) {
                    ex->setKeyValue("err", new QoreStringNode("SOCKET-TIMEOUT"), xsink);
                    ex->setKeyValue("desc", new QoreStringNode("socket operation timed out"), xsink);
                    ex->setKeyValue("type", new QoreStringNode("User"), xsink);
                    result.ex_hash = ex.release();
                }
            } else {
                // Call continuePoll
                ExceptionSink poll_xsink;
                QoreHashNode* new_info = callContinuePoll(op.spop_obj, &poll_xsink);
                if (poll_xsink) {
                    // Capture full exception info (err, desc, arg, callstack, etc.)
                    QoreException* ex_obj = poll_xsink.getException();
                    if (ex_obj) {
                        result.ex_hash = ex_obj->makeExceptionObject();
                    }
                    poll_xsink.clear();
                } else if (!new_info) {
                    result.completed = true;
                } else {
                    result.new_poll_info = new_info;
                }
            }

            poll_results.push_back(std::move(result));
        }

        // --- PHASE 3: Update under lock + deferred delivery ---
        std::vector<DeferredDelivery> deferred_deliveries;
        bool do_autostop = false;

        {
            AutoLocker al(m);

            for (auto& result : poll_results) {
                auto it = cache.find(result.key);
                if (it == cache.end()) {
                    // Operation was canceled during Phase 2
                    if (result.new_poll_info) {
                        result.new_poll_info->deref(xsink);
                    }
                    if (result.ex_hash) {
                        result.ex_hash->deref(xsink);
                    }
                    continue;
                }

                PollInfo& pinfo = it->second;

                if (result.ex_hash || result.timed_out || result.completed) {
                    // Operation finished (error, timeout, or completed)
                    // Clean up extra fds before unregistering main socket
                    unregisterExtraFds(result.key, xsink);
                    unregisterFromEventLoop(result.key, xsink);

                    // Build result hash (buildResultHash does refSelf on ex_hash)
                    QoreHashNode* result_hash = buildResultHash(pinfo, false, result.ex_hash, xsink);
                    // Deref our copy of ex_hash — buildResultHash already refSelf'd it
                    if (result.ex_hash) {
                        result.ex_hash->deref(xsink);
                        result.ex_hash = nullptr;
                    }

                    // Prepare deferred delivery
                    DeferredDelivery dd;
                    dd.key = result.key;
                    dd.queue = pinfo.queue;
                    if (dd.queue) {
                        dd.queue->ref();
                    }
                    dd.callback = pinfo.callback;
                    if (dd.callback) {
                        dd.callback->ref();
                    }
                    dd.result = result_hash;
                    deferred_deliveries.push_back(std::move(dd));

                    // Signal cancel waiters
                    auto cit = cancel_cond_map.find(result.key);
                    if (cit != cancel_cond_map.end()) {
                        cit->second->broadcast();
                        delete cit->second;
                        cancel_cond_map.erase(cit);
                    }

                    // Clean up and remove from cache
                    pinfo.cleanup(xsink);
                    cache.erase(it);
                } else {
                    // Operation still pending - update poll_info
                    if (pinfo.poll_info) {
                        pinfo.poll_info->deref(xsink);
                    }
                    pinfo.poll_info = result.new_poll_info;

                    // Update EventLoop registration
                    if (result.new_poll_info) {
                        std::string sock_hash;
                        int events = 0;
                        QoreObject* poll_sock = getSocketFromPollInfo(result.new_poll_info,
                            sock_hash, events, xsink);
                        if (!*xsink && poll_sock) {
                            updateEventLoopRegistration(result.key, poll_sock, sock_hash, events,
                                xsink);
                            // Update extra fd registrations
                            updateExtraFds(result.key, poll_sock, result.new_poll_info, xsink);
#if defined(__linux__) && defined(HAVE_IO_URING)
                            // Pass io_uring to any Http2Session on this socket (once).
                            // Use a local ExceptionSink to avoid hiding errors from
                            // the caller's xsink.
                            if (loop->getIoUring()) {
                                ExceptionSink uring_xsink;
                                QoreSocketObject* so = static_cast<QoreSocketObject*>(
                                    poll_sock->getReferencedPrivateData(CID_SOCKET,
                                        &uring_xsink));
                                if (so) {
                                    Http2SessionPtr h2s = so->priv->socket->priv->h2_session;
                                    if (h2s && !h2s->hasIoUring()) {
                                        h2s->setIoUring(loop->getIoUring());
                                    }
                                    so->deref(&uring_xsink);
                                }
                                if (uring_xsink) {
                                    uring_xsink.clear();
                                }
                            }
#endif
                        }

                        // Update poll deadline
                        if (pinfo.timeout_us >= 0 && pinfo.timeout_date_us > 0) {
                            if (poll_deadline_us == 0 || pinfo.timeout_date_us < poll_deadline_us) {
                                poll_deadline_us = pinfo.timeout_date_us;
                            }
                        }

                        // Check for protocol-level poll timeout hint (e.g., QUIC timer expiry)
                        if (apply_poll_timeout(result.new_poll_info, get_epoch_us())) {
                            // Timer already expired; force immediate poll on next iteration
                            poll_deadline_us = 1;
                        }
                    }
                }
            }

            // Check autostop (only if no deliveries pending — recheck after delivery)
            if (cache.empty() && dedicated_threads.empty() && autostop_flag && cmdq.empty()
                    && deferred_deliveries.empty()) {
                io_exiting = true;
                if (io_waiting) {
                    io_cond.broadcast();
                }
                do_autostop = true;
            }
        }

        // Deliver results outside lock
        for (auto& dd : deferred_deliveries) {
            if (dd.callback) {
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(dd.result, xsink);
                dd.result = nullptr;

                ExceptionSink cb_xsink;
                ValueHolder rv(dd.callback->execValue(*args, &cb_xsink), &cb_xsink);
                if (cb_xsink) {
                    log(QORE_LOG_LEVEL_ERROR, "callback exception for key '%s'", dd.key.c_str());
                    cb_xsink.clear();
                }
                dd.callback->deref(xsink);
            } else if (dd.queue) {
                dd.queue->push(xsink, dd.result);
                dd.result = nullptr;
            }

            if (dd.result) {
                dd.result->deref(xsink);
            }
            if (dd.queue) {
                dd.queue->deref(xsink);
            }
        }

        // Re-check autostop after deliveries (callbacks may have submitted new ops)
        if (!do_autostop && !deferred_deliveries.empty()) {
            AutoLocker al(m);
            if (cache.empty() && dedicated_threads.empty() && autostop_flag && cmdq.empty()) {
                io_exiting = true;
                if (io_waiting) {
                    io_cond.broadcast();
                }
                do_autostop = true;
            }
        }

        if (do_autostop) {
            log(QORE_LOG_LEVEL_DEBUG, "all operations completed; exiting I/O thread (autostop)");
            break;
        }

        // Calculate poll timeout
        int timeout_ms = -1;  // Wait indefinitely by default
        if (poll_deadline_us > 0) {
            int64 now_us = get_epoch_us();
            int64 remaining_us = poll_deadline_us - now_us;
            if (remaining_us <= 0) {
                timeout_ms = 0;
            } else {
                timeout_ms = (int)(remaining_us / 1000);
                if (timeout_ms == 0) {
                    timeout_ms = 1;  // Minimum 1ms to avoid busy loop
                }
            }
        }

        // Poll for events
        std::vector<QoreEventInfo> events;
        int count = loop->poll(events, timeout_ms, xsink);
        if (*xsink) {
            const QoreStringNode* err = xsink->getExceptionErr().get<const QoreStringNode>();
            const QoreStringNode* desc = xsink->getExceptionDesc().get<const QoreStringNode>();
            log(QORE_LOG_LEVEL_ERROR, "EventLoop::poll() error: %s: %s",
                err ? err->c_str() : "?", desc ? desc->c_str() : "?");
            xsink->clear();
            continue;
        }

        // Build ready socket hash set: always clear first, then populate from poll results
        ready_socket_hashes.clear();

        // Collect timer events and socket events from poll results
        struct TimerEvent {
            int64_t id;
            QoreValue udata;
        };
        std::vector<TimerEvent> timer_events;
        ResolvedCallReferenceNode* cb_snapshot = nullptr;

        if (count > 0) {
            AutoLocker al(m);
            for (int i = 0; i < count; ++i) {
                if (events[i].events & QORE_EV_TIMER) {
                    // Timer event — extract user data under lock
                    // If the entry is not found, it was already canceled by cancelTimer();
                    // silently drop the event to avoid delivering a stale callback
                    int64_t tid_val = events[i].timer_id;
                    auto it = timer_info_map.find(tid_val);
                    if (it != timer_info_map.end()) {
                        timer_events.push_back({tid_val, it->second.udata});
                        timer_info_map.erase(it);
                    }
                } else if (events[i].fd >= 0 && events[i].udata) {
                    QoreObject* obj = static_cast<QoreObject*>(events[i].udata);
                    // Find the socket hash for this object
                    for (auto& [key, sock_obj] : registered_sockets) {
                        if (sock_obj == obj) {
                            QoreSocketObject* s = static_cast<QoreSocketObject*>(
                                obj->getReferencedPrivateData(CID_SOCKET, xsink));
                            if (s) {
                                ready_socket_hashes.insert(getSocketHash(s));
                                s->deref(xsink);
                            }
                            break;
                        }
                    }
                }
            }
            // Snapshot timer callback under lock
            if (!timer_events.empty() && timer_callback) {
                cb_snapshot = timer_callback;
                cb_snapshot->ref();
            }
        }

        // Deliver timer events outside lock
        for (auto& te : timer_events) {
            if (cb_snapshot) {
                // Build TimerEventInfo hash
                ReferenceHolder<QoreHashNode> timer_hash(
                    new QoreHashNode(hashdeclTimerEventInfo, xsink), xsink);
                if (!*xsink) {
                    timer_hash->setKeyValue("id", te.id, xsink);
                    if (te.udata.hasNode()) {
                        timer_hash->setKeyValue("udata", te.udata.refSelf(), xsink);
                    }
                    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                    args->push(timer_hash.release(), xsink);

                    ExceptionSink cb_xsink;
                    ValueHolder rv(cb_snapshot->execValue(*args, &cb_xsink), &cb_xsink);
                    if (cb_xsink) {
                        log(QORE_LOG_LEVEL_ERROR, "timer callback exception for timer %lld",
                            (long long)te.id);
                        cb_xsink.clear();
                    }
                }
            }
            te.udata.discard(xsink);
        }
        if (cb_snapshot) {
            cb_snapshot->deref(xsink);
        }

#if defined(__linux__) && defined(HAVE_IO_URING)
        // Process io_uring completions for async file reads
        // NOTE: always call processCompletions() when uring exists, not just when
        // getPendingCount() > 0 — cancel operations (cancelStream) reset the weak_ptr
        // in PendingRead but don't erase the entry or decrement pending_count until
        // processCompletions() sees the CQE.  Cancel SQEs and expired-session CQEs
        // still fire the eventfd.  If we skip draining the eventfd, epoll_wait returns
        // immediately on every iteration, causing a busy loop.
        QoreIoUring* uring = loop->getIoUring();
        if (uring) {
            std::vector<QoreIoUring::CompletedRead> completions;
            uring->processCompletions(completions);
            for (auto& c : completions) {
                ExceptionSink uring_xsink;
                // Save data pointer before moving buffer — c.data points into
                // c.buffer and C++ does not guarantee argument evaluation order
                const char* data = c.data;
                c.session->handleAsyncReadCompletion(
                    c.stream_id, data, c.length, c.error,
                    std::move(c.buffer), &uring_xsink);
                if (uring_xsink) {
                    // Log and clear — don't let one stream's error kill the loop
                    const QoreStringNode* err = uring_xsink.getExceptionErr()
                        .get<const QoreStringNode>();
                    const QoreStringNode* desc = uring_xsink.getExceptionDesc()
                        .get<const QoreStringNode>();
                    log(QORE_LOG_LEVEL_ERROR,
                        "io_uring completion error (stream %d): %s: %s",
                        c.stream_id, err ? err->c_str() : "?",
                        desc ? desc->c_str() : "?");
                    uring_xsink.clear();
                }
            }
        }
#endif
    }

    // Cleanup on exit
    {
        AutoLocker al(m);

        // Remove notifier from event loop
#ifdef DARWIN
        loop->removeUserEvent(notifier->getUserIdent(), xsink);
        notifier->unbindFromKqueue();
#else
        loop->remove(notifier->fd(), xsink);
#endif

        // Clear all registrations
        for (auto& [key, sock_obj] : registered_sockets) {
            if (sock_obj) {
                sock_obj->deref(xsink);
            }
        }
        registered_sockets.clear();
        registered_events.clear();
        registered_fds.clear();
        key_events.clear();
        sock_hash_to_keys.clear();
        socket_refcounts.clear();

        // Clean up any remaining timers
        for (auto& [id, tinfo] : timer_info_map) {
            loop->cancelTimer(id);
            tinfo.udata.discard(xsink);
        }
        timer_info_map.clear();

        // Signal any pending CancelOwner done_cond waiters in the command queue
        // so cancelByOwner() doesn't hang when the I/O thread exits.
        // Must set cancel_done_flag under lock before broadcasting to prevent
        // use-after-free of the stack-allocated done_cond.
        for (auto& pending_cmd : cmdq) {
            if (pending_cmd.done_cond) {
                if (pending_cmd.cancel_done_flag) {
                    *pending_cmd.cancel_done_flag = true;
                }
                pending_cmd.done_cond->broadcast();
            }
        }
        cmdq.clear();
    }

    // Log the exit message before signaling waitStop(), so the per-controller
    // logger receives this message instead of the global logger
    log(QORE_LOG_LEVEL_DEBUG, "I/O thread exited");

    {
        AutoLocker al(m);
        tid = 0;
        io_exiting = false;
        ready_flag = false;
        processed_seq = 0;
        submit_seq = 0;
        processed_cond.broadcast();
        if (io_waiting) {
            io_cond.broadcast();
        }
    }
}

bool AsyncIoControllerPriv::processCommands(ExceptionSink* xsink) {
    // Double-check loop to handle race between enqueue and notify acknowledge
    while (true) {
        while (true) {
            Command cmd;
            {
                AutoLocker al(m);
                if (cmdq.empty()) {
                    break;
                }
                cmd = cmdq.front();
                cmdq.pop_front();
            }

            switch (cmd.cmd) {
                case IoCommand::Quit: {
                    // Collect PollInfo copies and cancel conditions under lock
                    std::vector<PollInfo> quit_pinfos;
                    std::vector<QoreCondition*> quit_conds;
                    {
                        AutoLocker al(m);
                        std::vector<std::string> keys;
                        for (auto& [key, pinfo] : cache) {
                            keys.push_back(key);
                        }

                        for (auto& key : keys) {
                            auto it = cache.find(key);
                            if (it != cache.end()) {
                                unregisterFromEventLoop(key, xsink);
                                // Move PollInfo out of cache
                                quit_pinfos.push_back(it->second);
                                it->second = PollInfo();
                                cache.erase(it);

                                auto cit = cancel_cond_map.find(key);
                                if (cit != cancel_cond_map.end()) {
                                    quit_conds.push_back(cit->second);
                                    cancel_cond_map.erase(cit);
                                } else {
                                    quit_conds.push_back(nullptr);
                                }
                            }
                        }

                        io_exiting = true;
                        if (io_waiting) {
                            io_cond.broadcast();
                        }
                    }

                    // Deliver cancel results outside lock (prevents callback deadlock)
                    for (size_t i = 0; i < quit_pinfos.size(); ++i) {
                        doCancelIntern(quit_pinfos[i], xsink);
                        quit_pinfos[i].cleanup(xsink);
                        if (quit_conds[i]) {
                            quit_conds[i]->broadcast();
                            delete quit_conds[i];
                        }
                    }
                    return true;
                }

                case IoCommand::Cancel: {
                    PollInfo pinfo_copy;
                    QoreCondition* cond = nullptr;
                    bool found = false;

                    {
                        AutoLocker al(m);
                        auto it = cache.find(cmd.key);
                        if (it != cache.end()) {
                            found = true;
                            unregisterFromEventLoop(cmd.key, xsink);
                            pinfo_copy = it->second;
                            // Clear the cache entry without cleanup (we'll do it after delivery)
                            it->second = PollInfo();
                            cache.erase(it);

                            auto cit = cancel_cond_map.find(cmd.key);
                            if (cit != cancel_cond_map.end()) {
                                cond = cit->second;
                                cancel_cond_map.erase(cit);
                            }
                        }
                    }

                    // Deliver canceled result BEFORE broadcasting (ensures socket released first)
                    if (found) {
                        doCancelIntern(pinfo_copy, xsink);
                        pinfo_copy.cleanup(xsink);
                    }
                    if (cond) {
                        cond->broadcast();
                        delete cond;
                    }
                    break;
                }

                case IoCommand::CancelOwner: {
                    // Find all operations for this owner
                    std::vector<std::string> keys;
                    {
                        AutoLocker al(m);
                        for (auto& [key, pinfo] : cache) {
                            if (pinfo.owner == cmd.owner) {
                                keys.push_back(key);
                            }
                        }
                    }

                    // Cancel each one
                    for (auto& key : keys) {
                        PollInfo pinfo_copy;
                        QoreCondition* cond = nullptr;
                        bool found = false;

                        {
                            AutoLocker al(m);
                            auto it = cache.find(key);
                            if (it != cache.end() && it->second.owner == cmd.owner) {
                                found = true;
                                unregisterFromEventLoop(key, xsink);
                                pinfo_copy = it->second;
                                it->second = PollInfo();
                                cache.erase(it);

                                auto cit = cancel_cond_map.find(key);
                                if (cit != cancel_cond_map.end()) {
                                    cond = cit->second;
                                    cancel_cond_map.erase(cit);
                                }
                            }
                        }

                        if (found) {
                            doCancelIntern(pinfo_copy, xsink);
                            pinfo_copy.cleanup(xsink);
                        }
                        if (cond) {
                            cond->broadcast();
                            delete cond;
                        }
                    }

                    // Signal done — set cancel_done_flag under lock BEFORE
                    // broadcasting to prevent a race where a spurious wakeup
                    // causes the caller to exit and destroy done_cond before
                    // we broadcast on it.
                    if (cmd.done_cond) {
                        AutoLocker al(m);
                        if (cmd.cancel_done_flag) {
                            *cmd.cancel_done_flag = true;
                        }
                        cmd.done_cond->broadcast();
                    }
                    break;
                }

                case IoCommand::Wake: {
                    AutoLocker al(m);
                    force_poll = true;
                    break;
                }

                case IoCommand::AddTimer: {
                    // Register timer in EventLoop with pre-allocated ID
                    loop->addTimer(cmd.timer_deadline_us, nullptr, cmd.timer_id);
                    break;
                }

                case IoCommand::CancelTimer: {
                    // Cancel timer in EventLoop; user data was already cleaned up
                    // in cancelTimer() when the entry was removed from timer_info_map
                    loop->cancelTimer(cmd.timer_id);
                    break;
                }

                case IoCommand::Add:
                    // No-op - operation was already added to cache in submit()
                    break;
            }
        }

        // Acknowledge notifier
        notifier->acknowledge(xsink);
        if (*xsink) {
            xsink->clear();
        }

        // Check if new commands arrived during acknowledge
        {
            AutoLocker al(m);
            if (cmdq.empty()) {
                break;
            }
        }
    }

    return false;
}

void AsyncIoControllerPriv::doCancelIntern(PollInfo& pinfo, ExceptionSink* xsink) {
    // Call abort on the poll operation
    callAbort(pinfo.spop_obj, xsink);

    // Build result hash
    QoreHashNode* result = buildResultHash(pinfo, true, nullptr, xsink);

    // Deliver result
    if (pinfo.callback) {
        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
        args->push(result, xsink);

        ExceptionSink cb_xsink;
        ValueHolder rv(pinfo.callback->execValue(*args, &cb_xsink), &cb_xsink);
        if (cb_xsink) {
            log(QORE_LOG_LEVEL_ERROR, "cancel callback exception");
            cb_xsink.clear();
        }
    } else if (pinfo.queue) {
        pinfo.queue->push(xsink, result);
    } else {
        result->deref(xsink);
    }
}

void AsyncIoControllerPriv::updateEventLoopRegistration(const std::string& key,
        QoreObject* socket, const std::string& sock_hash, int events, ExceptionSink* xsink) {
    // Convert SOCK_POLLIN/SOCK_POLLOUT to QORE_EV_READ/QORE_EV_WRITE
    int ev_flags = 0;
    if (events & SOCK_POLLIN) {
        ev_flags |= QORE_EV_READ;
    }
    if (events & SOCK_POLLOUT) {
        ev_flags |= QORE_EV_WRITE;
    }

    // Track per-key events
    key_events[key] = ev_flags;

    // Get previous socket for this key
    auto prev_it = registered_sockets.find(key);
    QoreObject* prev_sock = prev_it != registered_sockets.end() ? prev_it->second : nullptr;
    std::string prev_sock_hash;
    if (prev_sock) {
        QoreSocketObject* ps = static_cast<QoreSocketObject*>(
            prev_sock->getReferencedPrivateData(CID_SOCKET, xsink));
        if (ps) {
            prev_sock_hash = getSocketHash(ps);
            ps->deref(xsink);
        }
    }

    if (prev_sock && prev_sock_hash == sock_hash) {
        // Same socket object - check if underlying fd changed (e.g., reconnection to
        // a different host during multi-step poll operations like OAuth2 token refresh)
        bool fd_changed = false;
        QoreSocketObject* s = static_cast<QoreSocketObject*>(
            socket->getReferencedPrivateData(CID_SOCKET, xsink));
        if (s) {
            int curr_fd = s->getSocket();
            auto fd_it = registered_fds.find(sock_hash);
            if (fd_it != registered_fds.end() && fd_it->second != curr_fd) {
                fd_changed = true;
                printd(2, "AsyncIoControllerPriv::updateEventLoopRegistration() "
                    "fd changed for socket '%s': %d -> %d\n",
                    sock_hash.c_str(), fd_it->second, curr_fd);
                // Remove old fd from EventLoop — on Linux, epoll auto-removes
                // closed fds, but we must also clean up fd_map; remove()
                // silently handles EBADF/ENOENT from already-closed fds
                loop->remove(fd_it->second, xsink);
                // Add new fd to EventLoop
                int union_events = computeEventUnion(sock_hash);
                loop->add(curr_fd, union_events, socket, xsink);
                registered_fds[sock_hash] = curr_fd;
                registered_events[sock_hash] = union_events;
            }
            s->deref(xsink);
        }
        if (!fd_changed) {
            applyEventUnion(socket, sock_hash, xsink);
        }
        return;
    }

    // Different socket (or new registration)
    if (prev_sock) {
        // Remove key from previous socket's reverse index
        auto sit = sock_hash_to_keys.find(prev_sock_hash);
        if (sit != sock_hash_to_keys.end()) {
            sit->second.erase(key);
            if (sit->second.empty()) {
                sock_hash_to_keys.erase(sit);
            }
        }

        // Decrement refcount for previous socket
        auto rit = socket_refcounts.find(prev_sock_hash);
        if (rit != socket_refcounts.end()) {
            if (rit->second <= 1) {
                // Last reference - remove from EventLoop using the registered fd
                auto fd_it = registered_fds.find(prev_sock_hash);
                if (fd_it != registered_fds.end()) {
                    loop->remove(fd_it->second, xsink);
                } else {
                    QoreSocketObject* ps = static_cast<QoreSocketObject*>(
                        prev_sock->getReferencedPrivateData(CID_SOCKET, xsink));
                    if (ps) {
                        loop->remove(ps->getSocket(), xsink);
                        ps->deref(xsink);
                    }
                }
                registered_events.erase(prev_sock_hash);
                registered_fds.erase(prev_sock_hash);
                socket_refcounts.erase(rit);
            } else {
                rit->second--;
                applyEventUnion(prev_sock, prev_sock_hash, xsink);
            }
        }

        prev_sock->deref(xsink);
    }

    // Add key to new socket's reverse index
    sock_hash_to_keys[sock_hash].insert(key);
    socket->ref();
    registered_sockets[key] = socket;

    // Increment refcount for new socket
    auto rit = socket_refcounts.find(sock_hash);
    if (rit == socket_refcounts.end() || rit->second == 0) {
        // First registration
        int union_events = computeEventUnion(sock_hash);
        QoreSocketObject* s = static_cast<QoreSocketObject*>(
            socket->getReferencedPrivateData(CID_SOCKET, xsink));
        if (s) {
            int fd = s->getSocket();
            loop->add(fd, union_events, socket, xsink);
            registered_fds[sock_hash] = fd;
            s->deref(xsink);
        }
        socket_refcounts[sock_hash] = 1;
        registered_events[sock_hash] = union_events;
    } else {
        socket_refcounts[sock_hash]++;
        applyEventUnion(socket, sock_hash, xsink);
    }
}

void AsyncIoControllerPriv::unregisterFromEventLoop(const std::string& key, ExceptionSink* xsink) {
    key_events.erase(key);

    auto prev_it = registered_sockets.find(key);
    if (prev_it == registered_sockets.end()) {
        return;
    }

    QoreObject* prev_sock = prev_it->second;
    std::string prev_sock_hash;
    QoreSocketObject* ps = static_cast<QoreSocketObject*>(
        prev_sock->getReferencedPrivateData(CID_SOCKET, xsink));
    if (ps) {
        prev_sock_hash = getSocketHash(ps);
        ps->deref(xsink);
    }

    registered_sockets.erase(prev_it);

    if (!prev_sock_hash.empty()) {
        // Remove key from reverse index
        auto sit = sock_hash_to_keys.find(prev_sock_hash);
        if (sit != sock_hash_to_keys.end()) {
            sit->second.erase(key);
            if (sit->second.empty()) {
                sock_hash_to_keys.erase(sit);
            }
        }

        // Decrement refcount
        auto rit = socket_refcounts.find(prev_sock_hash);
        if (rit != socket_refcounts.end()) {
            if (rit->second <= 1) {
                // Last reference - remove from EventLoop using the registered fd
                auto fd_it = registered_fds.find(prev_sock_hash);
                if (fd_it != registered_fds.end()) {
                    // Verify the socket still owns this fd before removing from epoll.
                    // When a socket is closed, its fd is released and may be recycled
                    // by a new socket.  Calling remove() on a recycled fd would
                    // accidentally deregister the new socket from epoll.
                    bool should_remove = true;
                    QoreSocketObject* check_sock = static_cast<QoreSocketObject*>(
                        prev_sock->getReferencedPrivateData(CID_SOCKET, xsink));
                    if (check_sock) {
                        int current_fd = check_sock->getSocket();
                        if (current_fd < 0 || current_fd != fd_it->second) {
                            // Socket closed or fd changed — skip remove; epoll
                            // auto-removed the old fd on close
                            should_remove = false;
                            printd(2, "AsyncIoControllerPriv::unregisterFromEventLoop() "
                                "fd reuse detected for '%s': registered=%d current=%d, "
                                "skipping EventLoop remove\n",
                                prev_sock_hash.c_str(), fd_it->second, current_fd);
                        }
                        check_sock->deref(xsink);
                    }
                    if (should_remove) {
                        loop->remove(fd_it->second, xsink);
                    }
                }
                registered_events.erase(prev_sock_hash);
                registered_fds.erase(prev_sock_hash);
                socket_refcounts.erase(rit);
            } else {
                rit->second--;
                applyEventUnion(prev_sock, prev_sock_hash, xsink);
            }
        }
    }

    prev_sock->deref(xsink);
}

int AsyncIoControllerPriv::computeEventUnion(const std::string& sock_hash) const {
    int result = 0;
    auto it = sock_hash_to_keys.find(sock_hash);
    if (it != sock_hash_to_keys.end()) {
        for (auto& key : it->second) {
            auto eit = key_events.find(key);
            if (eit != key_events.end()) {
                result |= eit->second;
            }
        }
    }
    return result;
}

void AsyncIoControllerPriv::applyEventUnion(QoreObject* socket, const std::string& sock_hash,
        ExceptionSink* xsink) {
    int union_events = computeEventUnion(sock_hash);
    auto it = registered_events.find(sock_hash);
    int prev_events = it != registered_events.end() ? it->second : 0;
    if (union_events != prev_events) {
        QoreSocketObject* s = static_cast<QoreSocketObject*>(
            socket->getReferencedPrivateData(CID_SOCKET, xsink));
        if (s) {
            loop->modify(s->getSocket(), union_events, xsink);
            s->deref(xsink);
        }
        registered_events[sock_hash] = union_events;
    }
}

void AsyncIoControllerPriv::updateExtraFds(const std::string& key, QoreObject* socket,
        QoreHashNode* poll_info, ExceptionSink* xsink) {
    std::unordered_set<int> new_fds;

    // Parse extra_fds from poll_info
    QoreValue v = poll_info->getKeyValue("extra_fds");
    if (v.getType() == NT_LIST) {
        QoreListNode* list = v.get<QoreListNode>();
        ConstListIterator li(list);
        while (li.next()) {
            QoreHashNode* h = li.getValue().get<QoreHashNode>();
            int fd = (int)h->getKeyValue("fd").getAsBigInt();
            new_fds.insert(fd);
        }
    }

    auto& prev_fds = key_extra_fds[key];

    // Remove stale fds (were registered before, not in new set)
    for (int fd : prev_fds) {
        if (!new_fds.count(fd)) {
            loop->remove(fd, xsink);
        }
    }

    // Add new fds (not previously registered)
    // Use the Socket QoreObject* as udata so the event dispatch code
    // marks the same socket hash as ready, waking up continuePoll()
    // Collect fds that fail to add in a separate set to avoid erasing
    // from new_fds during iteration (undefined behavior with unordered_set)
    std::vector<int> failed_fds;
    for (int fd : new_fds) {
        if (!prev_fds.count(fd)) {
            int ev_flags = QORE_EV_READ;  // InputStreams only need read
            loop->add(fd, ev_flags, socket, xsink);
            if (*xsink) {
                // Non-fatal: the fd might not be epoll-compatible (e.g. regular file on Linux)
                // The I/O loop still drives streaming via POLLOUT on the socket fd
                printd(2, "updateExtraFds() failed to add fd %d to event loop; skipping\n", fd);
                xsink->clear();
                failed_fds.push_back(fd);
            }
        }
    }
    for (int fd : failed_fds) {
        new_fds.erase(fd);
    }

    if (new_fds.empty()) {
        key_extra_fds.erase(key);
    } else {
        prev_fds = std::move(new_fds);
    }
}

void AsyncIoControllerPriv::unregisterExtraFds(const std::string& key, ExceptionSink* xsink) {
    auto it = key_extra_fds.find(key);
    if (it != key_extra_fds.end()) {
        for (int fd : it->second) {
            loop->remove(fd, xsink);
        }
        key_extra_fds.erase(it);
    }
}

bool AsyncIoControllerPriv::enqueueCmdLocked(IoCommand cmd, const std::string& key,
        const std::string& owner, QoreCondition* done_cond, bool* cancel_done_flag) {
    // Caller must hold lock
    if (io_exiting || !tid) {
        // Need to restart
        ExceptionSink xsink;
        startIntern(&xsink);
        if (xsink) {
            xsink.clear();
            return false;
        }
    }
    Command c;
    c.cmd = cmd;
    c.key = key;
    c.owner = owner;
    c.done_cond = done_cond;
    c.cancel_done_flag = cancel_done_flag;
    cmdq.push_back(std::move(c));
    return true;
}

void AsyncIoControllerPriv::waitCancel(const std::string& key) {
    AutoLocker al(m);
    while (cancel_cond_map.count(key)) {
        cancel_cond_map[key]->wait(m);
    }
}

void AsyncIoControllerPriv::log(int level, const char* fmt, ...) const {
    // Snapshot and ref the logger under lock, then release the lock before calling
    // any user-provided methods (isEnabledFor, logArgs) to avoid deadlock if the
    // logger re-enters the controller.
    QoreLoggerBridge* lgr;
    bool use_global = false;
    {
        AutoLocker al(m);
        if (!logger) {
            use_global = true;
        } else {
            lgr = logger;
            lgr->ref();
        }
    }

    if (use_global) {
        // No per-controller logger — delegate to global async I/O logger (outside lock)
        va_list args;
        va_start(args, fmt);
        qore_async_io_log_v(level, fmt, args);
        va_end(args);
        return;
    }

    if (!lgr->isEnabledFor(level)) {
        ExceptionSink xsink;
        lgr->deref(&xsink);
        return;
    }

    va_list args;
    va_start(args, fmt);
    QoreStringNode* msg = new QoreStringNode();
    msg->vsprintf(fmt, args);
    va_end(args);

    ExceptionSink xsink;
    lgr->logArgs(level, msg, nullptr, &xsink);
    msg->deref();
    lgr->deref(&xsink);
    if (xsink) {
        xsink.clear();
    }
}

std::string AsyncIoControllerPriv::getSocketHash(QoreSocketObject* sock) {
    QoreString str;
    qore_get_ptr_hash(str, sock);
    return std::string(str.c_str());
}

QoreObject* AsyncIoControllerPriv::getSocketFromPollInfo(QoreHashNode* poll_info,
        std::string& sock_hash, int& events, ExceptionSink* xsink) {
    if (!poll_info) {
        return nullptr;
    }

    QoreValue v = poll_info->getKeyValue("socket");
    QoreObject* obj = v.getType() == NT_OBJECT ? v.get<QoreObject>() : nullptr;
    if (!obj) {
        return nullptr;
    }

    v = poll_info->getKeyValue("events");
    events = (int)v.getAsBigInt();

    QoreSocketObject* s = static_cast<QoreSocketObject*>(
        obj->getReferencedPrivateData(CID_SOCKET, xsink));
    if (s) {
        sock_hash = getSocketHash(s);
        s->deref(xsink);
    }

    return obj;
}

QoreHashNode* AsyncIoControllerPriv::callContinuePoll(QoreObject* spop_obj, ExceptionSink* xsink) {
    ValueHolder rv(spop_obj->evalMethod("continuePoll", nullptr, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    if (rv->getType() == NT_HASH) {
        return rv.release().get<QoreHashNode>();
    }
    return nullptr;
}

void AsyncIoControllerPriv::callAbort(QoreObject* spop_obj, ExceptionSink* xsink) {
    ValueHolder rv(spop_obj->evalMethod("abort", nullptr, xsink), xsink);
    if (*xsink) {
        xsink->clear();
    }
}

QoreHashNode* AsyncIoControllerPriv::buildResultHash(PollInfo& pinfo, bool canceled,
        QoreHashNode* ex_hash, ExceptionSink* xsink) {
    // use a temporary ExceptionSink for hash construction to avoid interference from pre-existing exceptions
    ExceptionSink hash_xsink;
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(hashdeclSocketPollResultInfo, &hash_xsink), xsink);
    if (hash_xsink) {
        xsink->assimilate(hash_xsink);
        return nullptr;
    }
    if (pinfo.sock_obj) {
        result->setKeyValue("sock", pinfo.sock_obj->refSelf(), xsink);
    }
    if (pinfo.spop_obj) {
        result->setKeyValue("spop", pinfo.spop_obj->refSelf(), xsink);
    }
    if (canceled) {
        result->setKeyValue("canceled", true, xsink);
    }
    if (ex_hash) {
        result->setKeyValue("ex", ex_hash->refSelf(), xsink);
    }
    if (pinfo.other) {
        result->setKeyValue("other", pinfo.other->refSelf(), xsink);
    }
    return result.release();
}

// --- Dedicated thread support ---

void AsyncIoControllerPriv::spawnDedicatedThread(DedicatedThreadInfo* dti, ExceptionSink* xsink) {
    // Create EventNotifier for this dedicated thread
    dti->notifier = new QoreEventNotifier(xsink);
    if (*xsink) {
        return;
    }

    // Add to dedicated_threads map under lock
    {
        AutoLocker al(m);
        if (shutting_down) {
            dti->notifier->deref(xsink);
            dti->notifier = nullptr;
            xsink->raiseException("ASYNC-IO-ERROR", "controller is shutting down");
            return;
        }
        dedicated_threads[dti->key] = dti;
    }

    // Reference the controller for the dedicated thread
    ref();

    int new_tid = q_start_thread(xsink, dedicatedThreadEntry, dti, DEDICATED_THREAD_STACK_SIZE);
    if (new_tid == -1) {
        ROdereference();  // Undo the ref() — caller holds a ref, so this is safe
        AutoLocker al(m);
        dedicated_threads.erase(dti->key);
        dti->notifier->deref(xsink);
        dti->notifier = nullptr;
        return;
    }

    {
        AutoLocker al(m);
        dti->tid = new_tid;
    }
}

void AsyncIoControllerPriv::dedicatedThreadEntry(ExceptionSink* xsink, void* arg) {
    DedicatedThreadInfo* dti = static_cast<DedicatedThreadInfo*>(arg);
    AsyncIoControllerPriv* ctrl = dti->controller;
    ctrl->dedicatedThread(dti, xsink);
    if (*xsink) {
        ctrl->log(QORE_LOG_LEVEL_ERROR, "dedicated thread exception for key '%s'", dti->key.c_str());
        xsink->clear();
    }
    ctrl->deref(xsink);
}

QoreHashNode* AsyncIoControllerPriv::callContinuePollDedicated(DedicatedThreadInfo* dti,
        ExceptionSink* xsink) {
    if (dti->has_qore_override) {
        // Dispatch to Qore worker thread pool
        return call_dispatcher->dispatchContinuePoll(dti->pinfo.spop_obj, xsink);
    }
    // Direct C++ call — no interpreter overhead
    return dti->spop_base->continuePoll(xsink);
}

void AsyncIoControllerPriv::dedicatedThread(DedicatedThreadInfo* dti, ExceptionSink* xsink) {
    // Create a private EventLoop for this thread
    dti->loop = new QoreEventLoop(xsink);
    if (*xsink) {
        goto cleanup;
    }

    // Register notifier with EventLoop
    {
        int ev_flags = QORE_EV_READ;
#ifdef DARWIN
        int rc = dti->notifier->bindToKqueue(dti->loop->getKqueueFd(), xsink);
        if (rc < 0) {
            goto cleanup;
        }
        rc = dti->loop->addUserEvent(dti->notifier->getUserIdent(), nullptr, xsink);
        if (rc < 0) {
            dti->notifier->unbindFromKqueue();
            goto cleanup;
        }
#else
        int rc = dti->loop->add(dti->notifier->fd(), ev_flags, nullptr, xsink);
        if (rc < 0) {
            goto cleanup;
        }
#endif
    }

    // Initialize timeout
    if (dti->pinfo.timeout_us >= 0) {
        dti->pinfo.timeout_date_us = get_epoch_us() + dti->pinfo.timeout_us;
    }

    log(QORE_LOG_LEVEL_DEBUG, "dedicated thread started for key '%s'", dti->key.c_str());

    // Main event loop
    while (!dti->stop_requested.load(std::memory_order_relaxed)) {
        // Check timeout
        int64 now_us = get_epoch_us();
        if (dti->pinfo.timeout_us >= 0 && dti->pinfo.timeout_date_us > 0
                && dti->pinfo.timeout_date_us <= now_us) {
            // Timeout — build timeout exception hash and deliver result
            ReferenceHolder<QoreHashNode> ex(new QoreHashNode(hashdeclExceptionInfo, xsink), xsink);
            if (!*xsink) {
                ex->setKeyValue("err", new QoreStringNode("SOCKET-TIMEOUT"), xsink);
                ex->setKeyValue("desc", new QoreStringNode("socket operation timed out"), xsink);
                ex->setKeyValue("type", new QoreStringNode("User"), xsink);
            }
            QoreHashNode* result_hash = buildResultHash(dti->pinfo, false,
                *xsink ? nullptr : ex.release(), xsink);
            if (result_hash) {
                // Remove from map BEFORE delivering result (prevents key conflict on re-submit)
                {
                    AutoLocker al(m);
                    dedicated_threads.erase(dti->key);
                    dti->exited = true;
                    // Broadcast immediately so cancelDedicatedThread() can proceed
                    // without waiting for cleanup (prevents mutex starvation deadlock)
                    dti->exit_cond.broadcast();
                }
                // Deliver result
                if (dti->pinfo.callback) {
                    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                    args->push(result_hash, xsink);
                    ExceptionSink cb_xsink;
                    ValueHolder rv(dti->pinfo.callback->execValue(*args, &cb_xsink), &cb_xsink);
                    if (cb_xsink) {
                        log(QORE_LOG_LEVEL_ERROR, "callback exception for dedicated key '%s'",
                            dti->key.c_str());
                        cb_xsink.clear();
                    }
                } else if (dti->pinfo.queue) {
                    dti->pinfo.queue->push(xsink, result_hash);
                } else {
                    result_hash->deref(xsink);
                }
            }
            goto cleanup_after_remove;
        }

        // Call continuePoll
        ExceptionSink poll_xsink;
        QoreHashNode* new_poll_info = callContinuePollDedicated(dti, &poll_xsink);

        if (poll_xsink) {
            // Error — extract full exception info and deliver result
            QoreException* ex_obj = poll_xsink.getException();
            QoreHashNode* ex_hash = ex_obj ? ex_obj->makeExceptionObject() : nullptr;
            poll_xsink.clear();

            QoreHashNode* result_hash = buildResultHash(dti->pinfo, false, ex_hash, xsink);
            if (result_hash) {
                {
                    AutoLocker al(m);
                    dedicated_threads.erase(dti->key);
                    dti->exited = true;
                    dti->exit_cond.broadcast();
                }
                if (dti->pinfo.callback) {
                    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                    args->push(result_hash, xsink);
                    ExceptionSink cb_xsink;
                    ValueHolder rv(dti->pinfo.callback->execValue(*args, &cb_xsink), &cb_xsink);
                    if (cb_xsink) {
                        cb_xsink.clear();
                    }
                } else if (dti->pinfo.queue) {
                    dti->pinfo.queue->push(xsink, result_hash);
                } else {
                    result_hash->deref(xsink);
                }
            }
            goto cleanup_after_remove;
        }

        if (!new_poll_info) {
            // Operation completed (goal reached) — deliver success result
            QoreHashNode* result_hash = buildResultHash(dti->pinfo, false, nullptr, xsink);
            if (result_hash) {
                {
                    AutoLocker al(m);
                    dedicated_threads.erase(dti->key);
                    dti->exited = true;
                    dti->exit_cond.broadcast();
                }
                if (dti->pinfo.callback) {
                    ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                    args->push(result_hash, xsink);
                    ExceptionSink cb_xsink;
                    ValueHolder rv(dti->pinfo.callback->execValue(*args, &cb_xsink), &cb_xsink);
                    if (cb_xsink) {
                        log(QORE_LOG_LEVEL_ERROR,
                            "callback exception for dedicated key '%s' (goal reached)",
                            dti->key.c_str());
                        cb_xsink.clear();
                    }
                } else if (dti->pinfo.queue) {
                    dti->pinfo.queue->push(xsink, result_hash);
                } else {
                    result_hash->deref(xsink);
                }
            }
            goto cleanup_after_remove;
        }

        // Update poll_info
        if (dti->pinfo.poll_info) {
            dti->pinfo.poll_info->deref(xsink);
        }
        dti->pinfo.poll_info = new_poll_info;

        // Get socket info from new poll_info and update EventLoop registration
        std::string sock_hash;
        int events = 0;
        QoreObject* poll_sock = getSocketFromPollInfo(new_poll_info, sock_hash, events, xsink);
        if (*xsink) {
            log(QORE_LOG_LEVEL_ERROR, "dedicated key '%s': getSocketFromPollInfo error",
                dti->key.c_str());
            xsink->clear();
        }

        if (poll_sock) {
            // Register/update the socket with the dedicated EventLoop
            QoreSocketObject* s = static_cast<QoreSocketObject*>(
                poll_sock->getReferencedPrivateData(CID_SOCKET, xsink));
            if (s) {
                int fd = s->getSocket();
                if (fd >= 0) {
                    ExceptionSink ev_xsink;
                    // Try modify first, then add if not registered
                    int rc = dti->loop->modify(fd, events, &ev_xsink);
                    if (ev_xsink) {
                        ev_xsink.clear();
                        dti->loop->add(fd, events, poll_sock, &ev_xsink);
                        if (ev_xsink) {
                            log(QORE_LOG_LEVEL_ERROR,
                                "dedicated key '%s': EventLoop add fd=%d failed",
                                dti->key.c_str(), fd);
                            ev_xsink.clear();
                        }
                    }
                }
                s->deref(xsink);
            }
        }

        // Calculate poll timeout
        int timeout_ms = -1;
        if (dti->pinfo.timeout_us >= 0 && dti->pinfo.timeout_date_us > 0) {
            now_us = get_epoch_us();
            int64 remaining_us = dti->pinfo.timeout_date_us - now_us;
            if (remaining_us <= 0) {
                timeout_ms = 0;
            } else {
                timeout_ms = (int)(remaining_us / 1000);
                if (timeout_ms == 0) {
                    timeout_ms = 1;
                }
            }
        }

        // Check for protocol-level poll timeout hint
        QoreValue ptv = new_poll_info->getKeyValue("poll_timeout_ms");
        if (!ptv.isNullOrNothing()) {
            int64 pt_ms = ptv.getAsBigInt();
            if (pt_ms <= 0) {
                timeout_ms = 0;  // Timer already expired
            } else if (timeout_ms < 0 || pt_ms < timeout_ms) {
                timeout_ms = (int)pt_ms;
            }
        }

        // Poll for events
        std::vector<QoreEventInfo> ev_events;
        dti->loop->poll(ev_events, timeout_ms, xsink);
        if (*xsink) {
            log(QORE_LOG_LEVEL_ERROR, "dedicated key '%s': EventLoop::poll() error",
                dti->key.c_str());
            xsink->clear();
        }

        // Acknowledge notifier if woken
        dti->notifier->acknowledge(xsink);
        if (*xsink) {
            log(QORE_LOG_LEVEL_ERROR, "dedicated key '%s': notifier acknowledge error",
                dti->key.c_str());
            xsink->clear();
        }
    }

    // stop_requested — deliver cancel result
    {
        QoreHashNode* result_hash = buildResultHash(dti->pinfo, true, nullptr, xsink);
        if (result_hash) {
            {
                AutoLocker al(m);
                dedicated_threads.erase(dti->key);
                dti->exited = true;
                dti->exit_cond.broadcast();
            }
            if (dti->pinfo.callback) {
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(result_hash, xsink);
                ExceptionSink cb_xsink;
                ValueHolder rv(dti->pinfo.callback->execValue(*args, &cb_xsink), &cb_xsink);
                if (cb_xsink) {
                    log(QORE_LOG_LEVEL_ERROR,
                        "callback exception for dedicated key '%s' (cancel)",
                        dti->key.c_str());
                    cb_xsink.clear();
                }
            } else if (dti->pinfo.queue) {
                dti->pinfo.queue->push(xsink, result_hash);
            } else {
                result_hash->deref(xsink);
            }
        }
        goto cleanup_after_remove;
    }

cleanup:
    // Early failure (EventLoop/notifier setup) — deliver error result
    {
        ReferenceHolder<QoreHashNode> ex(new QoreHashNode(hashdeclExceptionInfo, xsink), xsink);
        if (!*xsink) {
            ex->setKeyValue("err", new QoreStringNode("ASYNC-IO-ERROR"), xsink);
            ex->setKeyValue("desc", new QoreStringNode("dedicated I/O thread initialization failed"), xsink);
            ex->setKeyValue("type", new QoreStringNode("User"), xsink);
        }
        QoreHashNode* result_hash = buildResultHash(dti->pinfo, false,
            *xsink ? nullptr : ex.release(), xsink);
        if (result_hash) {
            {
                AutoLocker al(m);
                dedicated_threads.erase(dti->key);
                dti->exited = true;
                dti->exit_cond.broadcast();
            }
            if (dti->pinfo.callback) {
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                args->push(result_hash, xsink);
                ExceptionSink cb_xsink;
                ValueHolder rv(dti->pinfo.callback->execValue(*args, &cb_xsink), &cb_xsink);
                if (cb_xsink) {
                    cb_xsink.clear();
                }
            } else if (dti->pinfo.queue) {
                dti->pinfo.queue->push(xsink, result_hash);
            } else {
                result_hash->deref(xsink);
            }
            goto cleanup_after_remove;
        }
    }
    // Remove from dedicated_threads if not already removed
    {
        AutoLocker al(m);
        dedicated_threads.erase(dti->key);
        dti->exited = true;
        dti->exit_cond.broadcast();
    }

cleanup_after_remove:
    log(QORE_LOG_LEVEL_DEBUG, "dedicated thread exiting for key '%s'", dti->key.c_str());

    // Clean up EventLoop
    if (dti->loop) {
        delete dti->loop;
        dti->loop = nullptr;
    }

    // Clean up notifier
    if (dti->notifier) {
        dti->notifier->deref(xsink);
        dti->notifier = nullptr;
    }

    // Clean up C++ poll operation ref
    if (dti->spop_base) {
        dti->spop_base->deref(xsink);
        dti->spop_base = nullptr;
    }

    // Clean up PollInfo
    dti->pinfo.cleanup(xsink);

    // Enqueue for deferred delete — exit_cond was already broadcast when
    // dti->exited was set to true (above), so cancelDedicatedThread() has
    // already been unblocked.  We cannot delete dti here because
    // cancelDedicatedThread may still be accessing it inside exit_cond.wait()
    // between the broadcast and mutex reacquisition.
    {
        AutoLocker al(m);
        deferred_dti_deletes.push_back(dti);
    }
}

bool AsyncIoControllerPriv::cancelDedicatedThread(const std::string& key, ExceptionSink* xsink) {
    QoreCallDispatcher* dispatcher_to_stop = nullptr;

    // Hold the lock for the entire cancel sequence to prevent a race where
    // the dedicated thread exits naturally (error/goal/timeout), erases itself
    // from the map, and deletes DedicatedThreadInfo while we hold a pointer
    {
        AutoLocker al(m);
        auto it = dedicated_threads.find(key);
        if (it == dedicated_threads.end()) {
            return false;
        }
        DedicatedThreadInfo* dti = it->second;

        // Set stop flag and notify (under lock — dti is guaranteed alive)
        dti->stop_requested.store(true, std::memory_order_relaxed);
        if (dti->notifier) {
            dti->notifier->notify();
        }

        // If we ARE the I/O thread, we cannot wait synchronously — that would
        // deadlock since the I/O thread is the one that processes dedicated
        // thread completions.  Signal the stop and return; the dedicated thread
        // will clean up asynchronously.
        if (tid && q_gettid() == tid) {
            return true;
        }

        // Wait for exit
        while (!dti->exited) {
            dti->exit_cond.wait(m);
        }

        // Process deferred deletes (safe now — we hold the lock and are
        // done accessing all DedicatedThreadInfo pointers)
        for (auto* d : deferred_dti_deletes) {
            delete d;
        }
        deferred_dti_deletes.clear();

        // If no more dedicated threads remain, stop the call dispatcher
        // so its worker threads can exit cleanly during program shutdown.
        // New dedicated threads will create a fresh dispatcher if needed.
        if (dedicated_threads.empty() && call_dispatcher) {
            dispatcher_to_stop = call_dispatcher;
            call_dispatcher = nullptr;
        }
    }

    // Stop and delete outside the controller lock to avoid blocking
    // other threads while waiting for workers to exit
    if (dispatcher_to_stop) {
        dispatcher_to_stop->stop(xsink);
        delete dispatcher_to_stop;
    }

    return true;
}

void AsyncIoControllerPriv::stopDedicatedThreads(ExceptionSink* xsink) {
    // Collect all dedicated thread keys
    std::vector<std::string> keys;
    {
        AutoLocker al(m);
        for (auto& [key, dti] : dedicated_threads) {
            keys.push_back(key);
            dti->stop_requested.store(true, std::memory_order_relaxed);
            if (dti->notifier) {
                dti->notifier->notify();
            }
        }
    }

    // Wait for all to exit
    {
        AutoLocker al(m);
        for (auto& key : keys) {
            // The dti may have already been removed by the thread itself
            auto it = dedicated_threads.find(key);
            if (it != dedicated_threads.end()) {
                while (!it->second->exited) {
                    it->second->exit_cond.wait(m);
                }
            }
        }

        // Process deferred deletes
        for (auto* d : deferred_dti_deletes) {
            delete d;
        }
        deferred_dti_deletes.clear();
    }

    // Stop the call dispatcher if it exists
    if (call_dispatcher) {
        call_dispatcher->stop(xsink);
    }
}
