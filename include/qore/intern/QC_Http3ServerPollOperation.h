/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Http3ServerPollOperation.h

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

#ifndef _QORE_CLASS_HTTP3SERVERPOLLOPERATION_H

#define _QORE_CLASS_HTTP3SERVERPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"

#include <atomic>
#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations
class Queue;
class QoreSSLCertificate;
class QoreSSLPrivateKey;

//! C++ implementation of Http3ServerPollOperation for I/O thread execution
/** Manages an HTTP/3 server connection lifecycle entirely in C++ so that
    continuePoll() runs on the I/O thread (same thread every time).  This fixes
    the STREAM-THREAD-ERROR caused by running Qore poll ops on random worker
    threads, where the one-shot reassignThread() on BinaryInputStream fails
    after the first call.

    State machine:
    - INIT -> READING -> REQUEST_READY -> SENDING -> SENT -> READING (loop)
    - DRAINING -> CLOSED (graceful shutdown via GOAWAY)

    Thread safety:
    - continuePoll() runs on the I/O thread (via SocketPollOperationBase fast path)
    - Handler-thread methods (startSendResponse, continueReading, shutdown) modify
      state under op_lock.  The I/O thread also acquires op_lock in continuePoll().
    - Completion context methods and stream registration are called from Qore code
      on handler/callback threads.

    Ownership model:
    - This class owns (refs): sock_obj, read_op, send_op, cached_request, error_info
    - cleanup() releases ALL refs — called from QPP destructor before deref()
    - getOutput() transfers cached_request ownership to caller
    - Stream queue and listener objects are stored as QoreObject* refs in the QPP layer

    @since %Qore 2.3
*/
class Http3ServerPollOperationPriv : public SocketPollOperationBase {
public:
    //! HTTP/3 server connection states
    enum class H3State {
        INIT,
        READING,
        WAIT_READ,
        REQUEST_READY,
        SENDING,
        SENT,
        DRAINING,
        CLOSED
    };

