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

// Public API — exported poll operation classes
#include "qore/SocketPollOperation.h"

// Internal-only includes
#include "qore/intern/qore_socket_private.h"
#include "qore/intern/QC_Socket.h"
#include "qore/InputStream.h"
#include "qore/intern/QuicSession.h"

class QoreEventNotifier;

#include <deque>

//! Max single QUIC UDP datagram receive buffer (1500 typical MTU + headroom for jumbo frames)
//! @note Assumes GSO/GRO is not used; if Generic Segmentation Offload is enabled in the future,
//! this must be increased to handle coalesced datagrams (e.g., 64KB for GRO).
constexpr size_t QUIC_RECV_BUF_SIZE = 2048;

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

    DLLLOCAL virtual bool goalReached() const override {
        return state == SPS_ACCEPTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual QoreValue getOutput() const override;

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

    DLLLOCAL virtual const char* getStateImpl() const override {
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
constexpr int H2S_HEADERS_READY = 8;  // Headers received (headers-only mode)

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

    DLLLOCAL virtual bool goalReached() const override {
        return ((h2_state == H2S_REQUEST_READY || h2_state == H2S_HEADERS_READY) && !peer_closed);
    }

    //! Set headers-only mode: poll completes when HEADERS arrive (before END_STREAM)
    DLLLOCAL void setHeadersOnly(bool v) {
        headers_only = v;
        if (h2_session) {
            h2_session->setHeadersOnlyMode(v);
        }
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual QoreValue getOutput() const override;

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (h2_state) {
            case H2S_NONE: return "none";
            case H2S_SEND_PREFACE: return "sending-preface";
            case H2S_RECV_PREFACE: return "receiving-preface";
            case H2S_READING: return "reading-frames";
            case H2S_REQUEST_READY: return "request-ready";
            case H2S_HEADERS_READY: return "headers-ready";
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
    bool headers_only = false;  //!< If true, goal is reached on HEADERS (before END_STREAM)
    //! Cached completed stream info, dequeued in continuePoll(), used by getOutput()
    std::unique_ptr<Http2StreamInfo> cached_stream;

    DLLLOCAL virtual bool abortNeedsClose() const override { return true; }

    //! Initialize HTTP/2 session
    DLLLOCAL int initSession(ExceptionSink* xsink);

    //! Check for headers-only dispatch in continuePoll()
    /** @param handled set to true if the check produced a result (caller should return rv)
        @param xsink exception sink
        @return poll info hash if polling needed, nullptr if headers dispatched or not applicable
    */
    DLLLOCAL QoreHashNode* checkHeadersOnlyDispatch(bool& handled, ExceptionSink* xsink);
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

    DLLLOCAL virtual bool goalReached() const override {
        return h2_state == H2S_SENT;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual const char* getStateImpl() const override {
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

    DLLLOCAL virtual bool abortNeedsClose() const override { return true; }
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
        InputStream* input_stream, QoreObject* input_stream_obj, int64 chunk_size = 16384);

    DLLLOCAL ~SocketHttp2SendStreamingResponsePollOperation();

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            if (input_stream && !need_reassign) {
                input_stream->unassignThread(xsink);
            }
            if (input_stream_obj) {
                input_stream_obj->deref(xsink);
                input_stream_obj = nullptr;
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return ss_state == SS_DONE;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual const char* getStateImpl() const override {
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
        if (input_stream && !need_reassign) {
            input_stream->unassignThread(xsink);
        }
        input_stream = nullptr;
        if (input_stream_obj) {
            input_stream_obj->deref(xsink);
            input_stream_obj = nullptr;
        }
        current_chunk = nullptr;
        // Send RST_STREAM to notify client that the stream is being cancelled
        {
            AutoLocker al(sock->priv->m);
            Http2Session* session = sock->priv->socket->priv->h2_session.get();
            if (session) {
                ExceptionSink rst_xsink;
                session->submitRstStream(stream_id, NGHTTP2_CANCEL, &rst_xsink);
                if (!rst_xsink) {
                    session->sendPendingDataBlocking(100, &rst_xsink);
                }
            }
        }
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
    QoreObject* input_stream_obj = nullptr;  //!< Strong ref to InputStream QoreObject (prevents GC)
    int64 chunk_size;
    bool eof = false;
    bool is_pollable;
    bool need_reassign = true;
    int stream_fd = -1;
    SimpleRefHolder<BinaryNode> current_chunk;

    DLLLOCAL virtual bool abortNeedsClose() const override { return true; }
};

//! Poll operation for flushing pending HTTP/2 data
/** This poll operation flushes all pending HTTP/2 data that has been queued
    via submitHttp2StreamingResponseHeaders(), sendHttp2StreamData(), and/or
    sendHttp2Trailers(). It calls sendPendingData() until no more data remains.

    State machine:
    - H2F_FLUSHING: Calling sendPendingData() to write pending data to socket
    - H2F_DONE: All pending data has been written

    @since %Qore 2.3
*/
class SocketHttp2FlushPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketHttp2FlushPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return h2f_state == H2F_DONE;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual const char* getStateImpl() const override {
        switch (h2f_state) {
            case H2F_FLUSHING: return "flushing";
            case H2F_DONE: return "done";
            default: return "unknown";
        }
    }

private:
    enum FlushState { H2F_FLUSHING, H2F_DONE };
    FlushState h2f_state = H2F_FLUSHING;

    DLLLOCAL virtual bool abortNeedsClose() const override { return true; }
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

    DLLLOCAL virtual bool goalReached() const override {
        return phase == Phase::Complete || phase == Phase::Timeout || phase == Phase::Closed;
    }

    DLLLOCAL bool needsCloseOnComplete() const override {
        return phase == Phase::Closed;
    }

    DLLLOCAL virtual const char* getStateImpl() const override;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL virtual QoreValue getOutput() const override;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Clear buffers to prevent memory accumulation on abort
        send_data.discard();
        header_output = nullptr;
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    enum class Phase { Sending, Idle, ReadingHeader, Complete, Timeout, Closed, Error };
    Phase phase = Phase::Sending;

    // Sending phase: holds the pre-serialized HTTP response
    SimpleRefHolder<SimpleValueQoreNode> send_data;
    const char* buf;
    size_t size;

    // Idle phase: timeout deadline in milliseconds from epoch
    int64 idle_timeout_ms;

    // Header reading phase: parsed HTTP headers output
    mutable ReferenceHolder<QoreHashNode> header_output;

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return true;
    }
};

//! Poll operation to send HTTP response headers + stream InputStream body + optionally idle+read next header
/** State machine:
    SendHeaders -> StreamBody -> [Idle -> ReadingHeader -> Complete | Timeout]  (fused=true)
                              -> [Complete]                                     (fused=false)

    @since %Qore 2.3
*/
class SocketSendStreamAndReadHeaderPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketSendStreamAndReadHeaderPollOperation(ExceptionSink* xsink, BinaryNode* header_data,
        QoreSocketObject* sock, InputStream* input_stream, QoreObject* input_stream_obj,
        int64 content_length, int64 idle_timeout_ms, bool fused, bool chunked = false);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            if (input_stream && !need_reassign) {
                input_stream->unassignThread(xsink);
            }
            if (input_stream_obj) {
                input_stream_obj->deref(xsink);
                input_stream_obj = nullptr;
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return phase == Phase::Complete || phase == Phase::Timeout || phase == Phase::Closed;
    }

    DLLLOCAL bool needsCloseOnComplete() const override {
        return phase == Phase::Closed;
    }

    DLLLOCAL virtual const char* getStateImpl() const override;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;
    DLLLOCAL virtual QoreValue getOutput() const override;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        header_data.discard();
        if (input_stream && !need_reassign) {
            input_stream->unassignThread(xsink);
        }
        input_stream = nullptr;
        if (input_stream_obj) {
            input_stream_obj->deref(xsink);
            input_stream_obj = nullptr;
        }
        current_chunk = nullptr;
        header_output = nullptr;
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    enum class Phase { SendHeaders, StreamBody, Idle, ReadingHeader, Complete, Timeout, Closed, Error };
    Phase phase = Phase::SendHeaders;

    // SendHeaders phase: pre-serialized HTTP response headers
    SimpleRefHolder<SimpleValueQoreNode> header_data;
    const char* hdr_buf;
    size_t hdr_size;

    // StreamBody phase: InputStream body source
    SimpleRefHolder<InputStream> input_stream;
    QoreObject* input_stream_obj = nullptr;  //!< Strong ref to InputStream QoreObject (prevents GC)
    int64 content_length;       //!< -1 for chunked encoding (stream until EOF)
    int64 bytes_sent = 0;
    int stream_fd = -1;
    bool is_pollable = true;
    bool need_reassign = true;
    bool chunked_encoding = false; //!< True when using HTTP chunked Transfer-Encoding
    bool sent_terminal_chunk = false; //!< True after sending "0\r\n\r\n"
    SimpleRefHolder<BinaryNode> current_chunk;

    // Idle + ReadHeader phases
    bool fused;
    int64 idle_timeout_ms;
    mutable ReferenceHolder<QoreHashNode> header_output;

    DLLLOCAL virtual bool abortNeedsClose() const override {
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
    DLLLOCAL virtual bool goalReached() const override {
        AutoLocker al(response_lock);
        return !completed_responses.empty() || h2_state == H2C_CLOSED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Returns the next completed response, or NOTHING if none available
    DLLLOCAL virtual QoreValue getOutput() const override {
        AutoLocker al(response_lock);
        if (completed_responses.empty()) {
            return QoreValue();
        }
        // Return and remove the first response
        QoreHashNode* response = completed_responses.front();
        completed_responses.pop_front();
        return response;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
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
        const strcase_str_map_t& headers,
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

    DLLLOCAL virtual bool abortNeedsClose() const override { return true; }

    //! Initialize HTTP/2 client session
    DLLLOCAL int initSession(ExceptionSink* xsink);

    //! Callback invoked when a stream completes
    DLLLOCAL void onStreamComplete(int32_t stream_id, Http2StreamInfo* stream, ExceptionSink* xsink);
};

//! Poll operation for non-blocking UDP recvfrom()
/** Receives a single UDP datagram and captures the source address.
    Output is a hash with: data (binary), address (string), port (int),
    family (int), familystr (string).

    @since %Qore 2.3
*/
class SocketRecvFromPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketRecvFromPollOperation(ExceptionSink* xsink, size_t max_size, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return received;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return received ? "received" : "receiving";
    }

    DLLLOCAL virtual QoreValue getOutput() const override {
        return output ? output->hashRefSelf() : QoreValue();
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        output = nullptr;
        SocketPollSocketOperationBase::abort(xsink);
    }

private:
    size_t max_size;
    bool received = false;
    mutable ReferenceHolder<QoreHashNode> output;

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return false;  // UDP is connectionless, no need to close on abort
    }
};

//! Poll operation for non-blocking UDP sendto()
/** Sends a datagram to the specified destination address.

    @since %Qore 2.3
*/
class SocketSendToPollOperation : public SocketPollSocketOperationBase {
public:
    //! "data" must be passed already referenced; ownership transfers to the poll state
    DLLLOCAL SocketSendToPollOperation(ExceptionSink* xsink, const char* host, int port, int family,
        BinaryNode* data, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                sock->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return sent;
    }

    DLLLOCAL virtual const char* getStateImpl() const override {
        return sent ? "sent" : "sending";
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

private:
    struct sockaddr_storage dest_addr{};
    socklen_t dest_addr_len = 0;
    bool sent = false;

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return false;  // UDP is connectionless, no need to close on abort
    }
};

//! QUIC poll operation state machine states
enum class QCS : int {
    NONE = 0,            //!< initial state
    HANDSHAKE_SEND = 1,  //!< sending handshake packets
    HANDSHAKE_RECV = 2,  //!< receiving handshake packets
    SETUP_HTTP3 = 3,     //!< setting up HTTP/3 layer
    READING = 4,         //!< reading request/response packets
    REQUEST_READY = 5,   //!< HTTP/3 request ready (server)
    RESPONSE_READY = 6,  //!< HTTP/3 response ready (client)
    SENDING = 7,         //!< sending response/request data
    FLUSHING = 8,        //!< flushing pending QUIC packets
    SENT = 9,            //!< response sent
    CLOSED = 10,         //!< connection closed
    HEADERS_READY = 11,  //!< HTTP/3 headers received (headers-only mode)
};

//! Poll operation for QUIC client: handshake + HTTP/3 request/response
/** Creates a QUIC connection, performs the TLS 1.3 handshake over QUIC,
    sets up HTTP/3, submits a request, and reads the response.

    @since %Qore 2.3
*/
class SocketQuicClientPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketQuicClientPollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
                                           const char* host, uint16_t port, int family);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                AutoLocker al(sock->priv->m);
                sock->priv->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return qcs_state == QCS::RESPONSE_READY || qcs_state == QCS::CLOSED;
    }

    DLLLOCAL virtual const char* getStateImpl() const override;
    DLLLOCAL virtual QoreValue getOutput() const override;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    //! Flush pending writes and return a SocketPollInfo hash with accurate events
    /** Called when yielding on RESPONSE_READY to ensure pending requests
        (queued by concurrent submitQuicRequest() calls) are sent and the
        caller gets correct POLLOUT events and QUIC retransmission timeout.

        @param xsink for exception handling
        @param do_flush if true, call sendPendingPackets() before computing events;
               if false, use the connection's current expiry for the poll timeout
        @return SocketPollInfo hash or nullptr on error
    */
    DLLLOCAL QoreHashNode* flushAndReturnPollInfo(ExceptionSink* xsink, bool do_flush = true);

    // Client QUIC: clean up session and close the dedicated UDP socket.
    // Unlike SocketQuicServerPollOperation (shared socket, many sessions),
    // client connections own their UDP socket — it must be closed to prevent
    // fd leaks.
    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        {
            // Hold socket lock for session cleanup (lock ordering: priv->m →
            // quic_sessions_lock). Released before closeIo() which also takes
            // priv->m.
            AutoLocker al(sock->priv->m);
            // Wake handler threads blocked in waitForStreamData()/waitForStreamDrain()
            // BEFORE removing the session from the socket map
            if (quic_session) {
                quic_session->markClosed();
                sock->priv->socket->priv->removeQuicSession(quic_session->getSessionId());
                quic_session.reset();
            }
            if (set_non_block) {
                sock->priv->clearNonBlock();
                set_non_block = false;
            }
        }
        // Close the dedicated client UDP socket to prevent fd leak.
        // Must be called outside the socket lock — closeIo() acquires it.
        sock->closeIo(xsink);
    }

    //! Submit an HTTP/3 request on this connection
    DLLLOCAL int64_t submitRequest(const char* method, const char* path,
                                   const strcase_str_map_t& headers,
                                   const void* body, size_t body_len,
                                   ExceptionSink* xsink);

    //! Submit a streaming HTTP/3 request (headers only, no END_STREAM)
    DLLLOCAL int64_t submitRequestStreaming(const char* method, const char* path,
                                            const strcase_str_map_t& headers,
                                            ExceptionSink* xsink);

    //! Get the QuicSession
    DLLLOCAL std::shared_ptr<QuicSession> getSession() const { return quic_session; }

    //! Check if connection is still open
    DLLLOCAL bool isOpen() const { return qcs_state != QCS::CLOSED && quic_session && !quic_session->isClosed(); }

    //! Synchronously flush pending QUIC writes to the UDP socket
    /** Produces any queued QUIC packets (HTTP/3 requests, ACKs, etc.) via
        ngtcp2 and sends them via sendto()/sendmmsg().  Safe to call from
        any thread — acquires the QuicSession mutex internally.

        Use this after submitRequest() to guarantee that request frames
        are on the wire before the connection is closed.

        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int flushPendingWrites(ExceptionSink* xsink);

    //! Migrate the QUIC connection to a new socket (client-side active migration)
    /** Creates a new connected UDP socket, calls QuicSession::initiateMigration(),
        and swaps the fd on the underlying socket object.

        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int migrateConnection(ExceptionSink* xsink);

private:
    std::shared_ptr<QuicSession> quic_session;
    QCS qcs_state = QCS::NONE;
    struct sockaddr_storage local_addr_{};
    socklen_t local_addrlen_ = 0;
    struct sockaddr_storage remote_addr_{};
    socklen_t remote_addrlen_ = 0;
    //! Cached completed stream info; mutable so getOutput() (const) can consume it.
    //! Thread safety: the poll framework serializes continuePoll() and getOutput()
    //! calls on a single operation — no concurrent mutation occurs.
    mutable std::unique_ptr<QuicStreamInfo> cached_stream;
    uint8_t recv_buf_[QUIC_RECV_BUF_SIZE]{};
    //! Reusable packet batch (avoids per-call heap allocations)
    QuicPacketBatch pkt_batch_;

    //! Coalesced timer + write + send pending QUIC packets via UDP
    /** @param next_expiry output: next timer expiry for poll timeout
        @return 0 on success, SOCK_POLLOUT if partial send, -1 on error
    */
    DLLLOCAL int sendPendingPackets(ngtcp2_tstamp& next_expiry, ExceptionSink* xsink);

    //! Receive and process a UDP datagram
    DLLLOCAL int recvAndProcessPacket(ExceptionSink* xsink);

    //! Try to set up HTTP/3 early when 0-RTT TX key is installed
    /** Checks isEarlyDataReady() and sets up HTTP/3 + flushes setup frames.
        @param xsink exception sink
        @return nullptr on success/no-op, poll info hash if SOCK_POLLOUT needed;
                raises exception and returns nullptr on error (check *xsink)
    */
    DLLLOCAL QoreHashNode* trySetupEarlyHttp3(ExceptionSink* xsink);

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return true;  // Client QUIC owns a dedicated UDP socket
    }

    DLLLOCAL bool needsCloseOnComplete() const override {
        // Only close when the QUIC connection is truly terminal (CLOSED).
        // RESPONSE_READY means a single request/response completed — the
        // operation will be re-submitted for subsequent requests on the
        // same connection.  Closing on RESPONSE_READY kills the socket
        // before the next request can be sent.
        return qcs_state == QCS::CLOSED;
    }
};

