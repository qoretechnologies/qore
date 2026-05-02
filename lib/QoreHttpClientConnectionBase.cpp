/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpClientConnectionBase.cpp

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
#include <qore/HttpClientConnection.h>
#include <qore/HttpClientConnectionManager.h>
#include <qore/AsyncCompletionAction.h>

#include <cassert>
#include <unordered_map>

bool HttpClientConnectionBase::tryReserveStream() {
    AutoLocker al(reserve_lock);
    int max = getMaxConcurrentStreams();
    if (max && (getActiveStreamCount() + pending_stream_count) >= max) {
        return false;
    }
    ++pending_stream_count;
    return true;
}

void HttpClientConnectionBase::releaseStreamReservation() {
    AutoLocker al(reserve_lock);
    if (pending_stream_count > 0) {
        --pending_stream_count;
    }
}

int HttpClientConnectionBase::getPendingStreamCount() const {
    AutoLocker al(reserve_lock);
    return pending_stream_count;
}

void HttpClientConnectionBase::setManager(HttpClientConnectionManagerBase* mgr) {
    AutoLocker al(onclose_lock);
    manager_ = mgr;
}

void HttpClientConnectionBase::onClosedHook() {
    // Read the back-pointer under our local lock, then release the lock
    // BEFORE invoking the manager method.  This breaks any potential
    // ordering issues between onclose_lock and manager.pool_lock for
    // app-thread paths that take pool_lock first.
    //
    // Lifetime safety: setManager(nullptr) is contractually required to
    // run before the manager destroys itself, so a non-null manager_
    // observed here is guaranteed alive for the duration of the call.
    HttpClientConnectionManagerBase* mgr;
    {
        AutoLocker al(onclose_lock);
        mgr = manager_;
    }
    if (mgr) {
        mgr->onConnectionClosed(this);
    }
}

QoreHashNode* HttpClientConnectionBase::submitRequest(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        ExceptionSink* xsink) {
    // Default: not implemented.  C++ subclasses (Http1ClientConnection etc.)
    // override with protocol-specific implementations.  Qore subclasses
    // override the QPP method at the Qore level.
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "submitRequest not implemented on this connection class");
    return nullptr;
}

int64_t HttpClientConnectionBase::submitRequestStreaming(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "submitRequestStreaming not implemented on this connection class");
    return -1;
}

int64_t HttpClientConnectionBase::submitRequestWithAction(const char* method, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        AbstractAsyncAction* action, ExceptionSink* xsink) {
    // Default: not supported.  H1/H2 subclasses override.  We own the
    // action on entry and must release it on failure so the caller does
    // not leak (matches the H1/H2 overrides which also deref on error).
    if (action) {
        action->deref(xsink);
    }
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "submitRequestWithAction not implemented on this connection class");
    return -1;
}

QoreHashNode* HttpClientConnectionBase::submitRequestStreamingSend(const char* method, const char* path,
        const QoreHashNode* headers, bool streaming_recv,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "submitRequestStreamingSend not implemented on this connection class");
    return nullptr;
}

void HttpClientConnectionBase::pushSendData(const void* data, size_t len, ExceptionSink* xsink) {
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "pushSendData not implemented on this connection class");
}

void HttpClientConnectionBase::setTrailers(const QoreHashNode* trailers, ExceptionSink* xsink) {
    xsink->raiseException("HTTPCLIENT-NOT-IMPLEMENTED",
        "setTrailers not implemented on this connection class");
}

void HttpClientConnectionBase::setPoolKey(const std::string& key) {
    pool_key_ = key;
}

const std::string& HttpClientConnectionBase::getPoolKey() const {
    return pool_key_;
}

void HttpClientConnectionBase::closeConnection(ExceptionSink* xsink) {
    // Default: just transition the state machine to CLOSED.
    // C++ subclasses override to also abort the poll op and cancel the
    // controller submission.  Qore subclasses override at the Qore level.
    setClosed();
}

