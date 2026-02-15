/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QC_SocketPollOperation.h

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

#ifndef _QORE_CLASS_SOCKETPOLLOPERATION_H

#define _QORE_CLASS_SOCKETPOLLOPERATION_H

#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QC_Socket.h"
#include "qore/QoreSocketObject.h"
#include "qore/InputStream.h"

#include <memory>
#include <deque>

// goals: connect, connect-ssl
constexpr int SPG_CONNECT = 1;
constexpr int SPG_CONNECT_SSL = 2;

// states: none -> connecting -> [connecting-ssl ->] connected
constexpr int SPS_NONE = 0;
constexpr int SPS_CONNECTING = 1;
constexpr int SPS_CONNECTING_SSL = 2;
constexpr int SPS_CONNECTED = 3;

class SocketPollSocketOperationBase : public SocketPollOperationBase {
public:
    DLLLOCAL SocketPollSocketOperationBase(QoreObject* self) : SocketPollOperationBase(self) {
    }

    DLLLOCAL SocketPollSocketOperationBase(QoreSocketObject* sock) : sock(sock) {
    }

    DLLLOCAL ~SocketPollSocketOperationBase() {
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) {
        // Clear poll_state to release any buffers held by the poll state
        // This prevents memory accumulation if the operation object remains referenced after timeout
        poll_state.reset();
        if (set_non_block) {
            set_non_block = false;
            AutoLocker al(sock->priv->m);
            sock->priv->clearNonBlock();
            if (abortNeedsClose()) {
                // avoid re-locking priv->m via QoreSocketObject::close()
                // QoreSocketObject cleanup is handled by the owning code path.
                sock->priv->socket->close();
            }
            state = SPS_NONE;
        }
    }

protected:
    std::unique_ptr<AbstractPollState> poll_state;
    QoreSocketObject* sock = nullptr;
    int state = SPS_NONE;
    bool set_non_block = false;

    DLLLOCAL virtual bool abortNeedsClose() const {
        return true;
    }
};

class SocketConnectPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketConnectPollOperation(ExceptionSink* xsink, bool ssl, const char* target, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_CONNECTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

protected:
    //! Called in the constructor
    DLLLOCAL virtual int preVerify(ExceptionSink* xsink) {
        return 0;
    }

    //! Called when the connection is established
    DLLLOCAL virtual void connected();

    //! Called to switch to the connect-ssl state
    DLLLOCAL int startSslConnect(ExceptionSink* xsink);

private:
    std::string target;

    int sgoal = 0;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_CONNECTING:
                return "connecting";
            case SPS_CONNECTING_SSL:
                return "connecting-ssl";
            case SPS_CONNECTED:
                return "connected";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink);
};

class SocketSendPollOperation : public SocketPollSocketOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketSendPollOperation(ExceptionSink* xsink, QoreStringNode* data, QoreSocketObject* sock);

    // "data" must be passed already referenced
    DLLLOCAL SocketSendPollOperation(ExceptionSink* xsink, BinaryNode* data, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return sent;
    }

    DLLLOCAL virtual const char* getStateImpl() const {
        return sent ? "sent" : "sending";
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

private:
    SimpleRefHolder<SimpleValueQoreNode> data;
    const char* buf;
    size_t size;
    bool sent = false;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketRecvPollOperationBase : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketRecvPollOperationBase(QoreSocketObject* sock, bool to_string)
            : SocketPollSocketOperationBase(sock), to_string(to_string) {
    }

    DLLLOCAL virtual void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Clear data buffer to prevent memory accumulation on timeout
        data.discard();
        SocketPollSocketOperationBase::abort(xsink);
    }

    DLLLOCAL virtual bool goalReached() const {
        return received;
    }

    DLLLOCAL virtual const char* getStateImpl() const {
        return received ? "received" : "receiving";
    }

    DLLLOCAL virtual QoreValue getOutput() const {
        return data ? data->refSelf() : QoreValue();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

protected:
    SimpleRefHolder<SimpleValueQoreNode> data;
    bool to_string;
    bool received = false;

    DLLLOCAL int initIntern(ExceptionSink* xsink);

    DLLLOCAL virtual bool abortNeedsClose() const = 0;
};

class SocketRecvPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketRecvPollOperation(ExceptionSink* xsink, ssize_t size, QoreSocketObject* sock, bool to_string);

private:
    size_t size;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketRecvDataPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketRecvDataPollOperation(ExceptionSink* xsink, QoreSocketObject* sock, bool to_string);

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketRecvUntilBytesPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketRecvUntilBytesPollOperation(ExceptionSink* xsink, const QoreStringNode* pattern,
            QoreSocketObject* sock, bool to_string);

private:
    SimpleRefHolder<QoreStringNode> pattern;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketReadHttpHeaderPollOperation : public SocketRecvPollOperationBase {
public:
    // "data" must be passed already referenced
    DLLLOCAL SocketReadHttpHeaderPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Clear output buffer to prevent memory accumulation on timeout
        out = nullptr;
        SocketRecvPollOperationBase::abort(xsink);
    }

private:
    mutable ReferenceHolder<QoreHashNode> out;

    DLLLOCAL virtual bool abortNeedsClose() const;
};

class SocketUpgradeClientSslPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketUpgradeClientSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return done;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        return "connecting-ssl";
    }

private:
    bool done = false;
};

class SocketUpgradeServerSslPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketUpgradeServerSslPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return done;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        return "accepting-ssl";
    }

private:
    bool done = false;
};

// goals: accept, accept-ssl
constexpr int SPG_ACCEPT = 1;
constexpr int SPG_ACCEPT_SSL = 2;

// states: none -> accepting -> [accepting-ssl ->] accepted
constexpr int SPS_ACCEPTING = 1;
constexpr int SPS_ACCEPTING_SSL = 2;
constexpr int SPS_ACCEPTED = 3;

class SocketAcceptPollSocketOperationBase : public SocketPollOperationBase {
public:
    DLLLOCAL SocketAcceptPollSocketOperationBase(QoreObject* self) : SocketPollOperationBase(self) {
    }

    DLLLOCAL SocketAcceptPollSocketOperationBase(QoreSocketObject* sock) : sock(sock) {
    }

    DLLLOCAL ~SocketAcceptPollSocketOperationBase() {
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) {
        // NOTE: we do not close the socket here in any case
        if (set_non_block_accept) {
            set_non_block_accept = false;
            AutoLocker al(sock->priv->m);
            sock->priv->clearNonBlockAccept();
            state = SPS_NONE;
        }
    }

protected:
    QoreSocketObject* sock = nullptr;
    int state = SPS_NONE;
    bool set_non_block_accept = false;

    DLLLOCAL virtual bool abortNeedsClose() const {
        return true;
    }
};

class SocketAcceptPollOperation : public SocketAcceptPollSocketOperationBase {
public:
    DLLLOCAL SocketAcceptPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block_accept) {
                AutoLocker al(sock->priv->m);
                sock->priv->clearNonBlockAccept();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Clear poll_state and accepted_socket to prevent memory accumulation on timeout
        poll_state.reset();
        accepted_socket_obj = nullptr;
        accepted_socket.discard();
        SocketAcceptPollSocketOperationBase::abort(xsink);
    }

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_ACCEPTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

protected:
    mutable SimpleRefHolder<QoreSocketObject> accepted_socket;
    //! Cached QoreObject wrapper for the accepted socket during SSL handshake
    mutable ReferenceHolder<QoreObject> accepted_socket_obj;

    //! Called in the constructor
    DLLLOCAL virtual int preVerify(ExceptionSink* xsink) {
        return 0;
    }

    //! Called when the connection is established
    DLLLOCAL virtual void accepted();

    //! Called to switch to the connect-ssl state
    DLLLOCAL int startSslAccept(ExceptionSink* xsink);

    //! Returns the correct socket for poll info based on current state
    /** During SSL handshake, returns the accepted client socket instead of the listener socket.
        The accepted_socket_obj cache is used to avoid creating a new QoreObject wrapper on each call.
        Cache lifecycle: cleared in abort() and ownership transferred in getOutput().
    */
    DLLLOCAL QoreHashNode* getSocketPollInfoHash(ExceptionSink* xsink, int events) const {
        ReferenceHolder<QoreHashNode> info(new QoreHashNode(hashdeclSocketPollInfo, xsink), xsink);
        info->setKeyValue("events", events, xsink);
        if (state == SPS_ACCEPTING_SSL && accepted_socket) {
            // During SSL handshake, poll the accepted client socket, not the listener
            // Cache the QoreObject wrapper to avoid creating a new one each time
            if (!accepted_socket_obj) {
                accepted_socket->ref();
                accepted_socket_obj = new QoreObject(QC_SOCKET, getProgram(), *accepted_socket);
            }
            accepted_socket_obj->ref();
            info->setKeyValue("socket", *accepted_socket_obj, xsink);
        } else {
            info->setKeyValue("socket", getReferencedSocketObject(xsink), xsink);
        }
        return info.release();
    }