//! Poll operation for QUIC server: accept connection + read HTTP/3 request
/** Waits for QUIC packets, dispatches to the correct session by DCID,
    creates new sessions for Initial packets, and reads HTTP/3 requests.
    Supports multiple concurrent QUIC connections on a single UDP socket.

    @since %Qore 2.3
*/
class SocketQuicServerPollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketQuicServerPollOperation(ExceptionSink* xsink, QoreSocketObject* sock);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                AutoLocker al(sock->priv->m);
                sock->priv->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return qcs_state == QCS::REQUEST_READY || qcs_state == QCS::HEADERS_READY;
    }

    DLLLOCAL virtual const char* getStateImpl() const override;
    DLLLOCAL virtual QoreValue getOutput() const override;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    // NOTE: intentionally does NOT call SocketPollSocketOperationBase::abort() because:
    //  1. The base class closes the socket (abortNeedsClose()), but QUIC uses a shared
    //     UDP socket that must stay open for other sessions
    //  2. poll_state is never used by QUIC operations (QUIC manages its own state)
    //  3. QUIC uses qcs_state, not the base class 'state' member
    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Hold socket lock for the entire abort to prevent races with
        // concurrent socket destruction (lock ordering: priv->m → quic_sessions_lock)
        AutoLocker al(sock->priv->m);
        // Wake handler threads blocked in waitForStreamData()/waitForStreamDrain()
        // BEFORE removing sessions from the socket map
        for (auto& [id, session] : sessions_) {
            session->markClosed();
            sock->priv->socket->priv->removeQuicSession(id);
        }
        sessions_.clear();
        cached_stream_.reset();
        if (set_non_block) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }

    //! Maximum concurrent QUIC sessions per server socket.
    //! Prevents memory exhaustion from a flood of Initial packets;
    //! new connection attempts beyond this limit are silently dropped
    //! (clients will retry or time out).
    static constexpr size_t MAX_QUIC_SERVER_SESSIONS = 10000;

    //! Set headers-only dispatch mode
    /** When true, requests are dispatched as soon as headers are received,
        before the full body arrives.  The handler reads body data incrementally
        via readQuicStreamDataBlock().
    */
    DLLLOCAL void setHeadersOnly(bool v);

    //! Sets the maximum request body size for all sessions (0 = unlimited)
    /** Consistent with Http2Session::setMaxRequestBodySize().
    */
    DLLLOCAL void setMaxRequestBodySize(int64_t size);

