/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Http2ClientPollOperationBase.h

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

#ifndef _QORE_CLASS_HTTP2CLIENTPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTP2CLIENTPOLLOPERATIONBASE_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QC_AbstractHttpPollConnection.h"
#include "qore/intern/RSet.h"

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

//! C++ base for Http2ClientPollOperation providing SocketPollOperationBase fast path
/** This class implements the HTTP/2 client poll state machine (connecting,
    ssl_upgrade, proxy_connect_send, proxy_connect_recv, reading, wait_read,
    closed) entirely in C++, enabling the AsyncIoController to use the direct
    C++ continuePoll() fast path instead of evalMethod().

    The inner operations handle TCP connect, SSL upgrade, proxy CONNECT tunnel,
    and HTTP/2 multiplexed frame I/O (via nghttp2). This class manages the
    application-level state: state transitions, stream actions, response
    dispatch, connection readiness, and error handling.

    Thread safety: continuePoll() runs on the I/O thread. submitRequest() runs
    on application threads. Shared state is protected by stream_lock, but the
    lock is never held while submitRequest() waits for controller-backed socket
    work to finish.

    @since %Qore 3.0
*/
class DLLLOCAL Http2ClientPollOperationPriv : public SocketPollOperationBase {
public:
    //! HTTP/2 client connection states
    enum class H2State {
        CONNECTING,
        SSL_UPGRADE,
        PROXY_CONNECT_SEND,
        PROXY_CONNECT_RECV,
        READING,
        WAIT_READ,
        CLOSED
    };

