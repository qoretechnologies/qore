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

    // If no callback and no shared queue, create a new result queue
    if (!callback && !res.result_queue) {
        res.result_queue = new Queue();
        res.new_queue_obj = new QoreObject(QC_QUEUE, getProgram(), res.result_queue);
        res.result_queue->ref();  // extra ref for PollInfo storage
    }

    bool do_signal = false;
    bool need_cancel = false;

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
            // Queue cancel for existing operation
            if (!cancel_cond_map.count(uh)) {
                cancel_cond_map[uh] = new QoreCondition();
            }
            do_signal = enqueueCmdLocked(IoCommand::Cancel, uh);
            need_cancel = true;
        }
    }

    // Signal notifier outside lock
    if (do_signal) {
        notifier->notify();
    }

    // Wait for cancel to complete
    if (need_cancel) {
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
            if (tid && !io_exiting) {
                // I/O thread is running — enqueue the cancel command
                if (!cancel_cond_map.count(uh)) {
                    cancel_cond_map[uh] = new QoreCondition();
                }
                do_signal = enqueueCmdLocked(IoCommand::Cancel, uh);
            } else {
                // I/O thread not running — handle cancel directly
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
        if (autostop_flag && rv && cache.empty() && tid) {
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

    QoreCondition done_cond;

    {
        AutoLocker al(m);

        // Count matching operations
        for (auto& it : cache) {
            if (it.second.owner == owner_str) {
                ++count;
            }
        }

        if (count > 0) {
            if (tid && !io_exiting) {
                // I/O thread is running — enqueue the cancel command
                do_signal = enqueueCmdLocked(IoCommand::CancelOwner, std::string(), owner_str, &done_cond);
            } else {
                // I/O thread not running — handle cancel directly
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

    if (do_signal) {
        notifier->notify();
    }

    if (!direct_pinfos.empty()) {
        // Deliver cancel results directly (I/O thread not running)
        for (auto& pinfo : direct_pinfos) {
            doCancelIntern(pinfo, xsink);
            pinfo.cleanup(xsink);
        }
    } else if (count > 0) {
        // Wait for all cancellations to complete
        {
            AutoLocker al(m);
            // The CancelOwner handler will signal done_cond when all ops are canceled.
            // Also check !tid: if the I/O thread exits (e.g. via autostop or crash)
            // before processing the CancelOwner command, we must not hang.
            while (tid) {
                // Check if there are still operations for this owner
                bool found = false;
                for (auto& it : cache) {
                    if (it.second.owner == owner_str) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    break;
                }
                done_cond.wait(m);
            }

            // If tid dropped to 0 but matching ops remain, collect them for direct cancel
            if (!tid) {
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
        if (autostop_flag && count > 0 && cache.empty() && tid) {
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
                    // Capture exception
                    // Build exception hash from the ExceptionSink
                    const QoreValue err_val = poll_xsink.getExceptionErr();
                    const QoreValue desc_val = poll_xsink.getExceptionDesc();
                    ReferenceHolder<QoreHashNode> ex(new QoreHashNode(hashdeclExceptionInfo, xsink), xsink);
                    if (!*xsink) {
                        if (err_val.getType() == NT_STRING) {
                            ex->setKeyValue("err", err_val.get<const QoreStringNode>()->refSelf(), xsink);
                        }
                        if (desc_val.getType() == NT_STRING) {
                            ex->setKeyValue("desc", desc_val.get<const QoreStringNode>()->refSelf(), xsink);
                        }
                        ex->setKeyValue("type", new QoreStringNode("User"), xsink);
                        result.ex_hash = ex.release();
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
            if (cache.empty() && autostop_flag && cmdq.empty() && deferred_deliveries.empty()) {
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
            if (cache.empty() && autostop_flag && cmdq.empty()) {
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
                c.session->handleAsyncReadCompletion(
                    c.stream_id, c.data, c.length, c.error,
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
        // so cancelByOwner() doesn't hang when the I/O thread exits
        for (auto& pending_cmd : cmdq) {
            if (pending_cmd.done_cond) {
                pending_cmd.done_cond->broadcast();
            }
        }
        cmdq.clear();

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

    log(QORE_LOG_LEVEL_DEBUG, "I/O thread exited");
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

                    // Signal done
                    if (cmd.done_cond) {
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
        // Same socket - just update event union
        applyEventUnion(socket, sock_hash, xsink);
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
                // Last reference - remove from EventLoop
                QoreSocketObject* ps = static_cast<QoreSocketObject*>(
                    prev_sock->getReferencedPrivateData(CID_SOCKET, xsink));
                if (ps) {
                    loop->remove(ps->getSocket(), xsink);
                    ps->deref(xsink);
                }
                registered_events.erase(prev_sock_hash);
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
            loop->add(s->getSocket(), union_events, socket, xsink);
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
                if (ps) {
                    // Need to get socket again for the fd
                    ps = static_cast<QoreSocketObject*>(
                        prev_sock->getReferencedPrivateData(CID_SOCKET, xsink));
                    if (ps) {
                        loop->remove(ps->getSocket(), xsink);
                        ps->deref(xsink);
                    }
                }
                registered_events.erase(prev_sock_hash);
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
    for (int fd : new_fds) {
        if (!prev_fds.count(fd)) {
            int ev_flags = QORE_EV_READ;  // InputStreams only need read
            loop->add(fd, ev_flags, socket, xsink);
            if (*xsink) {
                // Non-fatal: the fd might not be epoll-compatible (e.g. regular file on Linux)
                // The I/O loop still drives streaming via POLLOUT on the socket fd
                printd(2, "updateExtraFds() failed to add fd %d to event loop; skipping\n", fd);
                xsink->clear();
                new_fds.erase(fd);
                continue;
            }
        }
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
        const std::string& owner, QoreCondition* done_cond) {
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
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(hashdeclSocketPollResultInfo, xsink), xsink);
    if (*xsink) {
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