private:
    //! Cached completed stream with session ID and session pointer (for peer address extraction)
    struct CachedStream {
        int64_t session_id;
        std::unique_ptr<QuicStreamInfo> stream;
        std::shared_ptr<QuicSession> session;
    };

    //! Local session map — single-threaded working copy used only from the I/O
    //! thread during continuePoll().  This is NOT the authoritative map; the
    //! authoritative map is qore_socket_private::quic_sessions (protected by
    //! quic_sessions_lock).  This copy avoids lock contention in the hot path.
    std::unordered_map<int64_t, std::shared_ptr<QuicSession>> sessions_;

    QCS qcs_state = QCS::NONE;

    //! Headers-only mode flag
    bool headers_only_ = false;

    //! Maximum request body size (0 = unlimited); propagated to sessions
    int64_t max_request_body_size_ = 0;

    //! Cached completed stream info; mutable so getOutput() (const) can consume it.
    //! Thread safety: the poll framework serializes continuePoll() and getOutput()
    //! calls on a single operation — no concurrent mutation occurs.
    mutable std::unique_ptr<CachedStream> cached_stream_;

    uint8_t recv_buf_[QUIC_RECV_BUF_SIZE]{};
    //! Reusable packet batch (avoids per-call heap allocations)
    QuicPacketBatch pkt_batch_;

    //! Cached local address (from getsockname at construction time)
    //! avoids per-datagram getsockname() syscall
    struct sockaddr_storage local_addr_{};
    socklen_t local_addrlen_ = 0;

    //! Check for headers-only dispatch during READING state
    /** @param handled set to true if a dispatch occurred
        @return nullptr if goal reached, poll info hash if I/O needed
    */
    DLLLOCAL QoreHashNode* checkHeadersOnlyDispatch(bool& handled, ExceptionSink* xsink);

    //! Coalesced timer + write + send for all sessions in a single pass
    /** Replaces separate processTimers() + sendAllPendingPackets() + getMinExpiry()
        with a single iteration over sessions.
        @param min_expiry output: minimum timer expiry across all sessions
        @return 0 on success, SOCK_POLLOUT if any session had a partial send, -1 on error
    */
    DLLLOCAL int processTimersAndSendAll(ngtcp2_tstamp& min_expiry, ExceptionSink* xsink);

    //! Receive and process a UDP datagram (dispatches to correct session)
    DLLLOCAL int recvAndProcessPacket(ExceptionSink* xsink, QuicSession** target_out = nullptr);

    //! Clean up closed sessions (called periodically, not every poll cycle)
    DLLLOCAL void cleanupClosedSessions();

    //! Poll cycle counter for periodic cleanup scheduling
    //! With up to 10K sessions, iterating all sessions every poll cycle is O(n);
    //! running cleanup every CLEANUP_INTERVAL cycles amortizes the cost.
    int cleanup_counter_ = 0;
    static constexpr int CLEANUP_INTERVAL = 100;

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return false;  // UDP is connectionless
    }
};