    //! Creates the poll operation with an initial TCP connect operation
    /** @param self the QoreObject wrapping this private data
        @param sock the TCP socket (referenced by caller, ownership transferred)
        @param connect_op the initial connect operation (referenced by caller, ownership transferred)
        @param ssl_required whether SSL/TLS is required for this connection
        @param proxy_tunnel whether a proxy CONNECT tunnel is needed (HTTPS through proxy)
        @param target_host the target hostname (for :authority pseudo-header and CONNECT)
        @param target_port the target port number
    */
    DLLLOCAL Http2ClientPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
            SocketPollOperationBase* connect_op, bool ssl_required, bool proxy_tunnel,
            std::string target_host, int target_port,
            AbstractHttpPollConnectionPriv* connection_priv);

    //! Creates the poll operation by adopting an already-connected and
    //! TLS-handshook socket (ALPN has already confirmed "h2").
    /** Used by @ref NegotiatingConnectionPollOp after per-connect ALPN
        negotiation selected HTTP/2.  The priv starts in
        @ref H2State::CONNECTING with no @c current_op; the connection
        class must call @ref initAdoptedMultiplex after @ref setSelf to
        install the multiplex inner op, transition to
        @ref H2State::READING, and fire the ready callback.

        Always implies @c ssl_required=true and no proxy tunnel — per
        the design doc §5.1, @ref HttpClientProtocol::NEGOTIATE only
        applies to TLS connections.

        @param self the QoreObject wrapping this private data (may be
            nullptr; set later via @ref setSelf)
        @param sock the already-connected (and SSL-handshook) socket
            (ref'd by caller, ownership transferred)
        @param target_host target hostname (for @c :authority pseudo-header)
        @param target_port target TCP port
        @param connection_priv the owning C++ connection priv

        @since %Qore 3.0
    */
    DLLLOCAL Http2ClientPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
            std::string target_host, int target_port,
            AbstractHttpPollConnectionPriv* connection_priv);

    //! Installs the HTTP/2 multiplex inner op on an adopt-socket-constructed
    //! poll op and transitions to READING + ready.
    /** Must be called after @ref setSelf and before the poll op is
        submitted to the I/O controller.  Only valid on instances created
        via the adopt-socket constructor.

        @param xsink exception sink

        @since %Qore 3.0
    */
    DLLLOCAL void initAdoptedMultiplex(ExceptionSink* xsink);

    DLLLOCAL virtual ~Http2ClientPollOperationPriv();

    //! Disarms the raw connection_priv back-pointer under stream_lock.
    /** Called by @ref Http2ClientConnection::closeConnection BEFORE the
        synchronous cancel() call.  After this returns, any concurrent or
        subsequent abort() on the I/O thread will see connection_priv ==
        nullptr and skip the setClosed() call, eliminating the race where
        abort() dereferences a destroyed connection.

        @since %Qore 3.0
    */
    DLLLOCAL void disarmConnectionPriv() {
        AutoLocker al(stream_lock);
        connection_priv = nullptr;
    }

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        return false;  // long-running multiplexed operation
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Stream management (called from Qore app thread) ---

    //! Submits an HTTP/2 request with headers and optional body
    /** Builds pseudo-headers, reserves capacity under stream_lock, submits via
        the controller-backed socket path, registers the completion action, and
        then wakes the I/O thread.

        @param method HTTP method (GET, POST, etc.)
        @param path request path (empty defaults to "/")
        @param user_headers optional user-provided headers (may include pseudo-headers)
        @param body request body pointer (or nullptr)
        @param body_len request body length
        @param streaming if true, submit without END_STREAM for bidirectional streaming
        @param action completion action — ownership transferred on success, deref'd on failure
        @param max_streams maximum concurrent streams (0 = unlimited)
        @param xsink exception sink
        @return stream ID on success, -1 on failure
    */
    DLLLOCAL int64_t submitRequest(const char* method, const char* path,
        const QoreHashNode* user_headers, const void* body, size_t body_len,
        bool streaming, AbstractAsyncAction* action, int max_streams,
        ExceptionSink* xsink);

    //! Cancel a stream: removes and notifies the action
    /** @param stream_id the HTTP/2 stream ID
        @param xsink for exception handling
        @return true if the stream was found and cancelled
    */
    DLLLOCAL bool cancelStream(int64_t stream_id, ExceptionSink* xsink);

    //! Send data on an existing stream
    /** @param stream_id the HTTP/2 stream ID
        @param data the data to send
        @param end_stream if true, signals end of stream
        @param xsink for exception handling
    */
    DLLLOCAL void sendStreamData(int64_t stream_id, const BinaryNode* data,
        bool end_stream, ExceptionSink* xsink);

    //! Installs a WebSocket frame-state decoder on a CONNECT stream's action
    /** Attaches a WebSocketStreamFrameState to the ChannelAction already
        registered for @a stream_id.  After this call, body bytes from H2
        DATA chunks dispatched by the I/O thread are fed into the decoder
        (in pure C++) and the resulting typed frame hashes are pushed to
        @a msg_queue; raw response hashes no longer flow through the
        handle's Channel.

        The install is race-free: the ChannelAction's per-action mutex
        serialises the install against all execute()/complete()/executeError()
        calls, and the install itself drains any items already buffered in
        the channel into the decoder BEFORE publishing the frame state.  No
        caller-side residual drain is required.

        @param stream_id    the H2 stream ID to upgrade
        @param msg_queue    Queue for typed frame hash delivery (ref transferred)
        @param initial      optional already-consumed response item whose body
                            bytes are fed to the decoder before the drain
        @param xsink        exception sink (raises HTTPCLIENT-STREAM-CLOSED if
                            the stream is not found or already complete)

        @since %Qore 3.0
    */
    DLLLOCAL void installFrameState(int32_t stream_id, Queue* msg_queue,
        QoreValue initial, ExceptionSink* xsink);

    //! Installs an SSE parser on an active ChannelAction stream
    DLLLOCAL void installSseState(int32_t stream_id, Queue* queue,
        QoreEventNotifier* notifier, QoreObject* notifier_obj, QoreValue initial,
        ExceptionSink* xsink);

    // --- Accessors ---

    DLLLOCAL bool isClosed() const {
        return h2_state.load(std::memory_order_acquire) == H2State::CLOSED;
    }

    DLLLOCAL bool isReady() const {
        H2State s = h2_state.load(std::memory_order_acquire);
        return (s == H2State::READING || s == H2State::WAIT_READ)
            && h2_confirmed.load(std::memory_order_acquire);
    }

    DLLLOCAL bool hasError() const {
        return error_info != nullptr;
    }

    DLLLOCAL QoreHashNode* getErrorInfo() const {
        AutoLocker al(stream_lock);
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    DLLLOCAL int getActiveStreamCount() const {
        AutoLocker al(stream_lock);
        return active_stream_count;
    }

    //! Sets the proactive idle timeout for this connection (microseconds).
    /** When the connection has no active streams for this duration,
        handleReading() will close it proactively — matching nginx's upstream
        keepalive_timeout behavior.  Defense-in-depth against peers that never
        close idle connections.

        @param timeout_us timeout in microseconds; <= 0 disables proactive timeout
    */
    DLLLOCAL void setIdleTimeout(int64_t timeout_us) {
        idle_timeout_us = timeout_us;
    }

    //! Sets the keepalive ping interval (microseconds, -1 = disabled)
    /** When positive, an HTTP/2 PING frame is submitted when the connection
        has been idle (no active streams) for this long.  nghttp2 handles the
        PING ACK automatically; if the connection is dead, the next recv()
        fails and existing error handling evicts it.
        @param interval_us ping interval in microseconds (-1 = disabled)
        @since %Qore 3.0
    */
    DLLLOCAL void setPingInterval(int64_t interval_us) {
        ping_interval_us = interval_us;
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

    //! Information for a registered CONNECT stream queue
    struct StreamQueueInfo {
        Queue* queue;                       //!< ref'd Queue for data delivery
        QoreObject* queue_obj;              //!< ref'd QoreObject wrapping queue (for DGC)
        QoreEventNotifier* notifier;        //!< ref'd EventNotifier for wake-up, or nullptr
        QoreObject* notifier_obj;           //!< ref'd QoreObject wrapping notifier, or nullptr
    };

    //! Registers a Queue (and optional EventNotifier) for a CONNECT stream
    /** Called from the Qore wrapper when dispatching an extended CONNECT handler.
        The Queue is used for non-blocking data delivery from the I/O thread.

        @param stream_id the HTTP/2 stream ID
        @param queue the Queue for data delivery (ref transferred)
        @param queue_obj the QoreObject wrapping queue (ref transferred)
        @param notifier optional EventNotifier for wake-up (ref transferred), or nullptr
        @param notifier_obj optional QoreObject wrapping notifier (ref transferred), or nullptr
    */
    DLLLOCAL void registerStreamQueue(int32_t stream_id, Queue* queue, QoreObject* queue_obj,
        QoreEventNotifier* notifier, QoreObject* notifier_obj);

    //! Returns and clears the list of stream IDs that had data drained
    /** Called by the controller after continuePoll() returns. For each
        stream ID, the controller dispatches onStreamData() to the worker pool.
    */
    DLLLOCAL std::vector<int32_t> getAndClearDataReadyStreams() {
        std::vector<int32_t> result;
        result.swap(data_ready_streams);
        return result;
    }

    //! Drains all registered stream queues after a read poll cycle
    /** Reads all available data from each registered stream's per-stream buffer
        and pushes it to the corresponding Queue.  When a stream is closed, a
        NOTHING sentinel is pushed and the registration is removed.

        @param xsink exception sink
    */
    DLLLOCAL void drainStreamQueues(ExceptionSink* xsink);

    //! Clears all stream queue registrations (called in abort/cleanup)
    /** Pushes NOTHING sentinels to all queues, notifies all EventNotifiers,
        and derefs all queue/notifier objects.

        @param xsink exception sink
    */
    DLLLOCAL void clearStreamQueues(ExceptionSink* xsink);

    //! Cleanup all referenced objects (must be called before destructor)
    DLLLOCAL void cleanup(ExceptionSink* xsink);

    //! Custom cycle scanner for stream_queues / pending_stream_registrations
    //! QoreObject refs that live in C++ containers invisible to the normal
    //! data/cdmap scan.
    /** Walks @ref stream_queues and @ref pending_stream_registrations
        and reports each ref'd @c queue_obj / @c notifier_obj via
        @c RObject::scanCheck().  Uses @c trylock on both @c stream_lock
        and @c sq_lock and returns false on contention — mirrors the
        non-blocking pattern of @ref qore_queue_private::scanMembers (a
        deadlock or a blocking wait here would freeze cycle detection).

        @param obj the @c RObject wrapping this private data (i.e. the
            Qore @c Http2ClientPollOperation object)
        @param rsh the scanner helper
        @return @c true if a lock error forces the scan transaction to
            be aborted (per the scanCheck contract)

        @since %Qore 3.0
    */
    DLLLOCAL bool scanMembers(RObject& obj, RSetHelper& rsh);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! Current inner poll operation (ref'd) — changes during state transitions
    SocketPollOperationBase* current_op;

    //! Protects current_op replacement against concurrent close()/abort()
    mutable QoreThreadLock op_lock;

    //! The socket object (ref'd by us — separate from inner op's ref)
    QoreSocketObject* sock_obj;

    //! Connection state (atomic for lock-free reads from app threads)
    std::atomic<H2State> h2_state{H2State::CONNECTING};

    //! True when H2 protocol is confirmed (ALPN for HTTPS, SETTINGS for h2c)
    std::atomic<bool> h2_confirmed{false};

    //! Whether SSL/TLS is required
    bool ssl_required;

    //! True when HTTPS target through proxy (requires CONNECT tunnel)
    bool proxy_tunnel;

    //! Target host for :authority pseudo-header and proxy CONNECT
    std::string target_host;

    //! Target port
    int target_port;

    //! Number of successful reading cycles (for h2c confirmation)
    int reading_cycle_count = 0;

    //! Consecutive empty reads counter
    int empty_read_count = 0;

    //! Set when a stream callback was dispatched during the current continuePoll
    bool response_dispatched = false;

    // --- Shared data (under stream_lock) ---

    mutable QoreThreadLock stream_lock;

    //! Serializes multiplex reads against submitRequest's stream-id registration window
    mutable QoreThreadLock submission_lock;

    //! Stream completion actions: stream_id string -> ref'd action
    /** Actions are pure C++ (no Qore interpreter) — execute/executeError
        are called directly on the I/O thread without execValue().
    */
    std::unordered_map<std::string, AbstractAsyncAction*> stream_actions;

    //! Completed raw streaming actions kept briefly for post-header parser install
    std::unordered_map<std::string, ChannelAction*> completed_channel_actions;

    int active_stream_count = 0;

    //! Error info (ref'd or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Connection C++ priv (raw pointer — Qore internal_members "connection" holds the ref)
    /** Not ref'd in C++ to avoid GC-invisible reference cycles.
        The QPP constructor stores the strong QoreObject ref in a Qore
        internal_members slot where the DGC can see and break cycles.
        This raw pointer is used for direct I/O-thread calls (onConnectionReady).
    */
    AbstractHttpPollConnectionPriv* connection_priv = nullptr;

    //! Lock protecting only pending_stream_registrations (app→I/O command queue)
    /** Separate from stream_lock to avoid lock-ordering inversion with the
        H2 session mutex.  Never held during I/O operations.
    */
    QoreThreadLock sq_lock;

    //! Pending stream queue registrations from app threads (under sq_lock)
    /** App threads push here; I/O thread drains at the start of
        drainStreamQueues().  Follows the nginx pattern: all mutable
        connection state is owned by the event loop thread.
    */
    std::vector<std::pair<int32_t, StreamQueueInfo>> pending_stream_registrations;

    // --- I/O-thread-only data (no lock needed) ---

    //! Registered stream queues — I/O-thread-only
    std::unordered_map<int32_t, StreamQueueInfo> stream_queues;

    //! Stream IDs that had data drained — I/O-thread-only
    std::vector<int32_t> data_ready_streams;

    //! Proactive idle timeout (microseconds); -1 = no proactive timeout
    /** Set by setIdleTimeout() after construction from the connection manager.
        When active_stream_count is 0 for this many microseconds, handleReading()
        closes the connection proactively with HTTP2-IDLE-TIMEOUT.  Mirrors
        nginx's upstream keepalive_timeout.  I/O thread only.
    */
    int64_t idle_timeout_us = -1;

    //! Deadline for the current idle period (epoch us); 0 = not yet armed
    /** Armed on the first handleReading() cycle that observes
        active_stream_count == 0; cleared whenever active_stream_count > 0 so
        the next idle period starts fresh.  I/O thread only.
    */
    int64_t idle_deadline_us = 0;

    //! Keepalive ping interval (microseconds); -1 = disabled
    /** When positive and active_stream_count == 0, a PING frame is
        submitted every ping_interval_us to probe connection liveness.
        nghttp2 handles PING ACK automatically.  I/O thread only.
    */
    int64_t ping_interval_us = -1;

    //! Timestamp of last PING sent (epoch us); 0 = none sent
    int64_t last_ping_sent_us = 0;

    static constexpr int MAX_DRAIN_ITERATIONS = 100;
    static constexpr int MAX_EMPTY_READS = 100;
    static constexpr int H2C_PROBE_MAX_EMPTY_READS = 5;

    // --- Internal methods (I/O thread only) ---

    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProxyConnectSend(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProxyConnectRecv(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleReading(ExceptionSink* xsink);

    //! Applies the proactive idle-timeout policy to a handleReading return value.
    /** If no idle timeout is configured or h2c is not yet confirmed, this is a
        no-op and returns \a poll_info unchanged.  Otherwise:
          - If any streams are active, clears the idle deadline and returns
            \a poll_info unchanged.
          - On the first idle cycle, arms the deadline to \c now + idle_timeout_us
            and annotates \a poll_info with a \c poll_timeout_ms so the
            controller wakes continuePoll() when the deadline expires.
          - On subsequent idle cycles, if the deadline has elapsed, calls
            \c setError("HTTP2-IDLE-TIMEOUT", ...), derefs \a poll_info, and
            returns \c nullptr so the caller exits continuePoll cleanly.

        All decision-making runs under \c stream_lock in one critical section to
        avoid a TOCTOU race with an app-thread submit.  Mirrors nginx's
        upstream keepalive_timeout with a per-connection timer.

        @param poll_info the poll_info hash about to be returned from
               handleReading (may be \c nullptr)
        @param xsink exception sink
        @return the (possibly annotated) \a poll_info, or \c nullptr when the
                connection has been closed due to idle timeout
    */
    DLLLOCAL QoreHashNode* applyIdleTimeout(QoreHashNode* poll_info, ExceptionSink* xsink);

    DLLLOCAL void startSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL void startProxyConnect(ExceptionSink* xsink);
    DLLLOCAL void startMultiplex(ExceptionSink* xsink);

    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void notifyPendingStreams(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void fireReadyCallback(ExceptionSink* xsink);

    //! Returns a referenced snapshot of the current inner operation
    DLLLOCAL SocketPollOperationBase* refCurrentOp() const;

    //! Installs a new inner operation unless the outer operation has been closed
    DLLLOCAL bool replaceCurrentOp(SocketPollOperationBase* op, H2State state, ExceptionSink* xsink);

    //! Release the current inner operation
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initHttp2ClientPollOperationBaseClass(QoreNamespace& qorens);

//! Class ID — defined by the generated QPP code (qore_classid_t storage in
//! the generated .cpp file, initialised from the registered class).
DLLLOCAL extern qore_classid_t CID_HTTP2CLIENTPOLLOPERATIONBASE;

#endif // _QORE_CLASS_HTTP2CLIENTPOLLOPERATIONBASE_H