bool HttpClientConnectionBase::waitForReadyOrError(int64_t timeout_ms, ExceptionSink* xsink) {
    // Delegate to the AbstractHttpPollConnectionPriv condition-variable wait.
    bool ready = AbstractHttpPollConnectionPriv::waitForReady(timeout_ms);

    if (ready) {
        return true;
    }

    // CLOSED takes precedence over wasReady().  A connection can transition
    // CONNECTING → READY → CLOSED faster than this thread can observe the
    // READY tick (common with TLS handshakes that complete locally before
    // the peer's fatal alert is read — e.g. mutual-auth where the server
    // rejects the client cert: SSL_connect succeeds, ALPN is read,
    // onConnectionReady() fires, then the next read on the I/O thread
    // surfaces the alert and setClosed() fires).  Surfacing the error info
    // here (rather than returning true via wasReady()) ensures callers like
    // HttpClientConnectionManagerBase::request see SOCKET-SSL-ERROR instead
    // of the misleading "cannot submit request: connection is closed" they
    // would otherwise get from submitRequest on the dead connection.
    if (isClosed()) {
        ReferenceHolder<QoreHashNode> err(getReferencedErrorInfo(), xsink);
        const char* err_str = "HTTPCLIENT-CONNECT-ERROR";
        const char* desc_str = wasReady()
            ? "connection closed after handshake, before request could be sent"
            : "connection closed before READY";
        std::string err_storage;
        std::string desc_storage;
        if (err) {
            QoreValue err_v = err->getKeyValue("err");
            if (err_v.getType() == NT_STRING) {
                QoreStringValueHelper err_val(err_v);
                err_storage = err_val->c_str();
                err_str = err_storage.c_str();
            }
            QoreValue desc_v = err->getKeyValue("desc");
            if (desc_v.getType() == NT_STRING) {
                QoreStringValueHelper desc_val(desc_v);
                desc_storage = desc_val->c_str();
                desc_str = desc_storage.c_str();
            }
        }
        xsink->raiseException(err_str, "%s", desc_str);
        return false;
    }

    // Connection went READY → DRAINING (e.g. GOAWAY received) without
    // dropping into CLOSED.  Existing streams may continue; the take-over /
    // submit path will decide whether DRAINING is acceptable for its
    // workload.  Report success so the caller can proceed.
    if (wasReady()) {
        return true;
    }

    // Timeout: no exception, caller decides what to do.
    return false;
}

void HttpClientConnectionBase::raiseClosedSubmitError(const char* fallback_desc,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> err(getReferencedErrorInfo(), xsink);
    if (*xsink) {
        return;
    }
    if (err) {
        const char* err_str = "HTTPCLIENT-CONNECT-ERROR";
        const char* desc_str = fallback_desc;
        std::string err_storage;
        std::string desc_storage;

        QoreValue err_v = err->getKeyValue("err");
        if (err_v.getType() == NT_STRING) {
            QoreStringValueHelper err_val(err_v);
            err_storage = err_val->c_str();
            err_str = err_storage.c_str();
        }
        QoreValue desc_v = err->getKeyValue("desc");
        if (desc_v.getType() == NT_STRING) {
            QoreStringValueHelper desc_val(desc_v);
            desc_storage = desc_val->c_str();
            desc_str = desc_storage.c_str();
        }
        xsink->raiseException(err_str, "%s", desc_str);
        return;
    }
    xsink->raiseException("HTTPCLIENT-STATE-ERROR", "%s", fallback_desc);
}

// -----------------------------------------------------------------------
// MethodGuard — see HttpClientConnection.h for the design rationale.
//
// The guard's job is to make "Qore code cannot crash libqore" hold for
// concurrent `delete rc` + `rc.get()` on different threads.  Entry CAS-
// increments the in-flight count unless the invalidated bit is set;
// exit decrements and, on the last release after invalidation, wakes
// the thread blocked in drainInFlight().
//
// Re-entrancy: method implementations may call into each other on the
// same thread (e.g., Http1ClientConnection::pushSendData(void*) forwards
// to pushSendData(QoreStringNode*), each of which guards).  A thread-
// local depth map tracks how deeply this thread has guarded a given
// connection; only the OUTERMOST guard touches the shared atomic.
// Inner guards succeed unconditionally because the outer one is still
// holding the count — the connection cannot have drained to zero while
// this thread is still inside a guarded method.  This costs one map
// lookup on entry/exit, amortized against the far-more-expensive atomic
// CAS that first-entry pays.
// -----------------------------------------------------------------------