private:
    std::unique_ptr<AbstractPollState> poll_state;
    std::string target;

    int sgoal = 0;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_ACCEPTING:
                return "accepting";
            case SPS_ACCEPTING_SSL:
                return "accepting-ssl";
            case SPS_ACCEPTED:
                return "accepted";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink);
};

class Http2Session;

// HTTP/2 poll operation states
constexpr int H2S_NONE = 0;
constexpr int H2S_SEND_PREFACE = 1;
constexpr int H2S_RECV_PREFACE = 2;
constexpr int H2S_READING = 3;
constexpr int H2S_REQUEST_READY = 4;
constexpr int H2S_SENDING = 5;
constexpr int H2S_FLUSHING = 6;  // Poll for POLLOUT to ensure data is flushed
constexpr int H2S_SENT = 7;

//! Poll operation for reading HTTP/2 requests on a server connection
/** This poll operation handles HTTP/2 server-side request reading:
    1. Exchange connection preface (SETTINGS frames)
    2. Read frames until a complete request stream is available

    The output is a hash with request information:
    - method: HTTP method
    - path: request path
    - headers: request headers hash
    - body: request body (binary)
    - stream_id: HTTP/2 stream ID for sending response
    - session: the Http2Session for sending responses
*/
class SocketHttp2ServerPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketHttp2ServerPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL ~SocketHttp2ServerPollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return h2_state == H2S_REQUEST_READY && !peer_closed;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (h2_state) {
            case H2S_NONE: return "none";
            case H2S_SEND_PREFACE: return "sending-preface";
            case H2S_RECV_PREFACE: return "receiving-preface";
            case H2S_READING: return "reading-frames";
            case H2S_REQUEST_READY: return "request-ready";
            default: return "unknown";
        }
    }

    //! Returns nullptr - session is managed by socket
    DLLLOCAL Http2SessionPtr takeSession();

    //! Returns the stream ID of the completed request
    DLLLOCAL int32_t getStreamId() const { return stream_id; }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Clear cached stream to prevent memory accumulation on timeout
        cached_stream.reset();
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    Http2SessionPtr h2_session;
    int h2_state = H2S_NONE;
    int32_t stream_id = 0;
    bool peer_closed = false;
    //! Cached completed stream info, dequeued in continuePoll(), used by getOutput()
    std::unique_ptr<Http2StreamInfo> cached_stream;

    DLLLOCAL virtual bool abortNeedsClose() const { return true; }

    //! Initialize HTTP/2 session
    DLLLOCAL int initSession(ExceptionSink* xsink);
};

//! Poll operation for sending HTTP/2 responses
/** This poll operation sends an HTTP/2 response:
    - Submits response headers and body
    - Sends all pending frames

    The HTTP/2 session is retrieved from the socket (stored by a previous
    read operation).
*/
class SocketHttp2SendResponsePollOperation : public SocketPollSocketOperationBase {
public:
    //! Creates the poll operation
    /** @param xsink exception sink
        @param sock the socket (must have an active HTTP/2 session from a previous read)
        @param h2_session unused, session is retrieved from socket
        @param stream_id the stream ID to respond on
        @param status_code HTTP status code
        @param headers response headers
        @param body response body (can be nullptr)
        @param is_connect if true, use submitConnectResponse for RFC 8441 WebSocket
    */
    DLLLOCAL SocketHttp2SendResponsePollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        const Http2SessionPtr& h2_session, int32_t stream_id, int status_code,
        const QoreHashNode* headers, const BinaryNode* body, bool is_connect = false);

    DLLLOCAL ~SocketHttp2SendResponsePollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return h2_state == H2S_SENT;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (h2_state) {
            case H2S_NONE: return "none";
            case H2S_SENDING: return "sending";
            case H2S_FLUSHING: return "flushing";
            case H2S_SENT: return "sent";
            default: return "unknown";
        }
    }

    //! Returns nullptr - session is managed by socket
    DLLLOCAL Http2SessionPtr takeSession();

