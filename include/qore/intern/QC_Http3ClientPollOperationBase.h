/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Http3ClientPollOperationBase.h

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

#ifndef _QORE_CLASS_HTTP3CLIENTPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTP3CLIENTPOLLOPERATIONBASE_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QC_AbstractHttpPollConnection.h"

#include <algorithm>
#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

//! C++ base for Http3ClientPollOperation providing SocketPollOperationBase fast path
/** This class implements the HTTP/3 client poll state machine (connecting, reading,
    wait_read, closed) entirely in C++, enabling the AsyncIoController to use the
    direct C++ continuePoll() fast path instead of evalMethod().

    The inner SocketQuicClientPollOperation handles QUIC packet I/O (ngtcp2/nghttp3).
    This class manages the HTTP/3 application-level state: stream callbacks, response
    draining, connection readiness notification, and error handling.

    Thread safety: continuePoll() runs on the I/O thread. submitRequest helper methods
    (beginSubmit, endSubmitOk, registerStream, etc.) run on application threads.
    Shared state is protected by stream_lock with careful lock ordering:
    sock->priv->m (held by inner continuePoll) -> stream_lock.

    @since %Qore 2.3
*/
class Http3ClientPollOperationPriv : public SocketPollOperationBase {
public:
    //! HTTP/3 client connection states
    enum class H3State {
        CONNECTING,
        READING,
        WAIT_READ,
        CLOSED
    };

