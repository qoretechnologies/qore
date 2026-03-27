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

extern qore_classid_t CID_ASYNCIOCONTROLLER;
extern QoreClass* QC_ASYNCIOCONTROLLER;

// --- Global singleton state ---
static QoreThreadLock aio_singleton_lock;
static AsyncIoControllerPriv* aio_singleton = nullptr;
static QoreObject* aio_singleton_obj = nullptr;

//! Program cleanup callback: cancel all async I/O operations belonging to the program being destroyed
/** Called from qore_program_private::waitForTerminationAndClear() BEFORE clearNamespaceData(),
    so type info is still valid and callbacks can execute safely.
*/
static void aio_program_cleanup(QoreProgram* pgm) {
    AsyncIoControllerPriv* ctrl;
    {
        AutoLocker al(aio_singleton_lock);
        ctrl = aio_singleton;
        if (ctrl) {
            ctrl->ref();
        }
    }
    if (ctrl) {
        ExceptionSink xsink;
        ctrl->cancelByProgram(pgm, &xsink);
        ctrl->deref(&xsink);
    }
}

QoreObject* qore_get_async_io_controller_obj(ExceptionSink* xsink) {
    AutoLocker al(aio_singleton_lock);
    if (aio_singleton_obj) {
        aio_singleton_obj->ref();
        return aio_singleton_obj;
    }
    // Lazy-create singleton with autostop=True
    ReferenceHolder<AsyncIoControllerPriv> holder(new AsyncIoControllerPriv(true, xsink), xsink);
    if (*xsink) {
        return nullptr;
    }
    aio_singleton = *holder;
    QoreObject* obj = new QoreObject(QC_ASYNCIOCONTROLLER, nullptr, holder.release());
    aio_singleton_obj = obj;
    // Register cleanup callback to cancel operations when QorePrograms are destroyed
    qore_register_program_cleanup_callback(aio_program_cleanup);
    // Return an extra ref for the caller
    obj->ref();
    return obj;
}

void qore_async_io_controller_cleanup() {
    QoreObject* obj;
    AsyncIoControllerPriv* priv;
    {
        AutoLocker al(aio_singleton_lock);
        obj = aio_singleton_obj;
        priv = aio_singleton;
        aio_singleton_obj = nullptr;
        aio_singleton = nullptr;
    }
    if (priv) {
        ExceptionSink xsink;
        priv->stopClear(&xsink);
    }
    if (obj) {
        ExceptionSink xsink;
        obj->deref(&xsink);
    }
}

bool qore_is_async_io_controller_singleton(AsyncIoControllerPriv* ctrl) {
    AutoLocker al(aio_singleton_lock);
    return ctrl == aio_singleton;
}

//! Thread-local flag: true when running on the async I/O thread.
//! Used to detect re-entrant calls from Qore object destructors triggered during
//! callback cleanup on the I/O thread.  Synchronous waits (waitCancel, cancelByOwner)
//! must be avoided on the I/O thread to prevent deadlock.
static thread_local bool on_async_io_thread = false;

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
    if (spop_base) {
        spop_base->deref(xsink);
        spop_base = nullptr;
    }
}

// --- QoreCallDispatcher implementation ---

QoreCallDispatcher::QoreCallDispatcher(int max_workers) {
    if (max_workers <= 0) {
        int hw = std::thread::hardware_concurrency();
        this->max_workers = hw > 0 ? std::min(hw, (int)MAX_WORKER_CAP) : 4;
    } else {
        this->max_workers = max_workers;
    }
}

QoreCallDispatcher::~QoreCallDispatcher() {
}

void QoreCallDispatcher::dispatchAbortAsync(QoreObject* spop_obj) {
    enqueue({spop_obj, nullptr, nullptr, nullptr, DT_ABORT, nullptr, std::string()});
}

void QoreCallDispatcher::dispatchOnCompleteAsync(QoreObject* spop_obj, QoreHashNode* result) {
    enqueue({spop_obj, result, nullptr, nullptr, DT_ON_COMPLETE, nullptr, std::string()});
}

void QoreCallDispatcher::dispatchAsync(ResolvedCallReferenceNode* callback, QoreListNode* args) {
    enqueue({nullptr, nullptr, callback, args, DT_CALLBACK, nullptr, std::string()});
}

void QoreCallDispatcher::dispatchContinuePollAsync(QoreObject* spop_obj,
        AsyncIoControllerPriv* controller, const std::string& key) {
    enqueue({spop_obj, nullptr, nullptr, nullptr, DT_CONTINUE_POLL, controller, key});
}

void QoreCallDispatcher::enqueue(AsyncWorkItem&& item) {
    AutoLocker al(m);

    if (stopping) {
        // Cannot dispatch — clean up refs synchronously
        ExceptionSink xsink;
        if (item.spop_obj) {
            item.spop_obj->deref(&xsink);
        }
        if (item.result) {
            item.result->deref(&xsink);
        }
        if (item.callback) {
            item.callback->deref(&xsink);
        }
        if (item.args) {
            item.args->deref(&xsink);
        }
        if (item.controller) {
            item.controller->deref(&xsink);
        }
        return;
    }

    // Lazily spawn a worker if needed and below max
    if (active_workers < max_workers) {
        ++active_workers;
        ExceptionSink xsink;
        int tid = q_start_thread(&xsink, workerEntry, this, QTF_EXTERNAL_LIFECYCLE);
        if (tid == -1) {
            --active_workers;
        }
    }

    // Enqueue even if no workers exist — stop() will drain and clean up.
    // Cannot execute synchronously here: caller may be the I/O thread,
    // and Qore code could call submit() causing deadlock.
    async_queue.push_back(std::move(item));
    work_avail.signal();
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

    // Clean up any remaining async work items
    for (auto& item : async_queue) {
        if (item.spop_obj) {
            item.spop_obj->deref(xsink);
        }
        if (item.result) {
            item.result->deref(xsink);
        }
        if (item.callback) {
            item.callback->deref(xsink);
        }
        if (item.args) {
            item.args->deref(xsink);
        }
        if (item.controller) {
            item.controller->deref(xsink);
        }
    }
    async_queue.clear();
}