private:
    Http2SessionPtr h2_session;
    int h2_state = H2S_NONE;
    int32_t stream_id = 0;

    DLLLOCAL virtual bool abortNeedsClose() const { return true; }
};

//! Poll operation for sending HTTP/2 streaming responses from an InputStream
/** This poll operation reads from an InputStream in chunks and sends them as HTTP/2
    DATA frames without buffering the entire body in memory.

    State machine:
    - SS_SUBMIT_HEADERS: Submit HTTP/2 response headers with deferred data provider
    - SS_READ_CHUNK: Read a chunk from the InputStream
    - SS_SEND_CHUNK: Send the chunk as HTTP/2 DATA frames
    - SS_FLUSH: Flush pending data to socket
    - SS_DONE: Goal reached

    @since Qore 2.2
*/
class SocketHttp2SendStreamingResponsePollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketHttp2SendStreamingResponsePollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
        int32_t stream_id, int status_code, const QoreHashNode* headers,
        InputStream* input_stream, int64 chunk_size = 16384);

    DLLLOCAL ~SocketHttp2SendStreamingResponsePollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return ss_state == SS_DONE;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (ss_state) {
            case SS_READ_CHUNK: return "reading-chunk";
            case SS_SEND_CHUNK: return "sending-chunk";
            case SS_FLUSH: return "flushing";
            case SS_RECV_WINDOW: return "receiving-window-update";
            case SS_DONE: return "done";
            default: return "unknown";
        }
    }

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        input_stream = nullptr;
        current_chunk = nullptr;
        if (set_non_block) {
            sock->clearNonBlock();
            set_non_block = false;
        }
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    enum StreamingState { SS_READ_CHUNK, SS_SEND_CHUNK, SS_FLUSH, SS_RECV_WINDOW, SS_DONE };
    StreamingState ss_state = SS_READ_CHUNK;
    int32_t stream_id = 0;
    SimpleRefHolder<InputStream> input_stream;
    int64 chunk_size;
    bool eof = false;
    bool is_pollable;
    bool need_reassign = true;
    int stream_fd = -1;
    SimpleRefHolder<BinaryNode> current_chunk;

    DLLLOCAL virtual bool abortNeedsClose() const { return true; }
};

//! Fused send HTTP response + idle wait + read next HTTP header poll operation
/** This operation combines three phases into a single poll operation:
    1. SENDING: Non-blocking write of the HTTP response
    2. IDLE: Wait for the next request (POLLIN) with a configurable timeout
    3. READING_HEADER: Read and parse the next HTTP header

    This eliminates the overhead of separate send, addIdleConnection, and header
    read operations in the keep-alive path.

    @since %Qore 2.3
*/
class SocketSendAndReadHeaderPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketSendAndReadHeaderPollOperation(ExceptionSink* xsink, BinaryNode* response_data,
        QoreSocketObject* sock, int64 idle_timeout_ms);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const {
        return phase == Phase::Complete || phase == Phase::Timeout;
    }

    DLLLOCAL virtual const char* getStateImpl() const;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);
    DLLLOCAL virtual QoreValue getOutput() const;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Clear buffers to prevent memory accumulation on abort
        send_data.discard();
        header_output = nullptr;
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    enum class Phase { Sending, Idle, ReadingHeader, Complete, Timeout, Error };
    Phase phase = Phase::Sending;

    // Sending phase: holds the pre-serialized HTTP response
    SimpleRefHolder<SimpleValueQoreNode> send_data;
    const char* buf;
    size_t size;

    // Idle phase: timeout deadline in milliseconds from epoch
    int64 idle_timeout_ms;

    // Header reading phase: parsed HTTP headers output
    mutable ReferenceHolder<QoreHashNode> header_output;

    DLLLOCAL virtual bool abortNeedsClose() const {
        return true;
    }
};