//! Poll operation to send an HTTP/3 response over QUIC
/** Queues an HTTP/3 response and flushes all pending QUIC packets.

    @since %Qore 2.3
*/
class SocketQuicSendResponsePollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketQuicSendResponsePollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
                                                  int64_t session_id, int64_t stream_id,
                                                  int status_code,
                                                  const QoreHashNode* headers,
                                                  const AbstractQoreNode* body);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (set_non_block) {
                AutoLocker al(sock->priv->m);
                sock->priv->clearNonBlock();
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return qcs_state == QCS::SENT;
    }

    DLLLOCAL virtual const char* getStateImpl() const override;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Hold socket lock for the entire abort to prevent races with
        // concurrent socket destruction (lock ordering: priv->m → quic_sessions_lock)
        AutoLocker al(sock->priv->m);
        // Release session reference to allow cleanup if the socket is closed
        quic_session.reset();
        if (set_non_block) {
            sock->priv->clearNonBlock();
            set_non_block = false;
        }
    }

private:
    std::shared_ptr<QuicSession> quic_session;
    QCS qcs_state = QCS::NONE;
    int64_t stream_id_ = -1;  //!< HTTP/3 stream ID (for ACK tracking in FLUSHING state)

    //! Migration generation snapshot from when SENDING state began.
    /** Compared against the session's current migration_gen_ to detect whether
        a connection migration occurred during this response.  When migration is
        detected, FLUSHING waits for the stream to be fully acknowledged (via
        stream_close callback) before transitioning to SENT, so that ngtcp2 can
        retransmit data lost on the old path.
    */
    uint64_t send_migration_gen_ = 0;

    //! Stored remote peer address for sendto()
    struct sockaddr_storage peer_addr_{};
    socklen_t peer_addrlen_ = 0;

    //! Per-session local address for source IP pinning via sendmsg() cmsg
    /** Distinct from local_addr_ (which is the wildcard from getsockname()
        used for receive-side pktinfo).  This is the actual per-session local
        IP captured at connection creation via pktinfo.
    */
    struct sockaddr_storage send_local_addr_{};
    socklen_t send_local_addrlen_ = 0;

    //! Receive buffer for incoming packets (ACKs)
    uint8_t recv_buf_[QUIC_RECV_BUF_SIZE]{};
    //! Reusable packet batch (avoids per-call heap allocations)
    QuicPacketBatch pkt_batch_;

    //! Cached local address (from getsockname at construction time)
    struct sockaddr_storage local_addr_{};
    socklen_t local_addrlen_ = 0;

    //! Coalesced timer + write + send pending QUIC packets via UDP
    /** @param next_expiry output: next timer expiry for poll timeout
        @return 0 on success, SOCK_POLLOUT if partial send, -1 on error
    */
    DLLLOCAL int sendPendingPackets(ngtcp2_tstamp& next_expiry, ExceptionSink* xsink);

    //! Receive and process incoming packets (ACKs for flow control)
    DLLLOCAL int recvAndProcessPackets(ExceptionSink* xsink);

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return false;  // UDP is connectionless
    }
};