    //! Creates the poll operation
    /** @param self the QoreObject wrapping this private data
        @param sock the QoreSocketObject (ref'd by caller, ownership transferred)
        @param drain_timeout drain timeout
        @param headers_only if true, dispatch on HEADERS before full body
    */
    DLLLOCAL Http3ServerPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
            std::chrono::microseconds drain_timeout, bool headers_only);

    DLLLOCAL virtual ~Http3ServerPollOperationPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        H3State s = h3_state.load(std::memory_order_acquire);
        return s == H3State::REQUEST_READY || s == H3State::SENT;
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Handler-thread methods (called from Qore code under op_lock) ---

    //! Starts sending an HTTP/3 response
    /** Creates a SocketQuicSendResponsePollOperation and transitions to SENDING.
        @param status_code HTTP status code
        @param headers response headers (may be nullptr)
        @param body response body (may be nullptr)
        @param xsink exception sink
    */
    DLLLOCAL void startSendResponse(int status_code, const QoreHashNode* headers,
            const AbstractQoreNode* body, ExceptionSink* xsink);

    //! Starts sending a streaming HTTP/3 response
    /** Creates a SocketQuicSendStreamingResponsePollOperation and transitions to SENDING.
        @param status_code HTTP status code
        @param headers response headers (may be nullptr)
        @param body_stream InputStream providing body data (ownership transferred)
        @param body_obj QoreObject wrapping the InputStream (ref'd, ownership transferred)
        @param chunk_size chunk size for reading from the InputStream
        @param xsink exception sink
    */
    DLLLOCAL void startSendStreamingResponse(int status_code, const QoreHashNode* headers,
            InputStream* body_stream, QoreObject* body_obj, int64_t chunk_size,
            ExceptionSink* xsink);

    //! Starts reading the next request (after sending a response)
    DLLLOCAL void startReadNextRequest(ExceptionSink* xsink);

    //! Continues reading without creating a new read operation
    DLLLOCAL void continueReading(ExceptionSink* xsink);

    //! Gracefully shuts down the QUIC connection
    /** @param session_id optional session ID; if 0, uses the cached request's session_id
    */
    DLLLOCAL void shutdown(int64_t session_id, ExceptionSink* xsink);

    // --- Stream data dispatch ---

    //! Registers a Queue for stream data delivery
    /** @param stream_key composite key "session_id:stream_id"
        @param queue the C++ QoreQueue pointer (not ref'd — QoreObject ref held in QPP layer)

        Closes the H3 op's shutdown race for extended-CONNECT (RFC 9220) tunnels:
        if the H3 op was already aborted (so @c stream_queues_closed is set) the
        caller's Queue would otherwise be stranded with no I/O thread to drain
        it.  In that case we push a NOTHING sentinel directly to the Queue so
        the caller's read loop unblocks immediately, and skip the map insert
        (the closed Queue won't receive any more data).
    */
    DLLLOCAL void registerStreamQueue(const std::string& stream_key, Queue* queue) {
        AutoLocker al(op_lock);
        if (stream_queues_closed) {
            // Push the NOTHING sentinel so the handler thread's read loop
            // exits cleanly.  Don't deref the Queue — the QoreObject ref is
            // held in the QPP layer's `stream_queue_objs` member hash and
            // is released by the standard internal_members deref path.
            queue->pushAndTakeRef(QoreValue());
            return;
        }
        stream_queues[stream_key] = queue;
    }

    //! Records stream data that arrived before a listener was registered
    /** @param stream_key composite key "session_id:stream_id"
    */
    DLLLOCAL void markStreamDataPending(const std::string& stream_key) {
        AutoLocker al(op_lock);
        pending_stream_data.insert(stream_key);
    }

    //! Consumes a pending stream-data notification or detects queued data
    /** Called after listener registration to replay notifications that raced
        with the handler setup path.  The queue check covers the window where
        data has already been pushed to the Queue but the worker-dispatched
        onStreamData() callback has not run yet.

        @param stream_key composite key "session_id:stream_id"
        @return true if the listener should be notified immediately
    */
    DLLLOCAL bool consumePendingOrQueueReady(const std::string& stream_key) {
        AutoLocker al(op_lock);
        bool pending = pending_stream_data.erase(stream_key) > 0;
        auto i = stream_queues.find(stream_key);
        bool queue_ready = i != stream_queues.end() && i->second && !i->second->empty();
        return pending || queue_ready;
    }

    //! Returns and clears the list of stream keys with data available
    /** Called by the I/O controller after continuePoll().
    */
    DLLLOCAL std::vector<std::string> getAndClearDataReadyStreams() {
        AutoLocker al(op_lock);
        std::vector<std::string> result;
        result.swap(data_ready_streams);
        return result;
    }

    //! Registers a Queue to receive the persistent-session close signal
    /** Bound to a QUIC session: when the peer resets a stream on that session or
        the session closes, continuePoll() pushes a \c {"type":"close"} hash to
        this Queue — the same I/O-thread Queue-push path used for stream data —
        so a persistent dedicated handler thread parked on the Queue is released
        and runs its teardown on the binding thread.  The QoreObject ref that
        keeps the Queue alive is held in the QPP layer's
        \c persistent_session_queue_objs member hash.

        @param session_id the QUIC session ID
        @param queue the C++ QoreQueue pointer (not ref'd here)
    */
    DLLLOCAL void registerPersistentSessionQueue(int64_t session_id, Queue* queue) {
        AutoLocker al(op_lock);
        persistent_session_queues_[session_id] = queue;
    }

    //! Deregisters the persistent-session close Queue for a session
    /** Erases the C++ map entry under op_lock; the QPP layer drops the
        QoreObject ref only after this returns, so a concurrent continuePoll()
        delivery on the I/O thread either sees the entry (Queue still alive via
        the held ref) or sees nothing.
    */
    DLLLOCAL void deregisterPersistentSessionQueue(int64_t session_id) {
        AutoLocker al(op_lock);
        persistent_session_queues_.erase(session_id);
    }

    // --- Accessors ---

    DLLLOCAL bool isClosed() const {
        return h3_state.load(std::memory_order_acquire) == H3State::CLOSED;
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

    DLLLOCAL bool shouldKeepAlive() const {
        H3State s = h3_state.load(std::memory_order_acquire);
        return keep_alive && s != H3State::CLOSED && s != H3State::DRAINING && !error_info;
    }

    //! Cleanup all referenced objects (must be called before destructor)
    DLLLOCAL void cleanup(ExceptionSink* xsink);

    //! Close all registered stream queues and reject future registrations
    /** Sets @c stream_queues_closed=true under op_lock and pushes a NOTHING
        sentinel to every entry in @c stream_queues so handler threads
        blocked in @c Queue::get() on RFC 9220 extended-CONNECT tunnels
        unblock immediately.  Mirrors @c Http2PollOperationPriv::clearStreamQueues
        — the H2 fix in commit @c af761a3d9 closes the same race for the H2
        side, this is the H3 analogue.

        Called from @ref abort() and @ref cleanup() so any subsequent
        @ref registerStreamQueue() call from a handler thread that races
        the shutdown observes the closed flag and pushes the sentinel
        directly to its caller's Queue (without inserting into the map).
    */
    DLLLOCAL void closeStreamQueues();

    //! Starts the read request operation (creates a new SocketQuicServerPollOperation)
    /** Called from the QPP constructor and from startReadNextRequest().
    */
    DLLLOCAL void startReadRequest(ExceptionSink* xsink);

    //! Sets the maximum request body size on the underlying QUIC server operation
    DLLLOCAL void setMaxRequestBodySize(int64_t size);

    //! Stages a certificate / private key swap to be applied on the I/O thread
    /** Safe to call from any thread. The actual swap happens at the top of the
        next continuePoll() iteration on the I/O thread, where we can update the
        udp_sock's cert/pk and free the cached shared QUIC server SSL_CTX without
        deadlocking against the I/O thread's own hold on the socket mutex.

        cert and pk must already be referenced by the caller; ownership of those
        references transfers to this object (consumed on apply or in cleanup()).
    */
    DLLLOCAL void setPendingCertificate(QoreSSLCertificate* cert, QoreSSLPrivateKey* pk);

    //! Protects Qore-side stream listener / queue object member maps
    /** The C++ queue state is protected by @c op_lock.  The QPP layer also keeps
        QoreObject refs in internal member hashes so objects remain alive while a
        stream is active.  Those hashes are mutated from handler and worker
        threads, so their copy/replace updates need separate serialization.
    */
    mutable QoreThreadLock stream_member_lock;

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! Inner C++ read operation (ref'd, or nullptr)
    SocketQuicServerPollOperation* read_op = nullptr;

    //! Inner C++ send operation (ref'd, or nullptr; only during SENDING)
    SocketPollOperationBase* send_op = nullptr;

    //! The socket object (ref'd)
    QoreSocketObject* sock_obj = nullptr;

    //! Connection state (atomic for lock-free reads; mutable for getOutput() const)
    mutable std::atomic<H3State> h3_state{H3State::INIT};

    //! Mutex for thread safety between I/O thread and handler threads
    mutable QoreThreadLock op_lock;

    //! Cached completed request hash (ref'd, or nullptr)
    mutable QoreHashNode* cached_request = nullptr;

    //! Error info hash (ref'd, or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Keep-alive flag (QUIC connections are persistent)
    bool keep_alive = true;

    //! Headers-only mode
    bool headers_only;

    //! Effective drain timeout
    std::chrono::microseconds drain_timeout;

    //! Consecutive empty reads counter
    mutable int empty_read_count = 0;

    //! Timestamp when draining started
    std::chrono::steady_clock::time_point drain_start;

    //! Session ID being drained
    int64_t drain_session_id = 0;

    //! Set to true by continuePoll() when a request is stored; cleared by getOutput()
    mutable bool request_pending = false;

    //! Registered stream data Queues (composite key -> C++ Queue*)
    /** The Queue objects are ref'd at the QoreObject level in the QPP layer.
        These raw pointers are used for empty() checks on the I/O thread
        without Qore interpreter overhead.
    */
    std::unordered_map<std::string, Queue*> stream_queues;

    //! Set under @c op_lock at the start of @ref closeStreamQueues() so any concurrent
    //! @ref registerStreamQueue() call from a handler thread that races shutdown
    //! short-circuits and pushes the NOTHING sentinel directly to the caller's
    //! Queue.  Mirrors @c Http2PollOperationPriv::stream_queues_closed.
    bool stream_queues_closed = false;

    //! Stream data notifications that arrived before listener registration
    std::unordered_set<std::string> pending_stream_data;

    //! Stream keys with data available (populated by continuePoll, consumed by controller)
    std::vector<std::string> data_ready_streams;

    //! Persistent-session close-delivery Queues (session_id -> C++ Queue*)
    /** Bound via registerPersistentSessionQueue(); continuePoll() pushes a
        {"type":"close"} hash on peer stream reset / session close.  The Queue
        objects are ref'd at the QoreObject level in the QPP layer
        (persistent_session_queue_objs) so the raw pointers stay valid across the
        I/O-thread push.  Protected by op_lock.
    */
    std::unordered_map<int64_t, Queue*> persistent_session_queues_;

    //! Lock for the pending-cert staging slot (separate from op_lock and the
    //! socket's priv->m so that setPendingCertificate() never contends with I/O)
    mutable QoreThreadLock pending_cert_lock;

    //! Pending replacement certificate (ref'd, or nullptr)
    QoreSSLCertificate* pending_cert = nullptr;

    //! Pending replacement private key (ref'd, or nullptr)
    QoreSSLPrivateKey* pending_pk = nullptr;

    //! Cheap flag probed on each continuePoll() entry to skip the lock fast path
    std::atomic<bool> has_pending_cert{false};

    static constexpr int MAX_EMPTY_READS = 100;

    // --- Internal methods (I/O thread only) ---

    DLLLOCAL QoreHashNode* handleReading(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSending(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleDraining(ExceptionSink* xsink);
    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);

    //! Check stream queues and populate data_ready_streams
    DLLLOCAL void checkStreamQueues();

    //! Deliver read_op's per-session lifecycle events to bound persistent-session
    //! Queues.  Drains peer-reset + session-close events from read_op and pushes
    //! {"type":"close"} to any persistent_session_queues_ entry for the affected
    //! session.  Called from continuePoll() under op_lock (I/O thread).
    DLLLOCAL void deliverSessionLifecycleEvents();

    //! Apply any staged cert/key swap; runs on the I/O thread before op_lock
    DLLLOCAL void applyPendingCertificate();
};

DLLLOCAL QoreClass* initHttp3ServerPollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTP3SERVERPOLLOPERATION_H