// HTTP/2 client multiplex poll operation states
constexpr int H2C_NONE = 0;
constexpr int H2C_SEND_PREFACE = 1;
constexpr int H2C_RECV_PREFACE = 2;
constexpr int H2C_READING = 3;
constexpr int H2C_RESPONSE_READY = 4;
constexpr int H2C_CLOSED = 5;

//! Poll operation for HTTP/2 client multiplexed connection handling
/** This poll operation manages an HTTP/2 client connection with multiple concurrent streams:
    1. Exchange connection preface (SETTINGS frames)
    2. Continuously read HTTP/2 frames
    3. Dispatch completed responses to registered stream callbacks
    4. Support submitting new requests while reading responses

    Unlike the server-side operation, this operation:
    - Queues completed responses for retrieval via getOutput()
    - Supports continuous reading mode for true multiplexing
    - goalReached() returns true when a response is ready or connection closed

    @since Qore 2.3
*/
class SocketHttp2ClientMultiplexPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketHttp2ClientMultiplexPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL ~SocketHttp2ClientMultiplexPollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    //! Returns true when a response is ready or connection is closed
    DLLLOCAL virtual bool goalReached() const {
        AutoLocker al(response_lock);
        return !completed_responses.empty() || h2_state == H2C_CLOSED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    //! Returns the next completed response, or NOTHING if none available
    DLLLOCAL virtual QoreValue getOutput() const {
        AutoLocker al(response_lock);
        if (completed_responses.empty()) {
            return QoreValue();
        }
        // Return and remove the first response
        QoreHashNode* response = completed_responses.front();
        completed_responses.pop_front();
        return response;
    }

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (h2_state) {
            case H2C_NONE: return "none";
            case H2C_SEND_PREFACE: return "sending-preface";
            case H2C_RECV_PREFACE: return "receiving-preface";
            case H2C_READING: return "reading-frames";
            case H2C_RESPONSE_READY: return "response-ready";
            case H2C_CLOSED: return "closed";
            default: return "unknown";
        }
    }

    //! Returns true if the connection is still open for multiplexing
    DLLLOCAL bool isOpen() const { return h2_state != H2C_CLOSED; }

    //! Returns the HTTP/2 session for submitting requests
    DLLLOCAL Http2SessionPtr getSession() const { return h2_session; }

    //! Submit an HTTP/2 request on this multiplexed connection
    /** @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param body_len request body length
        @param xsink exception sink
        @return stream ID on success, -1 on error
    */
    DLLLOCAL int32_t submitRequest(const char* method, const char* path,
        const std::map<std::string, std::string>& headers,
        const void* body, size_t body_len, ExceptionSink* xsink);

    //! Cancel a pending stream
    DLLLOCAL void cancelStream(int32_t stream_id, ExceptionSink* xsink);

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        h2_state = H2C_CLOSED;
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    Http2SessionPtr h2_session;
    int h2_state = H2C_NONE;
    bool peer_closed = false;

    //! Queue of completed responses (thread-safe)
    mutable std::deque<QoreHashNode*> completed_responses;
    mutable QoreThreadLock response_lock;

    //! Shared state for safe callback handling during destruction
    /** This struct is captured by the stream completion callback lambda (via shared_ptr).
        The mutex ensures that:
        1. The guard flag is only checked/modified while holding the mutex
        2. The destructor waits for any in-flight callback to complete
        3. No TOCTOU race between checking the flag and using 'this'
    */
    struct CallbackGuard {
        std::mutex mutex;
        bool destroyed = false;
    };
    std::shared_ptr<CallbackGuard> callback_guard = std::make_shared<CallbackGuard>();

    DLLLOCAL virtual bool abortNeedsClose() const { return true; }

    //! Initialize HTTP/2 client session
    DLLLOCAL int initSession(ExceptionSink* xsink);

    //! Callback invoked when a stream completes
    DLLLOCAL void onStreamComplete(int32_t stream_id, Http2StreamInfo* stream, ExceptionSink* xsink);
};

DLLLOCAL QoreClass* initSocketPollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_SOCKETPOLLOPERATION_H
