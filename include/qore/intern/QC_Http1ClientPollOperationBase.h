/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_Http1ClientPollOperationBase.h

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

#ifndef _QORE_CLASS_HTTP1CLIENTPOLLOPERATIONBASE_H

#define _QORE_CLASS_HTTP1CLIENTPOLLOPERATIONBASE_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QC_AbstractHttpPollConnection.h"

#include <atomic>
#include <string>

//! C++ base for Http1ClientPollOperation providing SocketPollOperationBase fast path
/** This class implements the HTTP/1.1 client poll state machine (connecting,
    ssl_upgrade, proxy_connect, reading/sending, closed) entirely in C++,
    enabling the AsyncIoController to use the direct C++ continuePoll() fast
    path instead of evalMethod().

    HTTP/1.1 is a serial protocol — only one request can be in flight at a
    time. The request/response sub-state machine handles: idle, sending,
    recv_header, recv_body_length (Content-Length), recv_chunk_size/data/crlf
    (chunked TE), and recv_body_close (connection-close).

    Thread safety: continuePoll() runs on the I/O thread. submitRequest() runs
    on application threads. Shared state is protected by stream_lock.

    @since %Qore 2.3
*/
class Http1ClientPollOperationPriv : public SocketPollOperationBase {
public:
    //! HTTP/1.1 connection states
    enum class H1State {
        CONNECTING,
        SSL_UPGRADE,
        PROXY_CONNECT_SEND,
        PROXY_CONNECT_RECV,
        READING,
        WAIT_READ,
        PROTOCOL_SWITCHED,  //!< 101 Switching Protocols received; raw bidirectional I/O
        CLOSED
    };

    //! Request sub-states within READING
    enum class ReqState {
        IDLE,
        SENDING,
        RECV_HEADER,
        RECV_BODY_LENGTH,
        RECV_CHUNK_SIZE,
        RECV_CHUNK_DATA,
        RECV_CHUNK_CRLF,
        RECV_BODY_CLOSE,
        PROTOCOL_SWITCHED   //!< raw bidirectional data after 101
    };

    //! Creates the poll operation with an initial TCP connect operation
    DLLLOCAL Http1ClientPollOperationPriv(QoreObject* self, QoreSocketObject* sock,
            SocketPollOperationBase* connect_op, bool ssl_required, bool proxy_tunnel,
            bool is_proxy_plain, std::string target_host, int target_port,
            AbstractHttpPollConnectionPriv* connection_priv);

    DLLLOCAL virtual ~Http1ClientPollOperationPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        // Long-running serial connection — never "done" during normal HTTP.
        // After 101 Switching Protocols, the HTTP layer IS done — the socket
        // transfers to a WebSocket poll operation.
        return protocol_switched;
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Stream management (called from Qore app thread) ---

    //! Submits an HTTP/1.1 request with headers and optional body
    DLLLOCAL int64_t submitRequest(const char* method, const char* path,
        const QoreHashNode* user_headers, const void* body, size_t body_len,
        bool streaming, AbstractAsyncAction* action, int max_streams,
        ExceptionSink* xsink);

    //! Cancel a stream
    DLLLOCAL bool cancelStream(int64_t stream_id, ExceptionSink* xsink);

    // --- Accessors ---

    DLLLOCAL bool isClosed() const {
        return h1_state.load(std::memory_order_acquire) == H1State::CLOSED;
    }

    DLLLOCAL bool isReady() const {
        H1State s = h1_state.load(std::memory_order_acquire);
        return s == H1State::READING || s == H1State::WAIT_READ
            || s == H1State::PROTOCOL_SWITCHED;
    }

    DLLLOCAL bool isProtocolSwitched() const {
        return h1_state.load(std::memory_order_acquire) == H1State::PROTOCOL_SWITCHED;
    }