//! Streaming states for QUIC streaming response poll operation
enum class QCS_SS : int {
    READ_CHUNK = 0,   //!< reading a chunk from InputStream
    SEND_CHUNK = 1,   //!< sending chunk data to QuicSession
    FLUSH = 2,        //!< flushing QUIC packets to the network
    RECV_ACK = 3,     //!< waiting for ACKs (backpressure)
    DONE = 4,         //!< streaming complete
};

//! Poll operation for sending an HTTP/3 streaming response via InputStream
/** Reads chunks from an InputStream and sends them incrementally over a QUIC
    stream using the deferred data reader mechanism (NGHTTP3_ERR_WOULDBLOCK /
    nghttp3_conn_resume_stream).

    State machine:
    QCS_SS_READ_CHUNK -> QCS_SS_SEND_CHUNK -> QCS_SS_FLUSH -> (loop or QCS_SS_DONE)
                                                  |
                                            QCS_SS_RECV_ACK (backpressure/flow control)

    @since %Qore 2.3
*/
class SocketQuicSendStreamingResponsePollOperation : public SocketPollSocketOperationBase {
public:
    DLLLOCAL SocketQuicSendStreamingResponsePollOperation(ExceptionSink* xsink, QoreSocketObject* sock,
                                                           int64_t session_id, int64_t stream_id,
                                                           int status_code,
                                                           const QoreHashNode* headers,
                                                           InputStream* input_stream,
                                                           QoreObject* input_stream_obj,
                                                           int64 chunk_size = 16384);