void QoreCallDispatcher::workerEntry(ExceptionSink* xsink, void* arg) {
    QoreCallDispatcher* self = static_cast<QoreCallDispatcher*>(arg);
    self->workerLoop(xsink);
    if (*xsink) {
        xsink->clear();
    }
}

void QoreCallDispatcher::workerLoop(ExceptionSink* xsink) {
    while (true) {
        AsyncWorkItem async_item{nullptr, nullptr, nullptr, nullptr, DT_ABORT, nullptr, std::string()};

        {
            AutoLocker al(m);
            while (async_queue.empty() && !stopping) {
                work_avail.wait(m);
            }
            if (stopping && async_queue.empty()) {
                --active_workers;
                if (active_workers == 0) {
                    workers_done.broadcast();
                }
                return;
            }
            async_item = async_queue.front();
            async_queue.pop_front();
        }

        ExceptionSink work_xsink;
        const char* method_name = nullptr;

        switch (async_item.type) {
            case DT_ABORT: {
                method_name = "abort";
                ValueHolder rv(async_item.spop_obj->evalMethod("abort", nullptr, &work_xsink),
                    &work_xsink);
                break;
            }
            case DT_ON_COMPLETE: {
                method_name = "onComplete";
                ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                if (async_item.result) {
                    args->push(async_item.result, xsink);
                    async_item.result = nullptr;  // ownership transferred to args
                }
                ValueHolder rv(async_item.spop_obj->evalMethod("onComplete", *args, &work_xsink),
                    &work_xsink);
                break;
            }
            case DT_CALLBACK: {
                method_name = "callback";
                if (async_item.callback) {
                    ValueHolder rv(async_item.callback->execValue(async_item.args, &work_xsink),
                        &work_xsink);
                }
                break;
            }
            case DT_CONTINUE_POLL: {
                method_name = "continuePoll";
                QoreHashNode* new_poll_info = nullptr;
                QoreHashNode* ex_hash = nullptr;
                bool completed = false;

                ValueHolder rv(async_item.spop_obj->evalMethod("continuePoll", nullptr, &work_xsink),
                    &work_xsink);
                if (work_xsink) {
                    QoreException* ex_obj = work_xsink.getException();
                    if (ex_obj) {
                        ex_hash = ex_obj->makeExceptionObject();
                    }
                    work_xsink.clear();
                } else if (rv->getType() == NT_HASH) {
                    new_poll_info = rv.release().get<QoreHashNode>();
                } else {
                    completed = true;
                }

                // Send result back to the I/O thread
                async_item.controller->enqueueContinuePollResult(
                    async_item.key, new_poll_info, ex_hash, completed);
                async_item.controller->deref(&work_xsink);
                async_item.controller = nullptr;
                break;
            }
        }

        if (work_xsink) {
            const QoreStringNode* err_str = work_xsink.getExceptionErr().get<const QoreStringNode>();
            const QoreStringNode* desc_str = work_xsink.getExceptionDesc().get<const QoreStringNode>();
            printd(0, "QoreCallDispatcher::workerLoop() %s exception: %s: %s\n",
                method_name ? method_name : "unknown",
                err_str ? err_str->c_str() : "?",
                desc_str ? desc_str->c_str() : "?");
            work_xsink.clear();
        }

        if (async_item.spop_obj) {
            async_item.spop_obj->deref(xsink);
        }
        if (async_item.result) {
            async_item.result->deref(xsink);
        }
        if (async_item.callback) {
            async_item.callback->deref(xsink);
        }
        if (async_item.args) {
            async_item.args->deref(xsink);
        }
    }
}

// --- AsyncIoControllerPriv implementation ---

AsyncIoControllerPriv::AsyncIoControllerPriv(bool autostop, ExceptionSink* xsink)
    : tid(0), autostop_flag(autostop), shutting_down(false), force_poll(false),
      io_waiting(false), io_exiting(false), ready_flag(false),
      submit_seq(0), processed_seq(0), autostop_idle_since(0),
      logger(nullptr), timer_callback(nullptr),
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
        stopThreadPool(xsink);
        if (call_dispatcher) {
            call_dispatcher->stop(xsink);
            delete call_dispatcher;
            call_dispatcher = nullptr;
        }
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

int AsyncIoControllerPriv::submitTask(ResolvedCallReferenceNode* task, ResolvedCallReferenceNode* cancel,
        ExceptionSink* xsink) {
    ThreadPool* tp;
    {
        AutoLocker al(pool_mutex);
        if (pool_stopped) {
            xsink->raiseException("ASYNC-IO-ERROR", "cannot submit task: controller thread pool has been stopped");
            return -1;
        }
        if (!thread_pool) {
            thread_pool = new ThreadPool(xsink, 0, 0, 8, 5000);
            if (*xsink) {
                delete thread_pool;
                thread_pool = nullptr;
                return -1;
            }
        }
        tp = thread_pool;
    }
    // Safe to use tp outside the lock: stopThreadPool() is only called from deref() when
    // ROdereference() returns true (last reference), so no concurrent submitTask() is possible
    return tp->submit(task, cancel, xsink);
}

