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

#include <atomic>
#include <string>
#include <unordered_map>

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
    on application threads. Shared state is protected by stream_lock. Lock
    ordering: stream_lock -> sock->priv->m (socket internal lock, acquired by
    submitHttp2Request).

    @since %Qore 2.3
*/
class Http2ClientPollOperationPriv : public SocketPollOperationBase {
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

    DLLLOCAL virtual ~Http2ClientPollOperationPriv();

    // --- SocketPollOperationBase overrides ---

    DLLLOCAL bool goalReached() const override {
        return false;  // long-running multiplexed operation
    }

    DLLLOCAL QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL void abort(ExceptionSink* xsink) override;
    DLLLOCAL QoreValue getOutput() const override;

    // --- Stream management (called from Qore app thread) ---

    //! Submits an HTTP/2 request with headers and optional body
    /** Builds pseudo-headers, checks capacity, submits via socket, registers
        the completion action, all under stream_lock for atomicity.

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
        if (error_info) {
            error_info->ref();
        }
        return error_info;
    }

    DLLLOCAL int getActiveStreamCount() const {
        return active_stream_count;
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

    //! Cleanup all referenced objects (must be called before destructor)
    DLLLOCAL void cleanup(ExceptionSink* xsink);

protected:
    DLLLOCAL const char* getStateImpl() const override;

private:
    //! Current inner poll operation (ref'd) — changes during state transitions
    SocketPollOperationBase* current_op;

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

    //! Stream completion actions: stream_id string -> ref'd action
    /** Actions are pure C++ (no Qore interpreter) — execute/executeError
        are called directly on the I/O thread without execValue().
    */
    std::unordered_map<std::string, AbstractAsyncAction*> stream_actions;
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

    static constexpr int MAX_DRAIN_ITERATIONS = 100;
    static constexpr int MAX_EMPTY_READS = 100;
    static constexpr int H2C_PROBE_MAX_EMPTY_READS = 5;

    // --- Internal methods (I/O thread only) ---

    DLLLOCAL QoreHashNode* handleConnecting(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProxyConnectSend(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleProxyConnectRecv(ExceptionSink* xsink);
    DLLLOCAL QoreHashNode* handleReading(ExceptionSink* xsink);

    DLLLOCAL void startSslUpgrade(ExceptionSink* xsink);
    DLLLOCAL void startProxyConnect(ExceptionSink* xsink);
    DLLLOCAL void startMultiplex(ExceptionSink* xsink);

    DLLLOCAL void setError(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void notifyPendingStreams(const char* err, const char* desc, ExceptionSink* xsink);
    DLLLOCAL void fireReadyCallback(ExceptionSink* xsink);

    //! Release the current inner operation
    DLLLOCAL void releaseCurrentOp(ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initHttp2ClientPollOperationBaseClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_HTTP2CLIENTPOLLOPERATIONBASE_H