    DLLLOCAL void setProtocolSwitchedListener(QoreObject* listener, ExceptionSink* xsink) {
        if (protocol_switched_listener) {
            protocol_switched_listener->deref(xsink);
        }
        if (listener) {
            listener->ref();
        }
        protocol_switched_listener = listener;
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

    //! Sets the proactive idle timeout for this connection (microseconds).
    /** When a connection has been idle (no pending request, no active stream)
        for this duration, handleIdle() will close it proactively — matching
        nginx's upstream keepalive_timeout behavior.  Provides defense-in-depth
        against peers that never close idle connections.

        @param timeout_us timeout in microseconds; <= 0 disables proactive timeout
    */
    DLLLOCAL void setIdleTimeout(int64_t timeout_us) {
        idle_timeout_us = timeout_us;
    }

    DLLLOCAL AbstractHttpPollConnectionPriv* getConnectionPriv() const {
        return connection_priv;
    }

    DLLLOCAL QoreObject* getReferencedSocket() const {
        if (self) {
            ExceptionSink xsink;
            return getReferencedSocketObject(&xsink);
        }
        return nullptr;
    }

    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! Current inner poll operation (ref'd)
    SocketPollOperationBase* current_op;

    //! The socket object (ref'd)
    QoreSocketObject* sock_obj;

    //! Connection state (atomic for lock-free reads from app threads)
    std::atomic<H1State> h1_state{H1State::CONNECTING};

    //! Whether SSL/TLS is required
    bool ssl_required;

    //! True when HTTPS target through proxy (requires CONNECT tunnel)
    bool proxy_tunnel;

    //! True when using plain HTTP through a proxy (absolute URI in request line)
    bool is_proxy_plain;

    //! Target host for Host header and proxy CONNECT
    std::string target_host;

    //! Target port
    int target_port;

    //! Consecutive empty reads counter
    int empty_read_count = 0;

    //! Set when a response was dispatched during the current continuePoll
    bool response_dispatched = false;

    //! Connection C++ priv (raw pointer — internal_members holds the ref)
    AbstractHttpPollConnectionPriv* connection_priv = nullptr;

    // --- Request sub-state (I/O thread only, no lock needed) ---

    ReqState req_state = ReqState::IDLE;

    //! Response status code
    int response_status_code = 0;

    //! Response status message (e.g., "OK", "Not Found"); ref'd or nullptr
    QoreStringNode* response_status_message = nullptr;

    //! Response HTTP version string (e.g., "1.1", "1.0"); ref'd or nullptr
    QoreStringNode* response_http_version = nullptr;

    //! Response headers (ref'd or nullptr, lowercase keys)
    QoreHashNode* response_headers = nullptr;

    //! Accumulated response body (ref'd or nullptr)
    BinaryNode* response_body = nullptr;

    //! Whether response uses chunked transfer encoding
    bool response_chunked = false;

    //! Content-Length from response headers (-1 = unknown)
    int64_t response_content_length = -1;

    //! Whether the server sent Connection: close
    bool connection_close = false;

    //! Whether the current request is in streaming mode (copied from shared state)
    bool streaming_active_io = false;

    //! True after a 101 Switching Protocols response has been received
    bool protocol_switched = false;

    //! Timestamp (epoch microseconds) when recv-header started on a reused
    //! connection, or 0 if stale detection is inactive.
    int64_t stale_detect_start_us = 0;

    //! Remaining bytes for streaming body with Content-Length
    int64_t streaming_body_remaining = 0;

    //! Proactive idle timeout (microseconds); -1 = no proactive timeout
    /** Set by setIdleTimeout() after construction from the connection manager.
        When the connection is in ReqState::IDLE with no pending request and
        this many microseconds pass without activity, handleIdle() closes the
        connection proactively with HTTP1-IDLE-TIMEOUT.  Mirrors nginx's
        upstream keepalive_timeout; I/O thread only.
    */
    int64_t idle_timeout_us = -1;

    //! Deadline for the current idle period (epoch us); 0 = not yet in idle wait
    /** Set on first handleIdle entry with no pending request; cleared when a
        pending request is picked up or the connection leaves the idle state.
        I/O thread only.
    */
    int64_t idle_deadline_us = 0;

    // --- Shared data (under stream_lock) ---

    mutable QoreThreadLock stream_lock;

    //! Single completion action (H1 is serial — at most one)
    AbstractAsyncAction* stream_action = nullptr;

    //! Stream ID of the active action
    int active_sid = 0;

    //! Active request method (for HEAD / 1xx / 204 / 304 handling)
    std::string active_method;

    //! Next stream ID counter
    int next_stream_id = 0;

    //! Number of active streams (0 or 1 for H1)
    int active_stream_count = 0;

    //! Pending request binary data (ref'd or nullptr)
    BinaryNode* pending_request_data = nullptr;

    //! Whether the current request uses streaming mode
    bool streaming_active = false;

    //! Error info (ref'd or nullptr)
    QoreHashNode* error_info = nullptr;

    //! Protocol-switched notifier object (ref'd or nullptr)
    /** Called from the I/O thread after raw data is dispatched in
        PROTOCOL_SWITCHED state.  Used by WebSocketClientHcioPollOp to
        trigger drainAndParseFrames() when data arrives.
    */
    QoreObject* protocol_switched_listener = nullptr;

    static constexpr int MAX_EMPTY_READS = 100;
    static constexpr size_t STREAMING_CHUNK_SIZE = 16384;

    //! Stale keep-alive detection timeout (milliseconds).
    /** On a reused connection, the controller wakes continuePoll after this
        interval even without socket events, giving us a chance to recv-peek
        for a delayed server close (TCP FIN race).
    */
    static constexpr int64 STALE_DETECT_TIMEOUT_MS = 500;

    // --- Internal methods (I/O thread only) ---

    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProxyConnectSend(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProxyConnectRecv(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleReading(ExceptionSink* xsink);

    DLLLOCAL void startSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL void startProxyConnect(ExceptionSink* xsink);
    DLLLOCAL void startReady(ExceptionSink* xsink);

    // Request sub-state handlers
    DLLLOCAL QoreHashNode* handleIdle(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSending(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvHeader(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvBodyLength(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvChunkSize(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvChunkData(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvChunkCrlf(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleRecvBodyClose(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProtocolSwitched(ExceptionSink* xsink);

    // Response dispatch
    DLLLOCAL QoreHashNode* dispatchResponse(ExceptionSink* xsink);
    DLLLOCAL void dispatchStreamingHeaders(ExceptionSink* xsink);
    DLLLOCAL void dispatchStreamingData(const BinaryNode* data, ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* dispatchStreamingEnd(ExceptionSink* xsink);
    DLLLOCAL void resetResponseState(ExceptionSink* xsink);

    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void notifyPendingStreams(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void fireReadyCallback(ExceptionSink* xsink);

    //! Arms the idle deadline (if not already armed) and annotates \a poll_info
    //! with a poll_timeout_ms hint so the I/O controller wakes continuePoll()
    //! when the deadline expires.  I/O thread only.  No-op if idle_timeout_us
    //! is <= 0 or \a poll_info is nullptr.
    DLLLOCAL QoreHashNode* armIdleDeadline(QoreHashNode* poll_info, ExceptionSink* xsink);

    //! Release the current inner operation
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);

    //! Convert binary body to string based on Content-Type charset
    DLLLOCAL QoreValue convertBodyEncoding(ExceptionSink* xsink);

    //! Check if Content-Type indicates a text-based body
    DLLLOCAL static bool isTextContentType(const char* ct);
};

DLLLOCAL QoreClass* initHttp1ClientPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTP1CLIENTPOLLOPERATIONBASE_H