void AsyncIoControllerPriv::stopThreadPool(ExceptionSink* xsink) {
    ThreadPool* tp = nullptr;
    {
        AutoLocker al(pool_mutex);
        pool_stopped = true;
        tp = thread_pool;
        thread_pool = nullptr;
    }
    if (tp) {
        tp->stopWait(xsink);
        tp->deref(xsink);
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

    AbstractPollableIoObjectBase* sock = static_cast<AbstractPollableIoObjectBase*>(
        sock_obj->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
    if (!sock) {
        if (!*xsink) {
            xsink->raiseException("ASYNC-IO-ERROR",
                "invalid pollable I/O object in 'sock' field; got instance of '%s'",
                sock_obj->getClassName());
        }
        return nullptr;
    }
    ReferenceHolder<AbstractPollableIoObjectBase> sock_holder(sock, xsink);

    v = info->getKeyValue("spop");
    QoreObject* spop_obj = v.getType() == NT_OBJECT ? v.get<QoreObject>() : nullptr;
    if (!spop_obj) {
        xsink->raiseException("ASYNC-IO-ERROR", "missing 'spop' field in SocketPollOperationInfo");
        return nullptr;
    }

    // Detect Qore-language method overrides — if present, dispatch to worker thread
    const QoreClass* spop_cls = spop_obj->getClass();
    const QoreMethod* abort_meth = spop_cls->findMethod("abort");
    bool has_qore_abort = abort_meth && !abort_meth->isBuiltin();
    const QoreMethod* on_complete_meth = spop_cls->findMethod("onComplete");
    // Detect onComplete overrides: either Qore-language (!isBuiltin) or C++
    // (DelegatingPollOperation) — any class other than AbstractPollOperation itself
    bool has_qore_on_complete = on_complete_meth
        && on_complete_meth->getClass()->getID() != CID_ABSTRACTPOLLOPERATION;

    // Get C++ poll operation for direct continuePoll() calls (bypasses evalMethod overhead).
    // Only attempt getReferencedPrivateData if the class actually inherits the C++ base;
    // Qore-language poll operation classes don't have C++ private data and would throw.
    SocketPollOperationBase* spop_base = nullptr;
    if (spop_cls->getClass(CID_SOCKETPOLLOPERATIONBASE)) {
        spop_base = static_cast<SocketPollOperationBase*>(
            spop_obj->getReferencedPrivateData(CID_SOCKETPOLLOPERATIONBASE, xsink));
        if (*xsink) {
            return nullptr;
        }
    }
    // spop_base is nullptr for Qore-language poll operations;
    // in that case continuePoll() is called via evalMethod on the I/O thread
    ReferenceHolder<SocketPollOperationBase> spop_base_holder(spop_base, xsink);

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

    // RAII cleanup for refcounted resources acquired below.
    // Pointers are nulled as ownership transfers to PollInfo or the caller.
    struct SubmitResources {
        Queue* result_queue;
        QoreHashNode* other_hash;
        QoreHashNode* poll_info_hash;
        QoreObject* new_queue_obj;
        ExceptionSink* xsink;

        ~SubmitResources() {
            if (result_queue) { result_queue->deref(xsink); }
            if (other_hash) { other_hash->deref(xsink); }
            if (poll_info_hash) { poll_info_hash->deref(xsink); }
            if (new_queue_obj) { new_queue_obj->deref(xsink); }
        }
    } res{nullptr, nullptr, nullptr, nullptr, xsink};

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

    // If no onComplete override and no shared queue, create a new result queue
    if (!has_qore_on_complete && !res.result_queue) {
        res.result_queue = new Queue();
        res.new_queue_obj = new QoreObject(QC_QUEUE, getProgram(), res.result_queue);
        res.result_queue->ref();  // extra ref for PollInfo storage
    }

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
            if (on_async_io_thread) {
                // On an async I/O thread — cancel directly to avoid deadlock
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

        // Start I/O thread if needed — must happen BEFORE inserting into cache,
        // because startIntern() releases the lock while waiting for an exiting
        // I/O thread, and the exit cleanup iterates cache and discards entries
        if (io_exiting || !tid) {
            startIntern(xsink);
            if (*xsink) {
                return nullptr;
            }
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
        pinfo.timeout_date_us = 0;  // Will be set in I/O thread
        pinfo.has_qore_abort = has_qore_abort;
        pinfo.has_qore_on_complete = has_qore_on_complete;
        pinfo.spop_base = spop_base;
        spop_base_holder.release();  // Transfer ownership to pinfo

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

bool AsyncIoControllerPriv::cancel(AbstractPollableIoObjectBase* sock, ExceptionSink* xsink) {
    std::string uh = getSocketHash(sock);
    SimpleRefHolder<QoreStringNode> key(new QoreStringNode(uh));
    return cancelByKey(*key, xsink);
}

bool AsyncIoControllerPriv::cancelByKey(const QoreStringNode* key, ExceptionSink* xsink) {
    std::string uh(key->c_str());

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
            if (tid && !io_exiting && !on_async_io_thread) {
                // I/O thread is running and we're NOT on any async I/O thread —
                // enqueue the cancel command
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
        if (autostop_flag && rv && cache.empty() && timer_info_map.empty() && tid) {
            do_signal = enqueueCmdLocked(IoCommand::Quit);
            stopped = true;
        } else {
            do_signal = false;
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    if (stopped && !on_async_io_thread) {
        // Do NOT waitStop from the I/O thread — it would deadlock waiting for
        // itself to exit.  The Quit command has been enqueued; the I/O thread
        // main loop will process it after returning from the current callback.
        waitStop(xsink);
    }

    return rv;
}

int AsyncIoControllerPriv::cancelByOwner(const QoreStringNode* owner, ExceptionSink* xsink) {
    std::string owner_str(owner->c_str());
    bool do_signal = false;
    int count = 0;
    std::vector<PollInfo> direct_pinfos;

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
            if (tid && !io_exiting && !on_async_io_thread) {
                // I/O thread is running and we're NOT on any async I/O thread —
                // enqueue the cancel command
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
        if (autostop_flag && count > 0 && cache.empty() && timer_info_map.empty() && tid) {
            do_signal = enqueueCmdLocked(IoCommand::Quit);
            stopped = true;
        } else {
            do_signal = false;
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    if (stopped && !on_async_io_thread) {
        // Do NOT waitStop from the I/O thread — it would deadlock waiting for
        // itself to exit.  The Quit command has been enqueued; the I/O thread
        // main loop will process it after returning from the current callback.
        waitStop(xsink);
    }

    return count;
}

void AsyncIoControllerPriv::cancelByProgram(QoreProgram* pgm, ExceptionSink* xsink) {
    // Cancel cache operations whose callbacks belong to this program
    // This runs on the program's cleanup thread, so we use the direct cancel path
    // (same as cancelByOwner when I/O thread is not running or we're on the I/O thread)
    std::vector<PollInfo> cancel_pinfos;
    bool do_signal = false;
    {
        AutoLocker al(m);
        std::vector<std::string> keys;
        for (auto& [key, pinfo] : cache) {
            if (pinfo.spop_obj && pinfo.spop_obj->getProgram() == pgm) {
                keys.push_back(key);
            }
        }
        for (auto& key : keys) {
            auto it = cache.find(key);
            if (it != cache.end()) {
                if (tid && !io_exiting) {
                    // Unregister from EventLoop via the I/O thread command queue
                    do_signal = enqueueCmdLocked(IoCommand::Cancel, key);
                }
                cancel_pinfos.push_back(it->second);
                it->second = PollInfo();
                cache.erase(it);
            }
        }
    }

    if (do_signal) {
        notifier->notify();
    }

    // Deliver cancel results while program type info is still valid
    for (auto& pinfo : cancel_pinfos) {
        doCancelIntern(pinfo, xsink);
        pinfo.cleanup(xsink);
    }
}

void AsyncIoControllerPriv::wake() {
    bool do_signal = false;
    {
        AutoLocker al(m);
        if (tid && !io_exiting) {
            do_signal = enqueueCmdLocked(IoCommand::Wake);
        }
    }
    if (do_signal) {
        notifier->notify();
    }
}

void AsyncIoControllerPriv::start(ExceptionSink* xsink) {
    AutoLocker al(m);
    if (!tid || io_exiting) {
        startIntern(xsink);
    }
}

void AsyncIoControllerPriv::stop(ExceptionSink* xsink) {
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

    // Stop and destroy call dispatcher so async callbacks complete before shutdown.
    // A fresh dispatcher will be lazily created on next submit() if needed.
    {
        QoreCallDispatcher* cd = nullptr;
        {
            AutoLocker al(m);
            cd = call_dispatcher;
            call_dispatcher = nullptr;
        }
        if (cd) {
            cd->stop(xsink);
            delete cd;
        }
    }

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
    return (int)cache.size();
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
    rv->setKeyValue("cache_size", (int64)cache.size(), xsink);
    rv->setKeyValue("autostop", autostop_flag, xsink);
    rv->setKeyValue("shutting_down", shutting_down, xsink);
    rv->setKeyValue("tid", (int64)tid, xsink);

    // Build cache key list
    ReferenceHolder<QoreListNode> keys(new QoreListNode(stringTypeInfo), xsink);
    for (auto& it : cache) {
        keys->push(new QoreStringNode(it.first), xsink);
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

        // Enqueue command
        Command cmd;
        cmd.cmd = IoCommand::AddTimer;
        cmd.timer_deadline_us = deadline_us;
        cmd.timer_id = id;
        cmd.done_cond = nullptr;

        if (io_exiting || !tid) {
            startIntern(xsink);
            if (*xsink) {
                return -1;
            }
        }

        // Store user data under lock AFTER startIntern() — the exiting I/O thread's
        // cleanup iterates timer_info_map and discards entries; inserting before
        // startIntern() lets the exit cleanup destroy our callback while we wait for
        // the old thread to finish, causing the timer to fire with no callback
        TimerInfo tinfo;
        tinfo.udata = udata.refSelf();
        timer_info_map[id] = tinfo;

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

void AsyncIoControllerPriv::setMaxCallbackWorkers(int max_workers) {
    AutoLocker al(m);
    max_callback_workers = max_workers;
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

    // Start I/O thread; singleton uses QTF_EXTERNAL_LIFECYCLE so it doesn't
    // block QoreProgramHelper shutdown — it's stopped by qore_async_io_controller_cleanup()
    ref();  // Reference for the I/O thread
    int thread_flags = qore_is_async_io_controller_singleton(this) ? QTF_EXTERNAL_LIFECYCLE : 0;
    tid = q_start_thread(xsink, ioThreadEntry, this, thread_flags);
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
    on_async_io_thread = true;
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
            SocketPollOperationBase* spop_base; // Not refed - direct C++ poll op (or nullptr)
            bool timed_out;
        };
        std::vector<OpToPoll> ops_to_poll;
        int64 poll_deadline_us = 0;
        bool forced = false;

        {
            AutoLocker al(m);

            if (force_poll) {
                force_poll = false;
                forced = true;
            }

            int64 now_us = get_epoch_us();

            for (auto& [key, pinfo] : cache) {
                // Skip Qore ops whose continuePoll is dispatched to a worker
                if (pinfo.continue_poll_in_flight) {
                    if (pinfo.timeout_us >= 0 && pinfo.timeout_date_us > 0) {
                        if (poll_deadline_us == 0 || pinfo.timeout_date_us < poll_deadline_us) {
                            poll_deadline_us = pinfo.timeout_date_us;
                        }
                    }
                    continue;
                }

                // Initialize timeout_date if not set
                if (pinfo.timeout_date_us == 0 && pinfo.timeout_us >= 0) {
                    pinfo.timeout_date_us = now_us + pinfo.timeout_us;
                }

                // Check for timeout (timeout_us >= 0 means timeout is enabled; < 0 means no timeout)
                if (pinfo.timeout_us >= 0 && pinfo.timeout_date_us > 0 && pinfo.timeout_date_us <= now_us) {
                    ops_to_poll.push_back({key, pinfo.spop_obj, pinfo.spop_base, true});
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

                        // Check for protocol-level poll timeout (e.g., QUIC timer expiry)
                        // Uses absolute deadline stored in PollInfo
                        if (pinfo.poll_timeout_deadline_us > 0
                                && pinfo.poll_timeout_deadline_us <= now_us) {
                            // Timer expired; force immediate continuePoll
                            pinfo.poll_timeout_deadline_us = 0;
                            ops_to_poll.push_back({key, pinfo.spop_obj, pinfo.spop_base, false});
                            continue;
                        }
                        // Update poll_deadline for EventLoop sleep
                        if (pinfo.poll_timeout_deadline_us > 0) {
                            if (poll_deadline_us == 0
                                    || pinfo.poll_timeout_deadline_us < poll_deadline_us) {
                                poll_deadline_us = pinfo.poll_timeout_deadline_us;
                            }
                        }

                        continue;
                    }
                }

                ops_to_poll.push_back({key, pinfo.spop_obj, pinfo.spop_base, false});
            }

            // Update processed sequence counter
            processed_seq = submit_seq;
            processed_cond.broadcast();
        }

        // --- PHASE 2: continuePoll outside lock ---
        // All continuePoll() calls run on the I/O thread. This is safe because:
        // - C++ built-in implementations are trusted and non-blocking by design
        // - Qore framework overrides (Http2PollOperation, etc.) are trusted and require
        //   thread affinity for stream objects (BinaryInputStream, FileInputStream)
        // Only callbacks are dispatched to worker threads (those are user code).
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
            } else if (op.spop_base) {
                // C++ poll operation — safe to call directly on I/O thread
                ExceptionSink poll_xsink;
                QoreHashNode* new_info = op.spop_base->continuePoll(&poll_xsink);
                if (poll_xsink) {
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
            } else {
                // Qore poll operation — check trust level via QDOM_PROCESS.
                // Trusted code (modules, framework) runs on I/O thread like
                // nginx runs socket handlers on the event loop.
                // Sandboxed code (without QDOM_PROCESS) is dispatched to the
                // worker pool to prevent blocking the event loop.
                QoreProgram* pgm = op.spop_obj->getProgram();
                bool trusted = pgm && !(pgm->getParseOptions() & PO_NO_PROCESS_CONTROL);
                if (trusted) {
                    // Trusted Qore code — safe to call on I/O thread
                    ExceptionSink poll_xsink;
                    ValueHolder rv(op.spop_obj->evalMethod("continuePoll", nullptr,
                        &poll_xsink), &poll_xsink);
                    if (poll_xsink) {
                        QoreException* ex_obj = poll_xsink.getException();
                        if (ex_obj) {
                            result.ex_hash = ex_obj->makeExceptionObject();
                        }
                        poll_xsink.clear();
                    } else if (rv->getType() == NT_HASH) {
                        result.new_poll_info = rv.release().get<QoreHashNode>();
                    } else {
                        result.completed = true;
                    }
                } else {
                    // Sandboxed Qore code — dispatch to worker thread; result
                    // comes back via IoCommand::ContinuePollResult
                    {
                        AutoLocker al(m);
                        if (!call_dispatcher) {
                            call_dispatcher = new QoreCallDispatcher(max_callback_workers);
                        }
                        auto it = cache.find(op.key);
                        if (it != cache.end()) {
                            it->second.continue_poll_in_flight = true;
                        }
                    }
                    this->ref();  // keep controller alive until worker delivers result
                    op.spop_obj->ref();
                    call_dispatcher->dispatchContinuePollAsync(op.spop_obj, this, op.key);
                    continue;  // do NOT add to poll_results
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
                    dd.spop_obj = pinfo.spop_obj;
                    if (dd.spop_obj) {
                        dd.spop_obj->ref();
                    }
                    dd.has_on_complete = pinfo.has_qore_on_complete;
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
                                AbstractPollableIoObjectBase* so = static_cast<AbstractPollableIoObjectBase*>(
                                    poll_sock->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE,
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

                        // Store absolute deadline for protocol-level poll timeout (QUIC timers)
                        {
                            QoreValue ptv = result.new_poll_info
                                ? result.new_poll_info->getKeyValue("poll_timeout_ms") : QoreValue();
                            if (!ptv.isNullOrNothing()) {
                                int64 pt_ms = ptv.getAsBigInt();
                                if (pt_ms <= 0) {
                                    // Timer already expired; force immediate poll
                                    poll_deadline_us = 1;
                                    pinfo.poll_timeout_deadline_us = 0;
                                } else {
                                    int64 deadline = get_epoch_us() + pt_ms * 1000;
                                    pinfo.poll_timeout_deadline_us = deadline;
                                    if (poll_deadline_us == 0 || deadline < poll_deadline_us) {
                                        poll_deadline_us = deadline;
                                    }
                                }
                            } else {
                                pinfo.poll_timeout_deadline_us = 0;
                            }
                        }
                    }
                }
            }

            // Check autostop (only if no deliveries pending — recheck after delivery)
            if (cache.empty() && timer_info_map.empty()
                    && autostop_flag && cmdq.empty() && deferred_deliveries.empty()) {
                // Skip grace period during shutdown — exit immediately
                if (shutting_down) {
                    io_exiting = true;
                    if (io_waiting) {
                        io_cond.broadcast();
                    }
                    do_autostop = true;
                } else {
                    int64 now_us = get_epoch_us();
                    if (!autostop_idle_since) {
                        // First time idle — start the grace period
                        autostop_idle_since = now_us;
                    } else if (now_us - autostop_idle_since >= AUTOSTOP_GRACE_US) {
                        // Grace period expired — exit
                        io_exiting = true;
                        if (io_waiting) {
                            io_cond.broadcast();
                        }
                        do_autostop = true;
                    }
                    // Else: still within grace period — keep polling
                    if (!do_autostop) {
                        // Ensure we wake up to re-check when the grace period expires
                        int64 grace_deadline_us = autostop_idle_since + AUTOSTOP_GRACE_US;
                        if (poll_deadline_us == 0 || grace_deadline_us < poll_deadline_us) {
                            poll_deadline_us = grace_deadline_us;
                        }
                    }
                }
            } else {
                // Cache is non-empty — reset idle timer
                autostop_idle_since = 0;
            }
        }

        // Deliver results outside lock — onComplete dispatched to worker threads
        for (auto& dd : deferred_deliveries) {
            deliverResult(dd.queue, dd.spop_obj, dd.has_on_complete, dd.result, xsink);
            dd.spop_obj = nullptr;
            dd.result = nullptr;
            if (dd.queue) {
                dd.queue->deref(xsink);
                dd.queue = nullptr;
            }
        }

        // Re-check autostop after deliveries (callbacks may have submitted new ops)
        if (!do_autostop && !deferred_deliveries.empty()) {
            AutoLocker al(m);
            if (cache.empty() && timer_info_map.empty()
                    && autostop_flag && cmdq.empty()) {
                if (shutting_down) {
                    io_exiting = true;
                    if (io_waiting) {
                        io_cond.broadcast();
                    }
                    do_autostop = true;
                } else {
                    int64 now_us = get_epoch_us();
                    if (!autostop_idle_since) {
                        autostop_idle_since = now_us;
                    } else if (now_us - autostop_idle_since >= AUTOSTOP_GRACE_US) {
                        io_exiting = true;
                        if (io_waiting) {
                            io_cond.broadcast();
                        }
                        do_autostop = true;
                    }
                    // Ensure we wake up to re-check when the grace period expires;
                    // without this, epoll_wait(-1) would sleep indefinitely
                    if (!do_autostop) {
                        int64 grace_deadline_us = autostop_idle_since + AUTOSTOP_GRACE_US;
                        if (poll_deadline_us == 0 || grace_deadline_us < poll_deadline_us) {
                            poll_deadline_us = grace_deadline_us;
                        }
                    }
                }
            } else {
                autostop_idle_since = 0;
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
                            AbstractPollableIoObjectBase* s = static_cast<AbstractPollableIoObjectBase*>(
                                obj->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
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

        // Deliver timer events outside lock — dispatch to worker threads
        {
            // Ensure call_dispatcher exists for async dispatch
            if (!timer_events.empty()) {
                AutoLocker al(m);
                if (!call_dispatcher) {
                    call_dispatcher = new QoreCallDispatcher(max_callback_workers);
                }
            }
        }
        for (auto& te : timer_events) {
            if (te.udata.getType() == NT_RUNTIME_CLOSURE || te.udata.getType() == NT_FUNCREF) {
                // udata is a code reference — dispatch it asynchronously
                ResolvedCallReferenceNode* code_ref = te.udata.get<ResolvedCallReferenceNode>();
                code_ref->ref();
                call_dispatcher->dispatchAsync(code_ref, nullptr);
            } else if (cb_snapshot) {
                // Fall back to the registered timer callback for non-code udata
                ReferenceHolder<QoreHashNode> timer_hash(
                    new QoreHashNode(hashdeclTimerEventInfo, xsink), xsink);
                if (!*xsink) {
                    timer_hash->setKeyValue("id", te.id, xsink);
                    if (te.udata.hasNode()) {
                        timer_hash->setKeyValue("udata", te.udata.refSelf(), xsink);
                    }
                    QoreListNode* args = new QoreListNode(autoTypeInfo);
                    args->push(timer_hash.release(), xsink);
                    cb_snapshot->ref();
                    call_dispatcher->dispatchAsync(cb_snapshot, args);
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

    // Cleanup on exit — cancel results are collected under lock and delivered outside
    std::vector<PollInfo> exit_cancel_pinfos;
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

        // Deliver cancel results to all remaining cache entries so that
        // callers blocked on callback delivery are unblocked during shutdown.
        // Collect entries under lock, deliver outside lock to avoid deadlock.
        for (auto& [key, pinfo] : cache) {
            exit_cancel_pinfos.push_back(pinfo);
            pinfo = PollInfo();
        }
        cache.clear();

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
            // Clean up refcounted fields from ContinuePollResult commands
            if (pending_cmd.cmd == IoCommand::ContinuePollResult) {
                if (pending_cmd.continue_poll_result) {
                    pending_cmd.continue_poll_result->deref(xsink);
                }
                if (pending_cmd.continue_poll_ex) {
                    pending_cmd.continue_poll_ex->deref(xsink);
                }
            }
        }
        cmdq.clear();
    }

    // Deliver cancel results to remaining cache entries outside the lock
    // so exec() callers blocked on q->shift() are unblocked
    for (auto& pinfo : exit_cancel_pinfos) {
        doCancelIntern(pinfo, xsink);
        pinfo.cleanup(xsink);
    }

    // NOTE: call_dispatcher is NOT stopped here — it's stopped in stop() (explicit
    // shutdown) and deref() (destruction). Workers use QTF_EXTERNAL_LIFECYCLE so they
    // don't block process exit. Stopping the dispatcher on every autostop cycle would
    // add unnecessary latency when the I/O thread restarts on the next submit().

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
        autostop_idle_since = 0;
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

                case IoCommand::ContinuePollResult: {
                    // Result from async continuePoll() dispatch from worker thread
                    bool finished = false;
                    DeferredDelivery dd;

                    {
                        AutoLocker al(m);
                        auto it = cache.find(cmd.key);
                        if (it == cache.end()) {
                            // Operation was canceled while continuePoll was in flight
                            if (cmd.continue_poll_result) {
                                cmd.continue_poll_result->deref(xsink);
                            }
                            if (cmd.continue_poll_ex) {
                                cmd.continue_poll_ex->deref(xsink);
                            }
                            break;
                        }

                        PollInfo& pinfo = it->second;
                        pinfo.continue_poll_in_flight = false;

                        if (cmd.continue_poll_ex || cmd.continue_poll_completed) {
                            // Operation finished (error or completed)
                            finished = true;
                            unregisterExtraFds(cmd.key, xsink);
                            unregisterFromEventLoop(cmd.key, xsink);

                            QoreHashNode* result_hash = buildResultHash(pinfo, false,
                                cmd.continue_poll_ex, xsink);
                            if (cmd.continue_poll_ex) {
                                cmd.continue_poll_ex->deref(xsink);
                            }

                            dd.key = cmd.key;
                            dd.queue = pinfo.queue;
                            if (dd.queue) { dd.queue->ref(); }
                            dd.spop_obj = pinfo.spop_obj;
                            if (dd.spop_obj) { dd.spop_obj->ref(); }
                            dd.has_on_complete = pinfo.has_qore_on_complete;
                            dd.result = result_hash;

                            auto cit = cancel_cond_map.find(cmd.key);
                            if (cit != cancel_cond_map.end()) {
                                cit->second->broadcast();
                                delete cit->second;
                                cancel_cond_map.erase(cit);
                            }

                            pinfo.cleanup(xsink);
                            cache.erase(it);
                        } else {
                            // Still pending — update poll_info and force re-poll
                            // so the operation is included in the next Phase 1 even
                            // if the socket isn't marked ready by epoll (application-
                            // level data may be buffered in the Http2Session)
                            force_poll = true;

                            if (pinfo.poll_info) {
                                pinfo.poll_info->deref(xsink);
                            }
                            pinfo.poll_info = cmd.continue_poll_result;

                            if (cmd.continue_poll_result) {
                                std::string sock_hash;
                                int events = 0;
                                QoreObject* poll_sock = getSocketFromPollInfo(
                                    cmd.continue_poll_result, sock_hash, events, xsink);
                                if (!*xsink && poll_sock) {
                                    updateEventLoopRegistration(cmd.key, poll_sock,
                                        sock_hash, events, xsink);
                                    updateExtraFds(cmd.key, poll_sock,
                                        cmd.continue_poll_result, xsink);
                                }

                                // Store absolute deadline for protocol-level poll timeout
                                QoreValue ptv = cmd.continue_poll_result->getKeyValue(
                                    "poll_timeout_ms");
                                if (!ptv.isNullOrNothing()) {
                                    int64 pt_ms = ptv.getAsBigInt();
                                    if (pt_ms <= 0) {
                                        pinfo.poll_timeout_deadline_us = 0;
                                    } else {
                                        pinfo.poll_timeout_deadline_us =
                                            get_epoch_us() + pt_ms * 1000;
                                    }
                                } else {
                                    pinfo.poll_timeout_deadline_us = 0;
                                }
                            }
                        }
                    }

                    // Deliver result outside lock
                    if (finished) {
                        deliverResult(dd.queue, dd.spop_obj, dd.has_on_complete,
                            dd.result, xsink);
                        dd.spop_obj = nullptr;
                        dd.result = nullptr;
                        if (dd.queue) {
                            dd.queue->deref(xsink);
                        }
                    }
                    break;
                }
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
    // Call abort on the poll operation.
    // Trusted code (QDOM_PROCESS) runs directly; sandboxed code is dispatched
    // to the worker pool to avoid blocking the I/O thread.
    if (pinfo.has_qore_abort && on_async_io_thread) {
        // Always dispatch Qore abort() to worker pool — it may acquire
        // application-level locks that would block the I/O thread
        {
            AutoLocker al(m);
            if (!call_dispatcher) {
                call_dispatcher = new QoreCallDispatcher(max_callback_workers);
            }
        }
        pinfo.spop_obj->ref();
        call_dispatcher->dispatchAbortAsync(pinfo.spop_obj);
    } else {
        // C++ built-in or not on I/O thread — safe to call directly
        callAbort(pinfo.spop_obj, xsink);
    }

    // Build result hash
    QoreHashNode* result = buildResultHash(pinfo, true, nullptr, xsink);

    // Deliver result — dispatches onComplete to worker thread if overridden
    if (pinfo.spop_obj) {
        pinfo.spop_obj->ref();
    }
    deliverResult(pinfo.queue, pinfo.spop_obj, pinfo.has_qore_on_complete, result, xsink);
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
        AbstractPollableIoObjectBase* ps = static_cast<AbstractPollableIoObjectBase*>(
            prev_sock->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
        if (ps) {
            prev_sock_hash = getSocketHash(ps);
            ps->deref(xsink);
        }
    }

    if (prev_sock && prev_sock_hash == sock_hash) {
        // Same socket object - check if underlying fd changed (e.g., reconnection to
        // a different host during multi-step poll operations like OAuth2 token refresh)
        bool fd_changed = false;
        AbstractPollableIoObjectBase* s = static_cast<AbstractPollableIoObjectBase*>(
            socket->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
        if (s) {
            int curr_fd = s->getPollableDescriptor();
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
                    AbstractPollableIoObjectBase* ps = static_cast<AbstractPollableIoObjectBase*>(
                        prev_sock->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
                    if (ps) {
                        loop->remove(ps->getPollableDescriptor(), xsink);
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
        AbstractPollableIoObjectBase* s = static_cast<AbstractPollableIoObjectBase*>(
            socket->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
        if (s) {
            int fd = s->getPollableDescriptor();
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
    AbstractPollableIoObjectBase* ps = static_cast<AbstractPollableIoObjectBase*>(
        prev_sock->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
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
                    AbstractPollableIoObjectBase* check_sock = static_cast<AbstractPollableIoObjectBase*>(
                        prev_sock->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
                    if (check_sock) {
                        int current_fd = check_sock->getPollableDescriptor();
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
        AbstractPollableIoObjectBase* s = static_cast<AbstractPollableIoObjectBase*>(
            socket->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
        if (s) {
            loop->modify(s->getPollableDescriptor(), union_events, xsink);
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

std::string AsyncIoControllerPriv::getSocketHash(AbstractPollableIoObjectBase* sock) {
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

    AbstractPollableIoObjectBase* s = static_cast<AbstractPollableIoObjectBase*>(
        obj->getReferencedPrivateData(CID_ABSTRACTPOLLABLEIOOBJECTBASE, xsink));
    if (s) {
        sock_hash = getSocketHash(s);
        s->deref(xsink);
    }

    return obj;
}

void AsyncIoControllerPriv::enqueueContinuePollResult(const std::string& key,
        QoreHashNode* new_poll_info, QoreHashNode* ex_hash, bool completed) {
    bool do_signal = false;
    {
        AutoLocker al(m);
        if (!tid || io_exiting) {
            ExceptionSink xsink;
            if (new_poll_info) {
                new_poll_info->deref(&xsink);
            }
            if (ex_hash) {
                ex_hash->deref(&xsink);
            }
            return;
        }
        Command c;
        c.cmd = IoCommand::ContinuePollResult;
        c.key = key;
        c.continue_poll_result = new_poll_info;
        c.continue_poll_ex = ex_hash;
        c.continue_poll_completed = completed;
        cmdq.push_back(std::move(c));
        do_signal = true;
    }
    if (do_signal) {
        notifier->notify();
    }
}

void AsyncIoControllerPriv::callAbort(QoreObject* spop_obj, ExceptionSink* xsink) {
    ValueHolder rv(spop_obj->evalMethod("abort", nullptr, xsink), xsink);
    if (*xsink) {
        xsink->clear();
    }
}

void AsyncIoControllerPriv::deliverResult(Queue* queue, QoreObject* spop_obj,
        bool has_on_complete, QoreHashNode* result, ExceptionSink* xsink) {
    if (has_on_complete && spop_obj) {
        // Always dispatch onComplete() to worker pool — it may acquire
        // application-level locks that would block the I/O thread.
        // Only continuePoll() is safe to run on the I/O thread (frame processing).
        {
            AutoLocker al(m);
            if (!call_dispatcher) {
                call_dispatcher = new QoreCallDispatcher(max_callback_workers);
            }
        }
        call_dispatcher->dispatchOnCompleteAsync(spop_obj, result);
    } else if (queue) {
        if (spop_obj) {
            spop_obj->deref(xsink);
        }
        queue->push(xsink, result);
    } else {
        if (spop_obj) {
            spop_obj->deref(xsink);
        }
        if (result) {
            result->deref(xsink);
        }
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