namespace {
    // Per-thread re-entrancy map: HttpClientConnectionBase* → depth.
    // Only one entry per connection guarded on this thread; absent when
    // not currently guarded.  Cleared fully on thread exit.
    thread_local std::unordered_map<HttpClientConnectionBase*, int>
        s_method_guard_depth;
}

HttpClientConnectionBase::MethodGuard::MethodGuard(
        HttpClientConnectionBase* c)
    : conn_(c), acquired_(false) {
    auto& depth = s_method_guard_depth[conn_];
    if (depth > 0) {
        // Nested call on the same thread — the outer guard is still
        // holding a count, so we're under the protection of the outer
        // barrier.  Skip the atomic CAS entirely; just bump the depth.
        ++depth;
        acquired_ = true;
        return;
    }
    // First guard on this thread: do the full acq-rel CAS loop.
    uint32_t expected = conn_->lifetime_state_.load(std::memory_order_acquire);
    for (;;) {
        if (expected & INVALIDATED_BIT) {
            // Connection is being (or has been) closed — refuse entry.
            // Caller checks acquired() and raises a Qore exception.
            // Clean up the (still-zero) depth entry we just inserted.
            s_method_guard_depth.erase(conn_);
            return;
        }
        // Defensive: the low 31 bits cap at IN_FLIGHT_MASK.  In practice
        // it's impossible to have >2^31 concurrent calls on a single
        // connection, but loop guard anyway to keep the counter domain
        // well-defined under adversarial scenarios.
        if ((expected & IN_FLIGHT_MASK) == IN_FLIGHT_MASK) {
            s_method_guard_depth.erase(conn_);
            return;
        }
        uint32_t desired = expected + 1;
        if (conn_->lifetime_state_.compare_exchange_weak(
                expected, desired,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            acquired_ = true;
            depth = 1;
            return;
        }
        // CAS failure refreshes `expected` with the observed value; loop.
    }
}

HttpClientConnectionBase::MethodGuard::~MethodGuard() {
    if (!acquired_) {
        return;
    }
    auto it = s_method_guard_depth.find(conn_);
    assert(it != s_method_guard_depth.end() && it->second > 0);
    if (--it->second > 0) {
        // Nested release: outer guard still holds the atomic count.
        return;
    }
    s_method_guard_depth.erase(it);

    // Outermost release: decrement the shared atomic.
    uint32_t prev = conn_->lifetime_state_.fetch_sub(
        1, std::memory_order_acq_rel);
    // If we just dropped the LAST in-flight call after invalidation, the
    // closer thread is blocked in drainInFlight() waiting for exactly
    // this moment — wake it.  Take close_mu_ to pair with the wait's
    // unique_lock so the notify happens-before the waiter observes the
    // zero.  Without the lock the waiter could miss the edge if it
    // reloads lifetime_state_ between the fetch_sub here and the CV
    // wait call.
    if ((prev & INVALIDATED_BIT) && (prev & IN_FLIGHT_MASK) == 1) {
        std::lock_guard<std::mutex> lk(conn_->close_mu_);
        conn_->close_cv_.notify_all();
    }
}

uint32_t HttpClientConnectionBase::markInvalidated() {
    // Idempotent: fetch_or returns the previous value, so repeat calls
    // see the bit already set.  acq-rel so the bit becomes visible to
    // concurrent MethodGuard constructors before this returns.
    return lifetime_state_.fetch_or(INVALIDATED_BIT, std::memory_order_acq_rel);
}

void HttpClientConnectionBase::drainInFlight() {
    // Fast path: no call was in flight at invalidation time.  No need to
    // take the mutex / wait on the CV.
    uint32_t cur = lifetime_state_.load(std::memory_order_acquire);
    if ((cur & IN_FLIGHT_MASK) == 0) {
        return;
    }
    // Slow path: wait for the in-flight count to reach zero.  The last
    // MethodGuard to release observes the zero transition and notifies
    // close_cv_ under close_mu_; we take the mutex here and reload the
    // state under it so the notify happens-before our observation of
    // zero.
    std::unique_lock<std::mutex> lk(close_mu_);
    close_cv_.wait(lk, [this] {
        return (lifetime_state_.load(std::memory_order_acquire)
            & IN_FLIGHT_MASK) == 0;
    });
}