    DLLLOCAL void deref(ExceptionSink* xsink) {
        if (ROdereference()) {
            if (input_stream && !need_reassign) {
                input_stream->unassignThread(xsink);
            }
            if (input_stream_obj) {
                input_stream_obj->deref(xsink);
                input_stream_obj = nullptr;
            }
            // If destroyed without abort() (e.g. owner cancellation), clean up
            // the stream and clear non-block tracking
            if (quic_session || set_non_block) {
                AutoLocker al(sock->priv->m);
                // Cancel stream to prevent leaked streaming_body_data_ entries
                if (quic_session && ss_state != QCS_SS::DONE) {
                    ExceptionSink cancel_xsink;
                    quic_session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED,
                        &cancel_xsink);
                }
                quic_session.reset();
                if (set_non_block) {
                    sock->priv->clearNonBlock();
                }
            }
            sock->deref(xsink);
            delete this;
        }
    }

    DLLLOCAL virtual bool goalReached() const override {
        return ss_state == QCS_SS::DONE;
    }

    DLLLOCAL virtual const char* getStateImpl() const override;
    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink) override;

    DLLLOCAL virtual void abort(ExceptionSink* xsink) override {
        // Release InputStream thread affinity and references
        if (input_stream && !need_reassign) {
            input_stream->unassignThread(xsink);
        }
        input_stream = nullptr;
        if (input_stream_obj) {
            input_stream_obj->deref(xsink);
            input_stream_obj = nullptr;
        }
        current_chunk = nullptr;
        // Cancel stream via QuicSession
        {
            AutoLocker al(sock->priv->m);
            if (quic_session) {
                ExceptionSink cancel_xsink;
                quic_session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED, &cancel_xsink);
            }
            quic_session.reset();
            if (set_non_block) {
                sock->priv->clearNonBlock();
                set_non_block = false;
            }
        }
    }