    //! Creates the poll operation wrapping an inner QUIC client operation
    /** @param self the QoreObject wrapping this private data
        @param sock the UDP socket (referenced by caller, ownership transferred)
        @param inner the SocketQuicClientPollOperation (referenced by caller, ownership transferred)
    */
    DLLLOCAL Http3ClientPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
            SocketQuicClientPollOperation* inner,
            AbstractHttpPollConnectionPriv* connection_priv);

    DLLLOCAL virtual ~Http3ClientPollOperationPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        return false;  // long-running multiplexed operation
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Stream management (called from Qore app thread) ---

    //! Phase 1: advisory capacity check under lock
    DLLLOCAL void checkCapacity(int max_streams, ExceptionSink* xsink);

    //! Phase 2 enter: increment submits_in_progress under lock
    DLLLOCAL void beginSubmit();

    //! Phase 2 exit (success): add to pending_stream_ids, decrement submits_in_progress
    DLLLOCAL void endSubmitOk(int64_t stream_id);

    //! Phase 2 exit (failure): decrement submits_in_progress
    DLLLOCAL void endSubmitFail();

    //! Phase 3: register completion action; returns buffered responses (or nullptr)
    /** @param stream_id the QUIC stream ID
        @param action the completion action (will be ref'd if stored)
        @param need_cancel set to true if connection died
        @param xsink for exception handling
        @return list of buffered response hashes (ref'd), or nullptr
    */
    DLLLOCAL QoreListNode* registerStream(int64_t stream_id, AbstractAsyncAction* action,
        bool& need_cancel, ExceptionSink* xsink);

    //! Cancel a stream: removes and derefs the action
    /** @param stream_id the QUIC stream ID
        @param xsink for exception handling
        @return true if the stream was found and cancelled
    */
    DLLLOCAL bool cancelStream(int64_t stream_id, ExceptionSink* xsink);

    //! Wait on stream_capacity_cond (for STREAM_ID_BLOCKED retry)
    DLLLOCAL void waitStreamCapacity(int64_t timeout_ms);

    //! Set error and close state (for connection death during submit)
    DLLLOCAL void setSubmitError(const char* err, const char* desc);

    // --- Accessors ---

    DLLLOCAL bool isClosed() const {
        return h3_state.load(std::memory_order_acquire) == H3State::CLOSED;
    }

    DLLLOCAL bool isReady() const {
        H3State s = h3_state.load(std::memory_order_acquire);
        return s == H3State::READING || s == H3State::WAIT_READ;
    }

    DLLLOCAL bool hasError() const {
        return error_info != nullptr;
    }

    DLLLOCAL QoreHashNode* getErrorInfo() const {
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    DLLLOCAL int getActiveStreamCount() const {
        return active_stream_count;
    }

    DLLLOCAL int getQuicStreamLimit() const {
        return quic_stream_limit;
    }

    DLLLOCAL void setQuicStreamLimit(int limit) {
        AutoLocker al(stream_lock);
        quic_stream_limit = limit;
    }

    DLLLOCAL int64_t getSessionId() const {
        return session_id.load(std::memory_order_acquire);
    }

    DLLLOCAL bool isQuicSessionOpen() const {
        if (!inner_op) {
            return false;
        }
        return inner_op->isOpen();
    }

    DLLLOCAL bool isGoawayReceived() const {
        return goaway_received.load(std::memory_order_acquire);
    }

    DLLLOCAL void setMigrationPending() {
        migration_pending.store(true, std::memory_order_release);
    }

    //! Returns the raw connection priv pointer (for I/O-thread calls)
    DLLLOCAL AbstractHttpPollConnectionPriv* getConnectionPriv() const {
        return connection_priv;
    }

    //! Get the socket object (returns a referenced QoreObject*)
    DLLLOCAL QoreObject* getReferencedSocket() const {
        if (self) {
            ExceptionSink xsink;
            return getReferencedSocketObject(&xsink);
        }
        return nullptr;
    }

    //! Registers a stream for onStreamData() notifications
    /** Called from Qore when setting up a CONNECT stream (WebSocket/SSE) to
        receive async notifications when data is dispatched for this stream.
        @param stream_id the QUIC stream ID
    */
    DLLLOCAL void registerStreamNotify(int64_t stream_id) {
        AutoLocker al(stream_lock);
        notify_streams.insert(stream_id);
    }

    //! Returns and clears the list of stream IDs with dispatched data
    /** Called by the controller after continuePoll() returns.
    */
    DLLLOCAL std::vector<int64_t> getAndClearDataReadyStreams() {
        std::vector<int64_t> result;
        result.swap(data_ready_streams);
        return result;
    }

    //! Cleanup all referenced objects (must be called before destructor)
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! Inner C++ QUIC client poll operation (ref'd)
    SocketQuicClientPollOperation* inner_op;

    //! The socket object (ref'd)
    QoreSocketObject* sock_obj;

    //! Connection state (atomic for lock-free reads from app threads)
    std::atomic<H3State> h3_state{H3State::CONNECTING};

    //! QUIC session ID (set when handshake completes)
    std::atomic<int64_t> session_id{0};

    //! True when GOAWAY received from server
    std::atomic<bool> goaway_received{false};

    //! Set by app thread, consumed by I/O thread
    std::atomic<bool> migration_pending{false};

    //! Consecutive empty reads counter
    int empty_read_count = 0;

    //! Set when a stream callback was dispatched during the current continuePoll
    bool response_dispatched = false;

    // --- Shared data (under stream_lock) ---

    mutable QoreThreadLock stream_lock;
    QoreCondition stream_capacity_cond;
    int stream_blocked_waiters = 0;
    int active_stream_count = 0;
    int submits_in_progress = 0;
    int quic_stream_limit = 0;

    //! Stream completion actions: stream_id string -> ref'd action
    /** Actions are pure C++ (no Qore interpreter) — execute/executeError
        are called directly on the I/O thread without execValue().
    */
    std::unordered_map<std::string, AbstractAsyncAction*> stream_actions;

    //! Pending responses buffered during submit race window: stream_id -> ref'd hashes
    std::unordered_map<std::string, std::vector<QoreHashNode*>> pending_responses;

    //! Stream IDs registered for onStreamData() notifications
    /** Populated by registerStreamNotify(); checked in handleReading() to signal
        data_ready_streams after ChannelAction dispatch.
    */
    std::unordered_set<int64_t> notify_streams;

    //! Stream IDs that had data dispatched in the last continuePoll cycle
    /** Populated by handleReading(); consumed by the controller via
        getAndClearDataReadyStreams() for onStreamData() worker dispatch.
    */
    std::vector<int64_t> data_ready_streams;

    //! Stream IDs between Phase 2 and Phase 3 of submit
    std::unordered_set<std::string> pending_stream_ids;

    //! Error info (ref'd or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Connection C++ priv (raw pointer — Qore internal_members "connection" holds the ref)
    /** Not ref'd in C++ to avoid GC-invisible reference cycles.
        The QPP constructor stores the strong QoreObject ref in a Qore
        internal_members slot where the DGC can see and break cycles.
        This raw pointer is used for direct I/O-thread calls (onConnectionReady).
    */
    AbstractHttpPollConnectionPriv* connection_priv = nullptr;

    static constexpr int MAX_DRAIN_ITERATIONS = 100;
    static constexpr int MAX_EMPTY_READS = 100;

    // --- Internal methods (I/O thread only) ---

    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleReading(ExceptionSink* xsink);
    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void notifyPendingStreams(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void cleanupPendingState();
    DLLLOCAL void fireReadyCallback(ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initHttp3ClientPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTP3CLIENTPOLLOPERATIONBASE_H