private:
    std::shared_ptr<QuicSession> quic_session;
    int64_t stream_id = -1;
    QCS_SS ss_state = QCS_SS::READ_CHUNK;

    //! InputStream fields
    SimpleRefHolder<InputStream> input_stream;
    QoreObject* input_stream_obj = nullptr;  //!< Strong ref to InputStream QoreObject (prevents GC)
    int64 chunk_size;
    bool eof = false;
    bool is_pollable;
    bool need_reassign = true;
    int stream_fd = -1;
    SimpleRefHolder<BinaryNode> current_chunk;

    //! Stored remote peer address for sendto()
    struct sockaddr_storage peer_addr_{};
    socklen_t peer_addrlen_ = 0;

    //! Per-session local address for source IP pinning via sendmsg() cmsg
    /** Distinct from local_addr_ (which is the wildcard from getsockname()
        used for receive-side pktinfo).  This is the actual per-session local
        IP captured at connection creation via pktinfo.
    */
    struct sockaddr_storage send_local_addr_{};
    socklen_t send_local_addrlen_ = 0;

    //! Receive buffer for incoming packets (ACKs)
    uint8_t recv_buf_[QUIC_RECV_BUF_SIZE]{};
    //! Reusable packet batch (avoids per-call heap allocations)
    QuicPacketBatch pkt_batch_;

    //! Cached local address (from getsockname at construction time)
    struct sockaddr_storage local_addr_{};
    socklen_t local_addrlen_ = 0;

    //! Coalesced timer + write + send pending QUIC packets via UDP
    /** @param next_expiry output: next timer expiry for poll timeout
        @return 0 on success, SOCK_POLLOUT if partial send, -1 on error
    */
    DLLLOCAL int sendPendingPackets(ngtcp2_tstamp& next_expiry, ExceptionSink* xsink);

    //! Receive and process incoming packets (ACKs for flow control)
    DLLLOCAL int recvAndProcessPackets(ExceptionSink* xsink);

    DLLLOCAL virtual bool abortNeedsClose() const override {
        return false;  // UDP is connectionless
    }
};

DLLLOCAL QoreClass* initSocketPollOperationClass(QoreNamespace& qorens);

#endif // _QORE_CLASS_SOCKETPOLLOPERATION_H
