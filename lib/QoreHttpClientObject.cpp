/*
    QoreHttpClientObject.cpp

    Qore Programming Language

    Copyright (C) 2006 - 2026 Qore Technologies, s.r.o.

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

/*
    RFC 2616 HTTP 1.1
    RFC 2617 HTTP authentication
    RFC 3986 HTTP URI specification
*/

#include <qore/Qore.h>
#include <qore/QoreURL.h>
#include <qore/QoreHttpClientObject.h>
#include "qore/intern/ql_misc.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QoreHttpClientObjectIntern.h"
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/ql_compression.h"

#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_string_private.h"

#include "qore/intern/Http2Session.h"
#include "qore/intern/QuicSession.h"
#include "qore/intern/QuicCommon.h"
#include "qore/intern/QoreLibIntern.h"

#include <atomic>
#include <cassert>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <cinttypes>

// issue #3564: set the I/O timeout for reading an incoming HTTP header after an aborted outbound chunked transfer
static const int ABORTED_TIMEOUT_MS = 5000;

method_map_t method_map;
strcase_set_t header_ignore;

constexpr int SPS_SENDING = 4;
constexpr int SPS_RECEIVING_HEADER = 5;
constexpr int SPS_RECEIVING_BODY = 6;
constexpr int SPS_CONNECTING_PROXY_SSL = 7;
constexpr int SPS_RECEIVED = 8;
constexpr int SPS_H2_ACTIVE = 9;  // HTTP/2 bidirectional send/receive
constexpr int SPS_H3_ACTIVE = 10;  // HTTP/3 bidirectional send/receive via QUIC

//! Poll state for HTTP/2 client operations
/** Handles bidirectional send/receive for HTTP/2 using Http2Session.
    This state manages:
    - Connection preface sending (on first call)
    - Request submission
    - Bidirectional data flow (sending pending data + receiving frames)
    - Response completion detection
*/
class Http2ClientPollState : public AbstractPollState {
public:
    //! Constructor - creates HTTP/2 session and submits request
    /** @param xsink Exception sink
        @param sock Socket private data
        @param method HTTP method
        @param path Request path
        @param headers Request headers
        @param body Request body (can be nullptr)
        @param body_len Length of body
        @param scheme URL scheme ("https" or "http")
    */
    DLLLOCAL Http2ClientPollState(ExceptionSink* xsink, qore_socket_private* sock,
            const char* method, const char* path,
            const std::vector<std::pair<std::string, std::string>>& headers,
            const void* body, size_t body_len, const char* scheme = "https")
            : sock(sock) {
        // Create HTTP/2 session
        h2_session = Http2Session::createClient(sock, xsink, scheme);
        if (*xsink || !h2_session) {
            return;
        }

        // Send connection preface
        if (h2_session->sendConnectionPreface(xsink)) {
            return;
        }

        // Submit the request
        stream_id = h2_session->submitRequest(method, path, headers, body, body_len, xsink);
        if (*xsink || stream_id < 0) {
            return;
        }

        // Try initial non-blocking send of pending data
        h2_session->sendPendingData(0, xsink);
    }

    DLLLOCAL virtual ~Http2ClientPollState() = default;

    //! Returns the HTTP/2 session
    DLLLOCAL Http2Session* getSession() const { return h2_session.get(); }

    //! Takes ownership of the HTTP/2 session
    DLLLOCAL Http2SessionPtr takeSession() { return std::move(h2_session); }

    //! Returns the stream ID
    DLLLOCAL int32_t getStreamId() const { return stream_id; }

    //! Returns true if the response is complete
    DLLLOCAL bool isResponseComplete() const {
        if (!h2_session) {
            return false;
        }
        if (cached_stream) {
            return cached_stream->headers_complete && cached_stream->body_complete;
        }
        Http2StreamInfo* stream = h2_session->getStream(stream_id);
        return stream && stream->headers_complete && stream->body_complete;
    }

    //! Returns response headers if available
    DLLLOCAL QoreHashNode* getResponseHeaders() const {
        if (!h2_session) {
            return nullptr;
        }
        if (cached_stream) {
            if (cached_stream->headers.empty()) {
                return nullptr;
            }
            return httpMultiHeadersToQoreHash(cached_stream->headers);
        }
        Http2StreamInfo* stream = h2_session->getStream(stream_id);
        if (!stream || stream->headers.empty()) {
            return nullptr;
        }
        return httpMultiHeadersToQoreHash(stream->headers);
    }

    //! Returns the HTTP status code
    DLLLOCAL int getStatusCode() const {
        if (!h2_session) {
            return -1;
        }
        if (cached_stream) {
            return cached_stream->status_code;
        }
        Http2StreamInfo* stream = h2_session->getStream(stream_id);
        return stream ? stream->status_code : -1;
    }

    //! Returns the response body
    DLLLOCAL BinaryNode* getResponseBody(ExceptionSink* xsink) const {
        if (!h2_session) {
            return nullptr;
        }
        if (cached_stream) {
            if (cached_stream->body.empty()) {
                return nullptr;
            }
            BinaryNode* rv = new BinaryNode();
            rv->append(cached_stream->body.data(), cached_stream->body.size());
            cached_stream->body.clear();
            return rv;
        }
        // Take all available stream data
        return h2_session->takeStreamData(stream_id, 0, xsink);
    }

    /** @return:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - SOCK_POLLIN | SOCK_POLLOUT = wait for read or write
        - 0 = done (response complete)
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) override {
        if (!h2_session) {
            xsink->raiseException("HTTP2-ERROR", "no HTTP/2 session available");
            return -1;
        }

        // Check if we're done
        if (checkComplete()) {
            return 0;  // Done
        }

        // Try to send any pending data (e.g., WINDOW_UPDATE frames)
        if (h2_session->hasPendingData() || h2_session->wantWrite()) {
            int rc = h2_session->sendPendingData(0, xsink);
            if (*xsink) {
                return -1;
            }
            // If send returned -1, we need to wait for POLLOUT
            if (rc < 0) {
                // Determine poll flags
                int flags = SOCK_POLLOUT;
                if (h2_session->wantRead()) {
                    flags |= SOCK_POLLIN;
                }
                return flags;
            }
        }

        // Try to receive data
        if (h2_session->wantRead()) {
            int rv = h2_session->receiveData(0, xsink);
            if (*xsink) {
                return -1;
            }
            if (rv == 1) {
                // EOF - connection closed by peer
                if (!checkComplete()) {
                    xsink->raiseException("HTTP2-ERROR", "connection closed before response was complete");
                    return -1;
                }
                return 0;  // Done
            }
        }

        // Check again if we're done after receiving
        if (checkComplete()) {
            return 0;  // Done
        }

        // Determine what we need to poll for
        int flags = 0;
        if (h2_session->wantRead()) {
            flags |= SOCK_POLLIN;
        }
        if (h2_session->hasPendingData() || h2_session->wantWrite()) {
            flags |= SOCK_POLLOUT;
        }

        // If no flags, default to waiting for input
        return flags ? flags : SOCK_POLLIN;
    }

private:
    //! Check if the response is complete, checking cached_stream or the session
    DLLLOCAL bool checkComplete() {
        if (cached_stream) {
            return cached_stream->headers_complete && cached_stream->body_complete;
        }
        Http2StreamInfo* stream = h2_session->getStream(stream_id);
        if (stream && stream->headers_complete && stream->body_complete) {
            return true;
        }
        // Stream may have been moved to completed_streams by markStreamComplete()
        if (h2_session->hasCompletedStreams()) {
            cached_stream = h2_session->takeCompletedStream();
            if (cached_stream && cached_stream->stream_id == stream_id) {
                return cached_stream->headers_complete && cached_stream->body_complete;
            }
        }
        return false;
    }

    qore_socket_private* sock;
    Http2SessionPtr h2_session;
    int32_t stream_id = -1;
    std::unique_ptr<Http2StreamInfo> cached_stream;  //!< Locally cached stream after completion
};

//! Poll state for HTTP/3 client operations
/** Handles bidirectional send/receive for HTTP/3 using a QUIC session.
    This state manages:
    - Request submission via QuicSession::submitRequest()
    - Non-blocking QUIC I/O (send/recv UDP datagrams)
    - QUIC timer expiry handling
    - Response completion detection via completed streams
*/
class Http3ClientPollState : public AbstractPollState {
public:
    //! Constructor - submits HTTP/3 request and starts QUIC I/O
    DLLLOCAL Http3ClientPollState(ExceptionSink* xsink, std::shared_ptr<QuicSession> session,
            int fd, const struct sockaddr_storage& local_addr, socklen_t local_addrlen,
            const char* method, const char* path,
            const strcase_str_map_t& headers,
            const void* body, size_t body_len)
            : quic_session(std::move(session)), quic_fd(fd) {
        assert(quic_session);
        assert(quic_fd >= 0);

        // Save local address for ngtcp2 path construction
        memcpy(&local_addr_, &local_addr, local_addrlen);
        local_addrlen_ = local_addrlen;

        // Set fd to non-blocking mode — required for poll-based I/O
        int flags = fcntl(quic_fd, F_GETFL, 0);
        if (flags < 0) {
            xsink->raiseErrnoException("HTTP3-POLL-ERROR", errno,
                "error in fcntl() getting socket descriptor status flag");
            return;
        }
        if (!(flags & O_NONBLOCK)) {
            if (fcntl(quic_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
                xsink->raiseErrnoException("HTTP3-POLL-ERROR", errno,
                    "error in fcntl() setting socket descriptor to non-blocking");
                return;
            }
            fd_was_blocking = true;
        }

        // Submit the request
        stream_id = quic_session->submitRequest(method, path, headers, body, body_len, xsink);
        if (*xsink || stream_id < 0) {
            return;
        }

        // Initial send of pending packets (request frames)
        ngtcp2_tstamp dummy_expiry;
        sendPendingPackets(dummy_expiry, xsink);
    }

    DLLLOCAL virtual ~Http3ClientPollState() {
        // Restore blocking mode on quic_fd if we changed it
        if (fd_was_blocking && quic_fd >= 0) {
            int flags = fcntl(quic_fd, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(quic_fd, F_SETFL, flags & ~O_NONBLOCK);
            }
            fd_was_blocking = false;
        }
    }

    //! Returns the HTTP status code from the completed stream
    DLLLOCAL int getStatusCode() const {
        return completed_stream ? completed_stream->status_code : -1;
    }

    //! Returns response headers as a QoreHashNode
    DLLLOCAL QoreHashNode* getResponseHeaders() const {
        if (!completed_stream || completed_stream->headers.empty()) {
            return nullptr;
        }
        return httpMultiHeadersToQoreHash(completed_stream->headers);
    }

    //! Returns the response body as a BinaryNode
    DLLLOCAL BinaryNode* getResponseBody() const {
        if (!completed_stream || completed_stream->body.empty()) {
            return nullptr;
        }
        return new BinaryNode(completed_stream->body.data(), completed_stream->body.size());
    }

    /** @return:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - SOCK_POLLIN | SOCK_POLLOUT = wait for read or write
        - 0 = done (response complete)
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) override {
        if (!quic_session) {
            xsink->raiseException("HTTP3-POLL-ERROR", "no QUIC session available");
            return -1;
        }

        // Check for already-completed streams matching our stream_id
        if (checkCompleted()) {
            return 0;
        }

        // Drain all available packets from the socket buffer
        while (true) {
            int rv = recvAndProcessPacket(xsink);
            if (*xsink) {
                return -1;
            }
            if (rv == SOCK_POLLIN) {
                break;  // EAGAIN — no more data
            }
            // Check for completed stream after each datagram
            if (checkCompleted()) {
                return 0;
            }
        }

        // Check if connection closed
        if (quic_session->isClosed()) {
            if (!completed_stream) {
                xsink->raiseException("HTTP3-ERROR", "QUIC connection closed before response was complete");
                return -1;
            }
            return 0;
        }

        // Send pending packets (ACKs, etc.) — coalesced timer + write
        ngtcp2_tstamp next_expiry;
        int srv = sendPendingPackets(next_expiry, xsink);
        if (*xsink) {
            return -1;
        }

        // Check again for completed stream after sending
        if (checkCompleted()) {
            return 0;
        }

        // After sending, try one more non-blocking recv before yielding to poll().
        // On low-latency paths, the server response may already be in the kernel buffer.
        while (true) {
            int rrv = recvAndProcessPacket(xsink);
            if (*xsink) {
                return -1;
            }
            if (rrv == SOCK_POLLIN) {
                break;  // EAGAIN
            }
            if (checkCompleted()) {
                return 0;
            }
        }

        // Save expiry for QUIC-aware poll timeout
        last_expiry_ = next_expiry;

        // Return poll events: always POLLIN, add POLLOUT if pending writes
        int events = SOCK_POLLIN;
        if (srv == SOCK_POLLOUT || quic_session->hasPendingWrite()) {
            events |= SOCK_POLLOUT;
        }
        return events;
    }

    //! Returns the last QUIC timer expiry for poll timeout calculation
    DLLLOCAL ngtcp2_tstamp getLastExpiry() const { return last_expiry_; }

private:
    //! Check for and take a completed stream matching our stream_id
    DLLLOCAL bool checkCompleted() {
        if (quic_session->hasCompletedStreams()) {
            // Drain completed streams until we find ours
            while (quic_session->hasCompletedStreams()) {
                auto stream = quic_session->takeCompletedStream();
                if (stream && stream->stream_id == stream_id) {
                    completed_stream = std::move(stream);
                    return true;
                }
                // Discard streams that don't match (stale from previous requests)
            }
        }
        return false;
    }

    //! Send pending QUIC packets (coalesced timer + write)
    DLLLOCAL int sendPendingPackets(ngtcp2_tstamp& next_expiry, ExceptionSink* xsink) {
        auto result = quic_session->processTimerAndWrite(pkt_batch_, xsink);
        if (result.error) {
            pkt_batch_.clear();
            return -1;
        }
        next_expiry = result.next_expiry;

        if (pkt_batch_.empty()) {
            return 0;
        }

        int sent = sendQuicPacketsBatch(quic_fd, pkt_batch_, nullptr, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return SOCK_POLLOUT;
            }
            pkt_batch_.clear();
            xsink->raiseErrnoException("HTTP3-SEND-ERROR", errno, "sendto/sendmmsg() failed");
            return -1;
        }
        if (sent > 0 && sent < pkt_batch_.size()) {
            pkt_batch_.removeFront(sent);
            return SOCK_POLLOUT;
        }

        pkt_batch_.clear();
        return 0;
    }

    //! Receive and process one QUIC packet
    DLLLOCAL int recvAndProcessPacket(ExceptionSink* xsink) {
        static thread_local struct sockaddr_storage src_addr;
        static thread_local uint8_t cmsg_buf[QUIC_CMSG_BUF_SIZE];
        socklen_t src_addrlen = sizeof(src_addr);

        size_t cmsg_len = sizeof(cmsg_buf);
        ssize_t nread = recvQuicPacket(quic_fd, recv_buf_, sizeof(recv_buf_), 0,
                                        reinterpret_cast<struct sockaddr*>(&src_addr), &src_addrlen,
                                        cmsg_buf, &cmsg_len);
        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return SOCK_POLLIN;
            }
            if (errno == EMSGSIZE) {
                return 0;  // truncated datagram discarded; continue
            }
            xsink->raiseErrnoException("HTTP3-RECV-ERROR", errno, "recvmsg() failed");
            return -1;
        }

        if (nread == 0) {
            return 0;
        }

        // Use cached getsockname() address — the client socket is connect()ed,
        // so the local address is always local_addr_.
        ngtcp2_path path;
        path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr_);
        path.local.addrlen = local_addrlen_;
        path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&src_addr);
        path.remote.addrlen = src_addrlen;

        quic_session->readPacket(recv_buf_, static_cast<size_t>(nread), path, xsink);
        if (*xsink) {
            return -1;
        }

        return 0;
    }

    std::shared_ptr<QuicSession> quic_session;
    int quic_fd;
    struct sockaddr_storage local_addr_{};
    socklen_t local_addrlen_ = 0;
    int64_t stream_id = -1;
    std::unique_ptr<QuicStreamInfo> completed_stream;
    QuicPacketBatch pkt_batch_;
    uint8_t recv_buf_[QUIC_RECV_BUF_SIZE]{};
    ngtcp2_tstamp last_expiry_ = UINT64_MAX;
    bool fd_was_blocking = false;
};

// states: none [-> connecting [-> connecting-ssl]] -> sending -> receiving-header [-> receiving-body] [-> connecting-proxy-ssl] -> [received | connected]
// HTTP/2 path: none -> connecting -> connecting-ssl -> h2-active -> received
// HTTP/3 path: none -> [connecting ->] h3-active -> received
/**
    state transitions:
    - none
      -> connecting
         -> connecting-ssl
            -> sending...
      -> sending
         -> receiving-header
            -> receiving->body
               -> received
            -> connecting...
            -> sending...
            -> connecting->proxy-ssl
               -> sending...
*/
class HttpClientConnectSendRecvPollOperation : public SocketPollOperationBase {
public:
    DLLLOCAL HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink, QoreHttpClientObject* client);

    DLLLOCAL HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink, QoreHttpClientObject* client, std::string method,
            std::string path, const void* data, size_t size, const QoreHashNode* headers,
            const QoreEncoding* enc = nullptr);

    DLLLOCAL virtual void deref(ExceptionSink* xsink);

    DLLLOCAL virtual bool goalReached() const {
        return state == SPS_RECEIVED || state == SPS_CONNECTED;
    }

    DLLLOCAL virtual QoreHashNode* continuePoll(ExceptionSink* xsink);

    DLLLOCAL virtual QoreValue getOutput() const;

    DLLLOCAL virtual void abort(ExceptionSink* xsink);

private:
    std::unique_ptr<AbstractPollState> poll_state;
    QoreHttpClientObject* client;
    std::string method, path;
    const void* data = nullptr;
    size_t size = 0;
    const QoreEncoding* enc = nullptr;
    con_info connection;

    ReferenceHolder<QoreHashNode> request_headers;
    ReferenceHolder<QoreHashNode> proxy_headers;
    ReferenceHolder<QoreHashNode> info;
    ReferenceHolder<QoreHashNode> response_headers;

    int state = SPS_NONE;

    bool set_non_block = false;
    bool keep_alive = true;
    bool host_override = false;
    bool use_proxy_connect = false;
    bool path_already_encoded = false;
    bool bodyp = false;
    bool close_connection = false;
    bool connect_only;
    const char* proxy_path = nullptr;

    unsigned redirect_count = 0;

    SimpleRefHolder<BinaryNode> send_data_holder;
    SimpleRefHolder<BinaryNode> recv_data_holder;
    int http_response_code = -1;

    DLLLOCAL virtual const char* getStateImpl() const {
        switch (state) {
            case SPS_NONE:
                return "none";
            case SPS_CONNECTING:
                return "connecting";
            case SPS_CONNECTING_SSL:
                return "connecting-ssl";
            case SPS_CONNECTING_PROXY_SSL:
                return "connecting-proxy-ssl";
            case SPS_SENDING:
                return "sending";
            case SPS_RECEIVING_HEADER:
                return "receiving-header";
            case SPS_RECEIVING_BODY:
                return "receiving-body";
            case SPS_RECEIVED:
                return "received";
            case SPS_CONNECTED:
                return "connected";
            case SPS_H2_ACTIVE:
                return "h2-active";
            case SPS_H3_ACTIVE:
                return "h3-active";
            default:
                assert(false);
        }
        return "";
    }

    DLLLOCAL int checkContinuePoll(ExceptionSink* xsink, bool keep = false) {
        assert(poll_state.get());

        // see if we are able to continue
        int rc = poll_state->continuePoll(xsink);
        //printd(5, "HttpClientConnectSendRecvPollOperation::continuePoll() state: %s rc: %d (exp: %d)\n", getStateImpl(),
        //    rc, (int)*xsink);
        if (*xsink) {
            assert(rc < 0);
            state = SPS_NONE;
            return -1;
        }
        if (!rc && !keep) {
            // release the AbstractPollState value
            poll_state.reset();
        }
        return rc;
    }

    DLLLOCAL int connectOrSend(ExceptionSink* xsink);
    DLLLOCAL int connectDone(ExceptionSink* xsink);
    DLLLOCAL int startSend(ExceptionSink* xsink);
    DLLLOCAL BinaryNode* getMessage(QoreString& hdr);
    DLLLOCAL int processResponse(ExceptionSink* xsink);
    DLLLOCAL int redirect(ExceptionSink* xsink);
    DLLLOCAL int processReceivedBody(ExceptionSink* xsink);
    DLLLOCAL int processH2Response(ExceptionSink* xsink);
    DLLLOCAL int processH3Response(ExceptionSink* xsink);
    DLLLOCAL int responseDone(ExceptionSink* xsink);
    DLLLOCAL void setFinal(int final_state);
};

static const QoreStringNode* get_string_header_node(ExceptionSink* xsink, QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
    QoreValue n = h.getKeyValue(header);
    if (n.isNothing())
        return nullptr;

    qore_type_t t = n.getType();
    if (t == NT_STRING)
        return n.get<const QoreStringNode>();
    assert(t == NT_LIST);
    if (!allow_multiple) {
        xsink->raiseException("HTTP-HEADER-ERROR", "multiple \"%s\" headers received in HTTP message", header);
        return nullptr;
    }
    // convert list to a comma-separated string
    const QoreListNode* l = n.get<const QoreListNode>();
    // get first list entry
    n = l->retrieveEntry(0);
    assert(n.getType() == NT_STRING);
    QoreStringNode* rv = n.get<QoreStringNode>()->copy();
    for (size_t i = 1; i < l->size(); ++i) {
        n = l->retrieveEntry(i);
        assert(n.getType() == NT_STRING);
        rv->concat(',');
        qore_string_private::get(rv)->concat(n.get<const QoreStringNode>());
    }
    // dereference old list and save reference to return value in header hash
    h.setKeyValue(header, rv, xsink);
    return rv;
}

static const char* get_string_header(ExceptionSink* xsink, QoreHashNode& h, const char* header,
        bool allow_multiple = false) {
   const QoreStringNode* str = get_string_header_node(xsink, h, header, allow_multiple);
   return str && !str->empty() ? str->c_str() : nullptr;
}

static const char* get_http_status_message(int code) {
    switch (code) {
        // 1xx: Informational
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 102: return "Processing";
        // 2xx: Success
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 203: return "Non-Authoritative Information";
        case 204: return "No Content";
        case 205: return "Reset Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 208: return "Already Reported";
        case 226: return "IM Used";
        // 3xx: Redirection
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 305: return "Use Proxy";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        // 4xx: Client Errors
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 407: return "Proxy Authentication Required";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 410: return "Gone";
        case 411: return "Length Required";
        case 412: return "Precondition Failed";
        case 413: return "Request Entity Too Large";
        case 414: return "Request-URI Too Long";
        case 415: return "Unsupported Media Type";
        case 416: return "Requested Range Not Satisfiable";
        case 417: return "Expectation Failed";
        case 418: return "I'm a teapot";
        case 420: return "Enhance Your Calm";
        case 422: return "Unprocessable Entity";
        case 423: return "Locked";
        case 424: return "Failed Dependency";
        case 425: return "Unordered Collection";
        case 426: return "Upgrade Required";
        case 428: return "Precondition Required";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        // 5xx: Server Errors
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 509: return "Bandwidth Limit Exceeded";
        case 510: return "Not Extended";
        case 511: return "Network Authentication Required";
    }
    return "Unknown";
}

static void set_http2_response_info(ExceptionSink* xsink, QoreHashNode& headers, QoreHashNode& info,
        int code) {
    headers.setKeyValue("status_code", code, xsink);
    const char* status_msg = get_http_status_message(code);
    headers.setKeyValue("status_message", new QoreStringNode(status_msg), xsink);
    headers.setKeyValue("http_version", new QoreStringNode("2"), xsink);

    QoreStringNode* response_uri = new QoreStringNode();
    response_uri->sprintf("HTTP/2 %d %s", code, status_msg);
    info.setKeyValue("response-uri", response_uri, xsink);
}

static void set_body_content_type_info(ExceptionSink* xsink, QoreHashNode& headers, QoreHashNode& info) {
    const QoreStringNode* ct = get_string_header_node(xsink, headers, "content-type", true);
    if (*xsink || !ct || ct->empty()) {
        return;
    }

    const char* start = ct->c_str();
    while (*start == ' ') {
        ++start;
    }
    const char* end = start;
    while (*end && *end != ';' && *end != ',') {
        ++end;
    }
    QoreStringNode* base_ct = new QoreStringNode();
    if (end > start) {
        base_ct->concat(start, end - start);
    }
    base_ct->trim();
    if (!base_ct->empty()) {
        info.setKeyValue("body-content-type", base_ct, xsink);
    } else {
        base_ct->deref(xsink);
    }

    QoreValue orig = headers.getKeyValue("_qore_orig_content_type");
    if (orig.getType() != NT_STRING) {
        return;
    }

    const char* orig_str = orig.get<const QoreStringNode>()->c_str();
    const char* p = strstr(orig_str, "charset=");
    if (!p || (p != orig_str && *(p - 1) != ';' && *(p - 1) != ' ')) {
        return;
    }

    const char* c = p + 8;
    char quote = '\0';
    if (*c == '\'' || *c == '"') {
        quote = *c;
        ++c;
    }
    QoreString enc;
    while (*c && *c != ';' && *c != ' ' && *c != quote) {
        enc.concat(*(c++));
    }
    if (!enc.empty()) {
        info.setKeyValue("charset", new QoreStringNode(enc.c_str()), xsink);
    }
}

static qore_uncompress_to_string_t get_decoder_for_content_encoding(const char* content_encoding,
        bool& ignore_encoding) {
    ignore_encoding = false;
    if (!content_encoding) {
        return nullptr;
    }

    const char* start = content_encoding;
    while (*start == ' ' || *start == '\t') {
        ++start;
    }
    const char* end = start;
    while (*end && *end != ',' && *end != ';' && *end != ' ') {
        ++end;
    }
    if (end <= start) {
        return nullptr;
    }

    QoreString token;
    token.concat(start, end - start);
    const char* tok = token.c_str();

    if (!strcasecmp(tok, "identity")) {
        ignore_encoding = true;
        return nullptr;
    }

    if (!strcasecmp(tok, "deflate") || !strcasecmp(tok, "x-deflate")) {
        return qore_inflate_to_string;
    }
    if (!strcasecmp(tok, "gzip") || !strcasecmp(tok, "x-gzip")) {
        return qore_gunzip_to_string;
    }
    if (!strcasecmp(tok, "bzip2") || !strcasecmp(tok, "x-bzip2")) {
        return qore_bunzip2_to_string;
    }
    if (!strcasecmp(tok, "br")) {
        return qore_unbrotli_to_string;
    }
    if (!strcasecmp(tok, "zstd")) {
        return qore_unzstd_to_string;
    }

    return nullptr;
}

//! Returns a binary decompressor for the given content-encoding, or nullptr if none needed.
/** Used by the HTTP/2 and HTTP/3 binary response paths where the body stays as BinaryNode.
    @param content_encoding the Content-Encoding header value (must not be nullptr)
    @param xsink for raising exceptions on unknown encodings
    @return the decompressor function, or nullptr for identity/character encodings
*/
static qore_uncompress_to_binary_t get_binary_decoder_for_content_encoding(const char* content_encoding,
        ExceptionSink* xsink) {
    if (!strcasecmp(content_encoding, "deflate") || !strcasecmp(content_encoding, "x-deflate")) {
        return qore_inflate_to_binary;
    }
    if (!strcasecmp(content_encoding, "gzip") || !strcasecmp(content_encoding, "x-gzip")) {
        return qore_gunzip_to_binary;
    }
    if (!strcasecmp(content_encoding, "bzip2") || !strcasecmp(content_encoding, "x-bzip2")) {
        return qore_bunzip2_to_binary;
    }
    if (!strcasecmp(content_encoding, "br")) {
        return qore_unbrotli_to_binary;
    }
    if (!strcasecmp(content_encoding, "zstd")) {
        return qore_unzstd_to_binary;
    }
    if (!strcasecmp(content_encoding, "identity")) {
        return nullptr;
    }
    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
        "don't know how to handle content-encoding '%s'", content_encoding);
    return nullptr;
}

static QoreValue process_binary_body(const BinaryNode* bin, const QoreEncoding* body_enc,
        const char* content_encoding, qore_uncompress_to_string_t dec, bool encoding_passthru,
        ExceptionSink* xsink) {
    if (!bin || !bin->size()) {
        return QoreValue();
    }

    if (content_encoding) {
        if (encoding_passthru) {
            return bin->refSelf();
        }
        if (!dec) {
            bool ignore_encoding = false;
            dec = get_decoder_for_content_encoding(content_encoding, ignore_encoding);
            if (ignore_encoding) {
                return new QoreStringNode((const char*)bin->getPtr(), bin->size(), body_enc);
            }
            if (!dec) {
                xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "don't know how to handle content-encoding '%s'",
                    content_encoding);
                return QoreValue();
            }
        }
        QoreStringNode* decoded = dec(bin, body_enc, xsink);
        return decoded;
    }

    return new QoreStringNode((const char*)bin->getPtr(), bin->size(), body_enc);
}

void do_content_length_event(Queue* event_queue, qore_socket_private* priv, size_t len) {
    if (event_queue) {
        QoreHashNode* h = priv->getEvent(QORE_EVENT_HTTP_CONTENT_LENGTH, QORE_SOURCE_HTTPCLIENT);
        qore_hash_private* hh = qore_hash_private::get(*h);
        hh->setKeyValueIntern("len", len);
        event_queue->pushAndTakeRef(h);
    }
}

void do_redirect_event(Queue* event_queue, qore_socket_private* priv, const QoreStringNode* loc,
        const QoreStringNode* msg) {
    if (event_queue) {
        QoreHashNode* h = priv->getEvent(QORE_EVENT_HTTP_REDIRECT, QORE_SOURCE_HTTPCLIENT);
        qore_hash_private* hh = qore_hash_private::get(*h);
        hh->setKeyValueIntern("location", loc->refSelf());
        if (msg)
            hh->setKeyValueIntern("status_message", msg->refSelf());
        event_queue->pushAndTakeRef(h);
    }
}

void do_event(Queue* event_queue, qore_socket_private* priv, int event) {
    if (event_queue) {
        QoreHashNode* h = priv->getEvent(event, QORE_SOURCE_HTTPCLIENT);
        event_queue->pushAndTakeRef(h);
    }
}

struct qore_httpclient_priv {
    my_socket_priv* msock;

    prot_map_t prot_map;

    con_info connection, proxy_connection;

    // issue #3978: default output encoding
    const QoreEncoding* enc = nullptr;

    bool
        // are we using http 1.1 or 1.0?
        http11 = true,
        // when set, TCP_NODELAY is used on the socket
        nodelay = false,
        /** means that a CONNECT message has been processed and the connection is now made as if it were directly with
            the client
        */
        proxy_connected = false,
        // turns off implicit connections for the current connection only
        persistent = false,
        // HTTP response errors do not result in exceptions being thrown
        error_passthru = false,
        // redirect messages will not be processed but rather passed to the caller
        redirect_passthru = false,
        // known content encodings are not decoded when set
        encoding_passthru = false,
        // if URLs are pre-encoded
        pre_encoded_urls = false,
        // HTTP/2 is currently active on the connection
        http2_active = false,
        // h2c upgrade is pending (for HTTP2_MODE_H2C_UPGRADE)
        h2c_upgrade_pending = false
        ;

    // HTTP/2 mode (HTTP2_MODE_DISABLED, HTTP2_MODE_AUTO, HTTP2_MODE_REQUIRED, HTTP2_MODE_H2C_*)
    int http2_mode = HTTP2_MODE_AUTO
        ;

    // HTTP/3 mode (HTTP3_MODE_DISABLED, HTTP3_MODE_AUTO, HTTP3_MODE_REQUIRED)
    // Atomic: may be read from I/O thread while set from API thread
    std::atomic<int> http3_mode{HTTP3_MODE_AUTO};

    // HTTP/3 is currently active on the connection
    // Atomic: read from public isHttp3Active() without locking
    std::atomic<bool> http3_active{false};

    // Alt-Svc cache entry
    struct AltSvcEntry {
        int port;                    // QUIC port from Alt-Svc header
        int64 expiry_epoch;          // When this entry expires (epoch seconds)
        int64 retry_after_epoch{0};  // After QUIC failure, don't retry before this time
    };

    // Alt-Svc cache: "host:port" → {quic_port, expiry}
    // Thread safety: protected by msock->m (held on all access paths)
    std::unordered_map<std::string, AltSvcEntry> alt_svc_cache;

    // QUIC connection state
    std::shared_ptr<QuicSession> quic_session;
    int quic_fd = -1;  // UDP socket fd for QUIC
    struct sockaddr_storage quic_local_addr{};
    socklen_t quic_local_addrlen = 0;

    // Current HTTP/2 stream ID (for WebSocket over HTTP/2)
    int32_t h2_stream_id = 0;

    // Helper to access the socket's HTTP/2 session
    // NOTE: The h2_session is now stored on the socket, not HTTPClient,
    // so that all layers (Socket, HTTPClient) share the same session
    DLLLOCAL Http2Session* getH2Session() const {
        return msock ? msock->socket->priv->h2_session.get() : nullptr;
    }

    DLLLOCAL void setH2Session(const Http2SessionPtr& session) {
        if (msock) {
            msock->socket->priv->h2_session = session;
        }
    }

    DLLLOCAL void clearH2Session() {
        // Reset the shared_ptr - will delete session if this is the last reference
        if (msock) {
            msock->socket->priv->h2_session.reset();
            msock->socket->priv->setH2ActiveStreamId(-1);
        }
        h2_stream_id = 0;
        http2_active = false;
    }

    // Sets the active HTTP/2 stream ID on both HTTPClient and socket
    DLLLOCAL void setActiveH2StreamId(int32_t stream_id) {
        h2_stream_id = stream_id;
        if (msock) {
            msock->socket->priv->setH2ActiveStreamId(stream_id);
        }
    }

    // persistent count
    unsigned persistent_count = 0;

    int default_port = HTTPCLIENT_DEFAULT_PORT,
        max_redirects = HTTPCLIENT_DEFAULT_MAX_REDIRECTS;

    std::string default_path;
    int timeout = HTTPCLIENT_DEFAULT_TIMEOUT;
    std::string socketpath;
    header_map_t default_headers;
    static header_map_t static_default_headers;
    int connect_timeout_ms = HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT;

    method_map_t additional_methods_map;

    // characters that must be encoded when pre_encoded_urls is enabled - all control chars plus {}|\\^~[]`
    static constexpr const char* must_encode_chars = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
        "\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f{}|\\^~[]`";

    // characters subject to percent encoding by Qore
    typedef std::map<char, const char*> pct_encoding_map_t;
    static pct_encoding_map_t pct_encoding_map;

    // any local map for this object for additional characters to encode
    typedef std::set<char> pct_encoding_set_t;
    pct_encoding_set_t local_pct_encoding_set;

    DLLLOCAL qore_httpclient_priv(my_socket_priv* ms) :
            msock(ms),
            connection(HTTPCLIENT_DEFAULT_PORT) {
        assert(ms);
        // setup protocol map
        prot_map["http"] = make_protocol(80, false);
        prot_map["https"] = make_protocol(443, true);

        // setup default headers
        default_headers = static_default_headers;
    }

    DLLLOCAL ~qore_httpclient_priv() {
        disconnect_unlocked();
    }

    QoreHashNode* getConfig(my_socket_priv& priv) const {
        qore_socket_private& sock = *qore_socket_private::get(*priv.socket);
        ReferenceHolder<QoreHashNode> rv(new QoreHashNode, nullptr);

        qore_hash_private* h = qore_hash_private::get(**rv);
        if (!additional_methods_map.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : additional_methods_map) {
                hh->setKeyValueIntern(i.first.c_str(), i.second);
            }
            h->setKeyValueIntern("additional_methods", amm.release());
        }
        if (sock.assume_http_encoding != "ISO-8859-1") {
            h->setKeyValueIntern("assume_encoding", new QoreStringNode(sock.assume_http_encoding));
        }
        if (connect_timeout_ms != HTTPCLIENT_DEFAULT_CONNECT_TIMEOUT) {
            h->setKeyValueIntern("connect_timeout", connect_timeout_ms);
        }
        if (!default_path.empty()) {
            h->setKeyValueIntern("default_path", new QoreStringNode(default_path));
        }
        if (default_port != HTTPCLIENT_DEFAULT_PORT) {
            h->setKeyValueIntern("default_port", default_port);
        }
        if (!local_pct_encoding_set.empty()) {
            SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
            for (auto& i : local_pct_encoding_set) {
                str->concat(i);
            }
            h->setKeyValueIntern("encode_chars", str.release());
        }
        if (enc) {
            h->setKeyValueIntern("encoding", new QoreStringNode(enc->getCode()));
        }
        if (encoding_passthru) {
            h->setKeyValueIntern("encoding_passthru", encoding_passthru);
        }
        if (error_passthru) {
            h->setKeyValueIntern("error_passthru", error_passthru);
        }
        if (!default_headers.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : default_headers) {
                header_map_t::const_iterator hi = static_default_headers.find(i.first);
                if (hi != static_default_headers.end() && hi->second == i.second) {
                    continue;
                }
                hh->setKeyValueIntern(i.first.c_str(), new QoreStringNode(i.second));
            }
            if (!amm->empty()) {
                h->setKeyValueIntern("headers", amm.release());
            }
        }
        // Output http_version based on http3_mode, http2_mode, and http11
        const char* version_str;
        if (http3_mode == HTTP3_MODE_REQUIRED) {
            version_str = "3.0";
        } else {
            switch (http2_mode) {
                case HTTP2_MODE_AUTO:
                    version_str = "auto";
                    break;
                case HTTP2_MODE_REQUIRED:
                    version_str = "2.0";
                    break;
                default:
                    version_str = http11 ? "1.1" : "1.0";
                    break;
            }
        }
        h->setKeyValueIntern("http_version", new QoreStringNode(version_str));
        // HTTP/3 mode
        {
            const char* h3_mode_str;
            switch (http3_mode) {
                case HTTP3_MODE_DISABLED:
                    h3_mode_str = "disabled";
                    break;
                case HTTP3_MODE_REQUIRED:
                    h3_mode_str = "required";
                    break;
                case HTTP3_MODE_AUTO:
                default:
                    h3_mode_str = "auto";
                    break;
            }
            h->setKeyValueIntern("http3_mode", new QoreStringNode(h3_mode_str));
        }
        if (max_redirects != HTTPCLIENT_DEFAULT_MAX_REDIRECTS) {
            h->setKeyValueIntern("max_redirects", max_redirects);
        }
        if (!connection.password.empty()) {
            h->setKeyValueIntern("password", new QoreStringNode(connection.password));
        }
        if (pre_encoded_urls) {
            h->setKeyValueIntern("pre_encoded_urls", pre_encoded_urls);
        }
        if (!prot_map.empty()) {
            ReferenceHolder<QoreHashNode> amm(new QoreHashNode, nullptr);
            qore_hash_private* hh = qore_hash_private::get(**amm);
            for (auto& i : prot_map) {
                // skip standard protocols always present
                if (i.second == -443 || i.second == 80) {
                    continue;
                }
                if (i.second < 0) {
                    ReferenceHolder<QoreHashNode> v(new QoreHashNode, nullptr);
                    qore_hash_private* vh = qore_hash_private::get(**v);
                    vh->setKeyValueIntern("ssl", true);
                    vh->setKeyValueIntern("port", -i.second);
                    hh->setKeyValueIntern(i.first.c_str(), v.release());
                } else {
                    hh->setKeyValueIntern(i.first.c_str(), i.second);
                }
            }
            if (!amm->empty()) {
                h->setKeyValueIntern("protocols", amm.release());
            }
        }
        if (proxy_connection.has_url()) {
            h->setKeyValueIntern("proxy", proxy_connection.get_url());
        }
        if (redirect_passthru) {
            h->setKeyValueIntern("redirect_passthru", redirect_passthru);
        }
        if (priv.cert) {
            h->setKeyValueIntern("ssl_cert_data", priv.cert->getDER(nullptr));
        }
        if (priv.pk) {
            h->setKeyValueIntern("ssl_key_data", priv.pk->getDER(nullptr));
        }
        if (sock.ssl_verify_mode == SSL_VERIFY_PEER) {
            h->setKeyValueIntern("ssl_verify_cert", true);
        }
        if (timeout != HTTPCLIENT_DEFAULT_TIMEOUT) {
            h->setKeyValueIntern("timeout", timeout);
        }
        if (connection.has_url()) {
            h->setKeyValueIntern("url", connection.get_url(URL_NO_AUTH));
        }
        if (!connection.username.empty()) {
            h->setKeyValueIntern("username", new QoreStringNode(connection.username));
        }
        return rv.release();
    }

    DLLLOCAL void setSocketPathIntern(const con_info& con) {
        if (con.path.empty() || !con.host.empty()) {
            socketpath = con.host;
            if (!con.is_unix) {
                socketpath += ":";
                char buff[20];
                sprintf(buff, "%d", con.port);
                socketpath += buff;
            }
            return;
        }

        socketpath = con.path;
    }

    DLLLOCAL void setSocketPath(const con_info& connection) {
        setSocketPathIntern(proxy_connection.has_url() ? proxy_connection : connection);
        //printd(5, "setSocketPath() '%s'\n", socketpath.c_str());
    }

    DLLLOCAL void lock() { msock->m.lock(); }
    DLLLOCAL void unlock() { msock->m.unlock(); }

    DLLLOCAL QoreObject* startPollSendRecv(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client,
            const QoreString* method, const QoreString* path, const AbstractQoreNode* data_save, const void* data,
            size_t size, const QoreHashNode* headers, const QoreEncoding* enc = nullptr) {
        client->ref();
        ReferenceHolder<HttpClientConnectSendRecvPollOperation> poller(
            new HttpClientConnectSendRecvPollOperation(xsink, client,
                method->c_str(), path ? path->c_str() : "", data, size, headers, enc
            ), xsink);
        if (*xsink) {
            return nullptr;
        }
        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", self->objectRefSelf(), xsink);
            rv->setValue("goal", new QoreStringNode("received"), xsink);
            if (data_save && size) {
                assert(data);
                rv->setValue("data_save", data_save->refSelf(), xsink);
            } else {
                assert(!data || !size);
            }
        }
        return rv.release();
    }

    DLLLOCAL QoreObject* startPollConnect(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client) {
        //printd(5, "qore_http_client_priv::startPoll() self: %p client: %p msock: %p (== %p)\n", self, client,
        //    msock, client->priv);

        client->ref();
        ReferenceHolder<SocketPollOperationBase> poller(new HttpClientConnectSendRecvPollOperation(xsink, client),
            xsink);
        if (*xsink) {
            return nullptr;
        }
        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", self->objectRefSelf(), xsink);
            rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return rv.release();
    }

    // returns -1 if an exception was thrown, 0 for OK
    DLLLOCAL int connect_unlocked(ExceptionSink* xsink, const con_info& connection) {
        assert(!msock->socket->isOpen());
        bool connect_ssl = proxy_connection.has_url()
            ? proxy_connection.ssl
            : connection.ssl;

        // Set up ALPN protocols based on current HTTP/2 mode and global settings
        // Always re-evaluate to handle mode changes on reused connections
        if (connect_ssl) {
            // Check global HTTP/2 mode
            int global_mode = qore_global_http2_mode.load(std::memory_order_relaxed);
            bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
            bool http2_enabled = (http2_mode == HTTP2_MODE_AUTO || http2_mode == HTTP2_MODE_REQUIRED)
                && global_mode != HTTP2_MODE_DISABLED && !lib_disabled;

            if (http2_enabled) {
                // Set ALPN protocols for HTTP/2
                ReferenceHolder<QoreListNode> protocols(new QoreListNode(autoTypeInfo), xsink);
                if (http2_mode == HTTP2_MODE_REQUIRED) {
                    // HTTP/2 only
                    protocols->push(new QoreStringNode("h2"), xsink);
                    if (*xsink) {
                        return -1;
                    }
                } else {
                    // Auto mode: prefer h2, fall back to http/1.1
                    protocols->push(new QoreStringNode("h2"), xsink);
                    if (*xsink) {
                        return -1;
                    }
                    protocols->push(new QoreStringNode("http/1.1"), xsink);
                    if (*xsink) {
                        return -1;
                    }
                }
                msock->socket->setAlpnProtocols(*protocols, xsink);
                if (*xsink) {
                    return -1;
                }
            } else {
                // HTTP/2 is disabled - clear any previously set ALPN protocols
                msock->socket->clearAlpnProtocols();
            }
        }

        int rc;
        if (connect_ssl) {
            rc = msock->socket->connectSSL(xsink, socketpath.c_str(), connect_timeout_ms, msock->cert, msock->pk);
        } else {
            rc = msock->socket->connect(socketpath.c_str(), connect_timeout_ms, xsink);
        }

        if (!rc) {
            setNoDelay();
            // Determine effective HTTP/2 mode based on global setting and object setting
            // Check both the new global mode and the legacy QLO_DISABLE_HTTP2 library option
            int global_mode = qore_global_http2_mode.load(std::memory_order_relaxed);
            bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
            int effective_http2_mode;
            if (global_mode == HTTP2_MODE_DISABLED || lib_disabled) {
                // Global disabled or library option overrides object setting
                effective_http2_mode = HTTP2_MODE_DISABLED;
            } else if (global_mode == HTTP2_MODE_REQUIRED) {
                // Global required overrides object setting (unless object explicitly uses h2c modes)
                effective_http2_mode = (http2_mode == HTTP2_MODE_H2C_DIRECT || http2_mode == HTTP2_MODE_H2C_UPGRADE)
                    ? http2_mode : HTTP2_MODE_REQUIRED;
            } else {
                // Global auto: use object's setting
                effective_http2_mode = http2_mode;
            }
            // Handle h2c (HTTP/2 cleartext) modes
            if (effective_http2_mode == HTTP2_MODE_H2C_DIRECT && !connect_ssl) {
                // Direct h2c: start HTTP/2 immediately with prior knowledge
                http2_active = true;
                // HTTP/2 sends many small frames as separate writes; enable TCP_NODELAY
                // to prevent Nagle + delayed ACK interaction causing ~40ms per-request delays
                msock->socket->setNoDelay(1);
                setH2Session(Http2Session::createClient(msock->socket->priv, xsink, "http"));
                if (*xsink) {
                    msock->socket->close();
                    http2_active = false;
                    return -1;
                }
                // Send connection preface
                if (getH2Session()->sendConnectionPreface(xsink)) {
                    clearH2Session();
                    msock->socket->close();
                    http2_active = false;
                    return -1;
                }
            } else if (effective_http2_mode == HTTP2_MODE_H2C_UPGRADE && !connect_ssl) {
                // h2c upgrade via HTTP/1.1 Upgrade header is deprecated by RFC 9113 §3.2
                // RFC 9113 removed the HTTP/1.1 Upgrade mechanism for HTTP/2.
                // Use h2c (h2c-direct / prior knowledge) mode instead.
                xsink->raiseException("HTTP2-ERROR", "h2c-upgrade mode is not supported "
                    "(deprecated by RFC 9113); use h2c (h2c-direct) mode for HTTP/2 cleartext "
                    "with prior knowledge");
                msock->socket->close();
                return -1;
            } else
            // Check if HTTP/2 was negotiated via ALPN (for TLS connections)
            if (effective_http2_mode != HTTP2_MODE_DISABLED && connect_ssl) {
                http2_active = msock->socket->isHttp2();
                // If HTTP/2 is required but not available, raise an exception
                if (effective_http2_mode == HTTP2_MODE_REQUIRED && !http2_active) {
                    xsink->raiseException("HTTP2-REQUIRED-ERROR", "HTTP/2 is required but the server does not "
                        "support HTTP/2 via ALPN");
                    msock->socket->close();
                    return -1;
                }
                // Create HTTP/2 session if HTTP/2 is active
                if (http2_active) {
                    // HTTP/2 sends many small frames as separate writes; enable TCP_NODELAY
                    // to prevent Nagle + delayed ACK interaction causing ~40ms per-request delays
                    msock->socket->setNoDelay(1);
                    setH2Session(Http2Session::createClient(msock->socket->priv, xsink));
                    if (*xsink) {
                        msock->socket->close();
                        http2_active = false;
                        return -1;
                    }
                    // Send connection preface
                    if (getH2Session()->sendConnectionPreface(xsink)) {
                        clearH2Session();
                        msock->socket->close();
                        http2_active = false;
                        return -1;
                    }
                }
            }
        }
        return rc;
    }

    DLLLOCAL void setNoDelay() {
        if (nodelay) {
            if (msock->socket->setNoDelay(1)) {
                nodelay = false;
            }
        }
    }

    DLLLOCAL void disconnect_unlocked() {
        if (msock->socket->isOpen()) {
            msock->socket->close();
            proxy_connected = false;
            persistent = false;
            persistent_count = 0;
            clearH2Session();
        }
        disconnectQuic();
    }

    // Disconnect QUIC session and clean up state
    // Always invalidates Alt-Svc cache (even on normal idle disconnects) so we
    // fall back to HTTP/1.x or HTTP/2 on the next request; the server will
    // re-advertise h3 support via Alt-Svc if it's still available.  This is
    // conservative but correct — stale cache entries for a down server would
    // cause repeated connection failures.
    // Thread safety: caller must hold priv->m (same as msock->m), or be in the
    // destructor where concurrent access is impossible.
    DLLLOCAL void disconnectQuic() {
        if (!connection.host.empty() && connection.port) {
            std::string origin = std::string(connection.host) + ":" + std::to_string(connection.port);
            alt_svc_cache.erase(origin);
        }
        // Send CONNECTION_CLOSE for graceful shutdown (best-effort, non-blocking).
        // The UDP socket is connect()-ed, so send() works without specifying the peer.
        if (quic_session && quic_fd >= 0) {
            static thread_local uint8_t close_buf[1280];
            ssize_t nwrite = quic_session->writeConnectionClose(close_buf, sizeof(close_buf));
            if (nwrite > 0) {
                // Best-effort: ignore send errors (fd may already be invalid)
                // MSG_DONTWAIT: non-blocking (available on Linux + macOS)
                // SIGPIPE already ignored process-wide (QoreSignalManager::init)
                ::send(quic_fd, close_buf, static_cast<size_t>(nwrite), MSG_DONTWAIT);
            }
        }
        quic_session.reset();
        if (quic_fd >= 0) {
            ::close(quic_fd);
            quic_fd = -1;
        }
        http3_active = false;
    }

    DLLLOCAL int setNoDelay(bool nd) {
        AutoLocker al(msock->m);

        if (!msock->socket->isOpen()) {
            nodelay = true;
            return 0;
        }

        if (nodelay) {
            return 0;
        }

        if (msock->socket->setNoDelay(1)) {
            return -1;
        }

        nodelay = true;
        return 0;
    }

    DLLLOCAL bool getNoDelay() const {
        return nodelay;
    }

    DLLLOCAL void setPersistent(ExceptionSink* xsink) {
        AutoLocker al(msock->m);

        if (!msock->socket->isOpen()) {
            xsink->raiseException("PERSISTENCE-ERROR", "HTTPClient::setPersistent() can only be called once an "
                "initial connection has been established; currently there is no connection to the server");
            return;
        }

        if (!persistent) {
            persistent = true;
        }
        ++persistent_count;
    }

    DLLLOCAL void clearPersistent() {
        AutoLocker al(msock->m);

        if (!persistent_count) {
            return;
        }
        if (!--persistent_count) {
            assert(persistent);
            persistent = false;
        }
    }

    // issue #3474: process redirect messages correctly
    // NOTE: callers pass a local copy of `connection` (e.g., `this_connection` in send_internal),
    // so redirects only affect the current request chain, not the client's base URL
    DLLLOCAL int redirectUrlUnlocked(const char* str, con_info& connection, ExceptionSink* xsink) {
        QoreURL url(str);
        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "redirect location '%s' cannot be parsed", str);
            return -1;
        }

        // check if the location is only a path, in which case we need to keep the rest of the connection info the same
        if (!url.getPort() && !url.getHost() && url.getPath()) {
            connection.path = url.getPath()->c_str();
            return 0;
        }

        bool port_set = false;
        if (connection.set_url(url, port_set, xsink)) {
            return -1;
        }

        const QoreString* tmp = url.getProtocol();
        if (tmp) {
            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            if (i == prot_map.end()) {
                xsink->raiseException("HTTP-CLIENT-UNKNOWN-PROTOCOL", "protocol '%s' in redirect message is unknown",
                    tmp->c_str());
                return -1;
            }

            // set port only if it wasn't overridden in the URL
            if (!port_set && !connection.is_unix) {
                connection.port = get_port(i->second);
            }

            // set SSL setting from protocol default
            connection.ssl = get_ssl(i->second);
        } else {
            connection.ssl = false;
            if (!port_set && !connection.is_unix) {
                connection.port = default_port;
            }
        }

        if (!proxy_connection.has_url()) {
            setSocketPath(connection);
        }

        return 0;
    }

    DLLLOCAL int setUrlUnlocked(const char* str, ExceptionSink* xsink) {
        QoreURL url(str);

        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "URL '%s' cannot be parsed", str);
            return -1;
        }

        bool port_set = false;
        if (connection.set_url(url, port_set, xsink)) {
            return -1;
        }

        const QoreString* tmp = url.getProtocol();
        if (tmp) {
            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            if (i == prot_map.end()) {
                xsink->raiseException("HTTP-CLIENT-UNKNOWN-PROTOCOL", "protocol '%s' is not supported",
                    tmp->c_str());
                return -1;
            }

            // set port only if it wasn't overridden in the URL
            if (!port_set && !connection.is_unix) {
                connection.port = get_port(i->second);
            }

            // set SSL setting from protocol default
            connection.ssl = get_ssl(i->second);
        } else {
            connection.ssl = false;
            if (!port_set && !connection.is_unix) {
                connection.port = default_port;
            }
        }

        if (!proxy_connection.has_url()) {
            setSocketPath(connection);
        }

        return 0;
    }

    DLLLOCAL int setProxyUrlUnlocked(const char* pstr, ExceptionSink* xsink) {
        QoreURL url(pstr);

        if (!url.isValid()) {
            xsink->raiseException("HTTP-CLIENT-URL-ERROR", "proxy URL '%s' cannot be parsed", pstr);
            return -1;
        }

        bool port_set = false;
        if (proxy_connection.set_url(url, port_set, xsink))
            return -1;

        const QoreString *tmp = url.getProtocol();
        if (tmp) {
            if (strcasecmp(tmp->c_str(), "http") && strcasecmp(tmp->c_str(), "https")) {
                xsink->raiseException("HTTP-CLIENT-PROXY-PROTOCOL-ERROR", "protocol '%s' is not supported for "
                    "proxies, only 'http' and 'https'", tmp->c_str());
                return -1;
            }

            prot_map_t::const_iterator i = prot_map.find(tmp->c_str());
            assert(i != prot_map.end());

            // set port only if it wasn't overridden in the URL
            if (!port_set && !proxy_connection.is_unix)
                proxy_connection.port = get_port(i->second);

            // set SSL setting from protocol default
            proxy_connection.ssl = get_ssl(i->second);
        } else {
            proxy_connection.ssl = false;
            if (!port_set)
                proxy_connection.port = default_port;
        }

        setSocketPath(connection);
        return 0;
    }

    DLLLOCAL void setUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        AutoLocker al(msock->m);

        connection.setUserPassword(user, pass);
    }

    DLLLOCAL void clearUserPassword() {
        AutoLocker al(msock->m);
        connection.clearUserPassword();
    }

    DLLLOCAL void setProxyUserPassword(const char* user, const char* pass) {
        assert(user && pass);
        AutoLocker al(msock->m);

        proxy_connection.setUserPassword(user, pass);
    }

    DLLLOCAL void clearProxyUserPassword() {
        AutoLocker al(msock->m);
        proxy_connection.clearUserPassword();
    }

    DLLLOCAL bool setErrorPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = error_passthru;
        error_passthru = set;
        return rv;
    }

    DLLLOCAL bool getErrorPassthru() const {
        AutoLocker al(msock->m);
        return error_passthru;
    }

    DLLLOCAL bool setRedirectPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = redirect_passthru;
        redirect_passthru = set;
        return rv;
    }

    DLLLOCAL bool getRedirectPassthru() const {
        AutoLocker al(msock->m);
        return redirect_passthru;
    }

    DLLLOCAL bool setEncodingPassthru(bool set) {
        AutoLocker al(msock->m);
        bool rv = encoding_passthru;
        encoding_passthru = set;
        return rv;
    }

    DLLLOCAL bool getEncodingPassthru() const {
        AutoLocker al(msock->m);
        return encoding_passthru;
    }

    DLLLOCAL void setEncoding(const QoreEncoding* qe) {
        SafeLocker sl(msock->m);
        msock->socket->setEncoding(qe);
        enc = qe;
    }

    DLLLOCAL const QoreEncoding* getEncoding() const {
        SafeLocker sl(msock->m);
        if (enc) {
            return enc;
        }
        return msock->socket->getEncoding();
    }

    DLLLOCAL void addHttpMethod(const char* method, bool enable) {
        additional_methods_map.insert(method_map_t::value_type(method, enable));
    }

    // issue #2340: duplicate headers are overwritten; duplicate headers are checked with a case-insensitive search
    // the last header that matches is used for sending
    DLLLOCAL static QoreStringNode* getHeaderString(strcase_set_t& hdrs, QoreHashNode& nh, const char* key,
            ExceptionSink* xsink) {
        SimpleRefHolder<QoreStringNode> str(new QoreStringNode);
        strcase_set_t::iterator i = hdrs.find(key);
        if (i == hdrs.end()) {
            hdrs.insert(i, key);
        } else {
            //printd(5, "qore_httpclient_priv::getHeaderString() taking '%s' -> setting '%s'\n", (*i).c_str(), key);
            // remove the key
            QoreValue t = nh.takeKeyValue((*i).c_str());
            assert(!t.isNothing());
            assert(t.getType() == NT_STRING || t.getType() == NT_LIST);
            t.discard(xsink);
            assert(!*xsink);
            // replace key in set with new case if different
            if (*i != key) {
                hdrs.erase(i);
                hdrs.insert(key);
            }
        }
        nh.setKeyValue(key, *str, xsink);
        assert(!*xsink);

        return str.release();
    }

    // issue #2340: duplicate headers are overwritten; duplicate headers are checked with a case-insensitive search
    // the last header that matches is used for sending
    DLLLOCAL static QoreListNode* getHeaderList(strcase_set_t& hdrs, QoreHashNode& nh, const char* key,
            ExceptionSink* xsink) {
        ReferenceHolder<QoreListNode> l(new QoreListNode(stringTypeInfo), xsink);
        strcase_set_t::iterator i = hdrs.find(key);
        if (i == hdrs.end()) {
            hdrs.insert(i, key);
        } else {
            //printd(5, "qore_httpclient_priv::getHeaderList() taking '%s' -> setting '%s'\n", (*i).c_str(), key);
            // remove the key
            ValueHolder t(nh.takeKeyValue((*i).c_str()), xsink);
            assert(!t->isNothing());
            assert(t->getType() == NT_STRING || t->getType() == NT_LIST);
            // replace key in set with new case if different
            if (*i != key) {
                hdrs.erase(i);
                hdrs.insert(key);
            }
        }
        nh.setKeyValue(key, *l, xsink);
        assert(!*xsink);

        return l.release();
    }

    DLLLOCAL static void addAppendHeader(strcase_set_t& hdrs, QoreHashNode& nh, const char* key, const QoreValue v,
            ExceptionSink* xsink) {
        if (v.getType() == NT_LIST) {
            QoreListNode* l = getHeaderList(hdrs, nh, key, xsink);
            ConstListIterator li(v.get<const QoreListNode>());
            while (li.next()) {
                QoreStringNodeValueHelper vh(li.getValue());
                l->push(vh.getReferencedValue(), xsink);
            }
            return;
        }

        QoreStringNodeValueHelper vh(v);
        if (!vh->empty()) {
            QoreStringNode* str = getHeaderString(hdrs, nh, key, xsink);
            if (!str->empty()) {
                str->concat(',');
            }
            str->concat(*vh, xsink);
            //printd(5, "qore_httpclient_priv::addAppendHeader() %s: %s\n", key, str->c_str());
        }
    }

    DLLLOCAL QoreStringNode* getHostHeaderValue() {
        AutoLocker al(msock->m);
        return getHostHeaderValueUnlocked(connection);
    }

    // always generate a Host header pointing to the host hosting the resource, not the proxy
    // (RFC 2616 is not totally clear on this, but other clients do it this way)
    DLLLOCAL QoreStringNode* getHostHeaderValueUnlocked(const con_info& connection) {
        //printd(5, "getHostHeaderValueUnlocked() connection %s:%d\n", connection.host.c_str(), connection.port);

        // RFC 7230 section 5.5: "if the connection's incoming TCP port number
        //   differs from the default port for the effective request URI's
        //   scheme, then a colon (":") and the incoming port number (in
        //   decimal form) are appended to the authority component"
        // https://tools.ietf.org/html/rfc7230#section-5.5
        // therefore, we don't include the port number if it's the default port for the protocol
        if ((!connection.ssl && connection.port == 80) || (connection.ssl && connection.port == 443)) {
            return new QoreStringNode(connection.host.c_str());
        }

        QoreStringNodeHolder str(new QoreStringNode);
        // issue #3474: Host: headers with UNIX domain sockets must be URL encoded
        if (connection.is_unix) {
            str->concat(connection.unix_urlencoded_path);
        } else {
            str->concat(connection.host);
            str->sprintf(":%d", connection.port);
        }
        return str.release();
    }

    DLLLOCAL QoreHashNode* sendMessageAndGetResponse(con_info& connection, const char* mname,
            const char* meth, const char* mpath,
            const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
            const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
            const ResolvedCallReferenceNode* trailer_callback, QoreHashNode* info, bool with_connect, int timeout_ms,
            int& code, bool& aborted, bool path_already_encoded, ExceptionSink* xsink);

    // called locked
    DLLLOCAL const char* getMsgPath(ExceptionSink* xsink, const con_info& connection, const char* mpath,
            QoreString& pstr, bool already_encoded = false) {
        pstr.clear();

        // use default path if no path is set
        if (!mpath || !mpath[0]) {
            mpath = connection.path.empty()
                ? (default_path.empty() ? "/" : (const char*)default_path.c_str())
                : (const char*)connection.path.c_str();
        }

        if (proxy_connection.has_url() && !proxy_connected) {
            // create URL string for path for proxy
            pstr.concat("http");
            if (connection.ssl) {
                pstr.concat('s');
            }
            pstr.concat("://");
            pstr.concat(connection.host.c_str());
            if (connection.port && connection.port != 80) {
                pstr.sprintf(":%d", connection.port);
            }
            if (mpath[0] != '/') {
                pstr.concat('/');
            }
        }

        if (already_encoded) {
            pstr.concat(mpath);
        } else if (pre_encoded_urls) {
            const char* p = strchrs(mpath, must_encode_chars);
            if (p) {
                xsink->raiseException("URL-ENCODING-ERROR", "URI path '%s' contains at least one unencoded character "
                    "('%%%02X'), when the 'pre_encoded_urls' option is set, URLs must be already encoded with percent "
                    "encoding", mpath, *p);
                return nullptr;
            }
            pstr.concat(mpath);
        } else {
            // concat mpath to pstr, performing minimal URL encoding until '?'
            const char* p = mpath;
            while (*p) {
                // always encode control characters
                if ((*p) < 32) {
                    pstr.concat("%%%02X", *p);
                } else {
                    pct_encoding_map_t::const_iterator i = pct_encoding_map.find(*p);
                    if (i == pct_encoding_map.end()) {
                        pct_encoding_set_t::iterator j = local_pct_encoding_set.find(*p);
                        if (j == local_pct_encoding_set.end()) {
                            pstr.concat(*p);
                        } else {
                            QoreStringMaker tmp("%%%X", *p);
                            pstr.concat(tmp.c_str());
                        }
                    } else {
                        pstr.concat(i->second);
                    }
                }
                ++p;
            }
        }

        //printd(5, "qore_httpclient_priv::getMsgPath() cpath: '%s' dp: '%s' mpath: '%s' pstr: '%s'\n",
        //    !connection.path.empty() ? connection.path.c_str() : "",
        //    !default_path.empty() ? default_path.c_str() : "",
        //    mpath, pstr.c_str());
        return (const char*)pstr.c_str();
    }

    DLLLOCAL const char* checkMethod(ExceptionSink* xsink, const char* meth, bool& bodyp) const {
        method_map.find(meth);

        // check if method is valid
        method_map_t::const_iterator i = method_map.find(meth);
        if (i == method_map.end()) {
            i = additional_methods_map.find(meth);
            if (i == additional_methods_map.end()) {
                xsink->raiseException("HTTP-CLIENT-METHOD-ERROR", "HTTP method (%s) not recognized.", meth);
                return nullptr;
            }
        }

        // make sure the capitalized version is used
        bodyp = i->second;
        return i->first.c_str();
    }

    DLLLOCAL QoreHashNode* getRequestHeaders(ExceptionSink* xsink, const QoreHashNode* headers,
            const QoreEncoding* string_body_enc, bool msg_body, bool send_chunked, bool& keep_alive,
            bool& host_override) {
        ReferenceHolder<QoreHashNode> nh(new QoreHashNode(autoTypeInfo), xsink);
        bool transfer_encoding = false;
        // issue #1824: find content-type header, if any
        const char* ct = nullptr;
        if (headers) {
            // issue #2340 track headers in a case-insensitive way
            strcase_set_t hdrs;
            ConstHashIterator hi(headers);
            while (hi.next()) {
                // if one of the mandatory headers is found, then ignore it
                strcase_set_t::iterator si = header_ignore.find(hi.getKey());
                if (si != header_ignore.end()) {
                    continue;
                }

                // otherwise set the value in the hash
                const QoreValue n = hi.get();
                if (!n.isNothing()) {
                    if (!strcasecmp(hi.getKey(), "transfer-encoding")) {
                        transfer_encoding = true;
                    } else if (!strcasecmp(hi.getKey(), "host")) {
                        host_override = true;
                    }

                    addAppendHeader(hdrs, **nh, hi.getKey(), n, xsink);

                    if (!strcasecmp(hi.getKey(), "connection")
                        || (proxy_connection.has_url() && !strcasecmp(hi.getKey(), "proxy-connection"))) {
                        const char* conn = get_string_header(xsink, **nh, hi.getKey(), true);
                        if (*xsink) {
                            disconnect_unlocked();
                            return 0;
                        }
                        if (conn && !strcasecmp(conn, "close")) {
                            keep_alive = false;
                        }
                    } else if (!strcasecmp(hi.getKey(), "content-type")) {
                        const char* ct_value = get_string_header(xsink, **nh, hi.getKey(), true);
                        if (*xsink) {
                            disconnect_unlocked();
                            return nullptr;
                        }
                        if (ct_value && !strstr(ct_value, "charset=") && !strstr(ct_value, "boundary=")) {
                            ct = hi.getKey();
                        }
                    }
                }
            }
        }

        // add default headers if they weren't overridden
        for (auto& hdri : default_headers) {
            // look in original headers to see if the key was already given
            if (headers) {
                bool skip = false;
                ConstHashIterator hi(headers);
                while (hi.next()) {
                    if (!strcasecmp(hi.getKey(), hdri.first.c_str())) {
                        skip = true;
                        break;
                    }
                }
                if (skip) {
                    continue;
                }
            }
            const char* hdr_value = hdri.second.c_str();
            // if there is no message body then do not send the "content-type" header
            if (!strcmp(hdri.first.c_str(), "Content-Type")) {
                if (!msg_body) {
                    continue;
                }
                if (!strstr(hdr_value, "charset=") && !strstr(hdr_value, "boundary=")) {
                    ct = hdri.first.c_str();
                }
            }
            nh->setKeyValue(hdri.first.c_str(), new QoreStringNode(hdr_value), xsink);
        }

        // issue #1824: add ";charset=xxx" to Content-Type header if sending non-ISO-8891-1 text
        if (string_body_enc && ct) {
            if (!enc) {
                enc = msock->socket->getEncoding();
            }
            // any string will be converted to the socket's encoding before sending, so we have to compare the socket's
            // encoding and not the string's
            if (enc != QCS_ISO_8859_1) {
                QoreStringNode* v = nh->getKeyValue(ct).get<QoreStringNode>();
                assert(v->is_unique());
                QoreString code(string_body_enc->getCode());
                code.tolwr();
                v->sprintf(";charset=%s", code.c_str());
            }
        }

        // set Transfer-Encoding: chunked if used with a send callback
        if (send_chunked && !transfer_encoding) {
            nh->setKeyValue("Transfer-Encoding", new QoreStringNode("chunked"), xsink);
        }

        if (!connection.username.empty()) {
            // check for "Authorization" header
            bool auth_found = false;
            if (headers) {
                ConstHashIterator hi(headers);
                while (hi.next()) {
                    if (!strcasecmp(hi.getKey(), "Authorization")) {
                        auth_found = true;
                        break;
                    }
                }
            }

            if (!auth_found) {
                QoreString tmp;
                tmp.sprintf("%s:%s", connection.username.c_str(), connection.password.c_str());
                QoreStringNode* auth_str = new QoreStringNode("Basic ");
                auth_str->concatBase64(&tmp);
                nh->setKeyValue("Authorization", auth_str, xsink);
            }
        }

        return nh.release();
    }

    DLLLOCAL QoreHashNode* setProxyHeaders(ExceptionSink* xsink, const con_info& connection, QoreHashNode* headers,
            bool& use_proxy_connect, const char*& proxy_path) const {
        ReferenceHolder<QoreHashNode> proxy_headers(xsink);
        // use CONNECT if we need to make an HTTPS connection from the proxy
        if (!proxy_connection.ssl && connection.ssl) {
            use_proxy_connect = true;
            SimpleRefHolder<QoreStringNode> hostport(new QoreStringNode(connection.host));
            // RFC 7231 section 4.3.6 (https://tools.ietf.org/html/rfc7231#section-4.3.6) states
            // that the hostname and port number should be included when establishing an HTTP tunnel
            // with the CONNECT method
            if (connection.port) {
                hostport->sprintf(":%d", connection.port);
            }
            proxy_path = hostport->c_str();
            proxy_headers = new QoreHashNode(autoTypeInfo);
            proxy_headers->setKeyValue("Host", hostport.release(), xsink);

            addProxyAuthorization(headers, **proxy_headers, xsink);
        } else {
            addProxyAuthorization(headers, *headers, xsink);
        }
        return proxy_headers.release();
    }

    //! process content type header
    DLLLOCAL int processContentType(ExceptionSink* xsink, QoreHashNode& ans) {
        const QoreStringNode* v = get_string_header_node(xsink, ans, "content-type");
        if (*xsink) {
            return -1;
        }

        if (!v) {
            return 0;
        }

        // see if there is a character set specification in the content-type header
        // save original content-type header before processing
        ans.setKeyValue("_qore_orig_content_type", v->refSelf(), xsink);

        const char* str = v->c_str();
        const char* p = strstr(str, "charset=");
        if (p && (p == str || *(p - 1) == ';' || *(p - 1) == ' ')) {
            // move p to start of encoding
            const char* c = p + 8;
            char quote = '\0';
            if (*c == '\'' || *c == '"') {
                quote = *c;
                ++c;
            }
            QoreString enc;
            while (*c && *c != ';' && *c != ' ' && *c != quote) {
                enc.concat(*(c++));
            }

            if (quote && *c == quote) {
                ++c;
            }

            printd(5, "qore_httpclient_priv::processContentType() setting encoding to '%s' from content-type "
                "header: '%s' (cs: %p c: %p %d)\n", enc.c_str(), str, p + 8, c);

            // set new encoding
            msock->socket->setEncoding(QEM.findCreate(&enc));
            // strip from content-type
            QoreStringNode* nc = new QoreStringNode;
            // skip any spaces before the charset=
            while (p != str && (*(p - 1) == ' ' || *(p - 1) == ';')) {
                p--;
            }
            if (p != str) {
                nc->concat(str, p - str);
            }
            if (*c) {
                nc->concat(c);
            }
            ans.setKeyValue("content-type", nc, xsink);
            str = nc->c_str();
        }
        // split into a list if ";" characters are present
        p = strchr(str, ';');
        if (p) {
            bool multipart = false;
            QoreListNode* l = new QoreListNode(stringTypeInfo);
            do {
                // skip whitespace
                while (*str == ' ') str++;
                if (str != p) {
                    int len = p - str;
                    check_headers(str, len, multipart, ans, msock->socket->getEncoding(), xsink);
                    l->push(new QoreStringNode(str, len, msock->socket->getEncoding()), xsink);
                }
                str = p + 1;
            } while ((p = strchr(str, ';')));
            // skip whitespace
            while (*str == ' ') ++str;
            // add last field
            if (*str) {
                check_headers(str, strlen(str), multipart, ans, msock->socket->getEncoding(), xsink);
                l->push(new QoreStringNode(str, msock->socket->getEncoding()), xsink);
            }
            ans.setKeyValue("content-type", l, xsink);
        }
        return 0;
    }

    DLLLOCAL const char* normalizeContentEncoding(ExceptionSink* xsink, QoreHashNode& ans, bool recv_callback,
            qore_uncompress_to_string_t& dec) {
        const char* content_encoding = get_string_header(xsink, ans, "content-encoding");
        if (*xsink) {
            return nullptr;
        }

        if (content_encoding) {
            const char* start = content_encoding;
            while (*start == ' ' || *start == '\t') {
                ++start;
            }
            const char* end = start;
            while (*end && *end != ',' && *end != ';' && *end != ' ') {
                ++end;
            }
            QoreString token;
            if (end > start) {
                token.concat(start, end - start);
            }

            // check for misuse of this header by including a character encoding value
            if (!token.empty() && (!strncasecmp(token.c_str(), "iso", 3) || !strncasecmp(token.c_str(), "utf-", 4))) {
                msock->socket->setEncoding(QEM.findCreate(token.c_str()));
                content_encoding = nullptr;
            } else if (!encoding_passthru && !token.empty()) {
                bool ignore_encoding = false;
                dec = get_decoder_for_content_encoding(token.c_str(), ignore_encoding);
                if (ignore_encoding) {
                    content_encoding = nullptr;
                } else if (!dec) {
                    // issue #2953 ignore unknown content encodings or a crash will result
                    content_encoding = nullptr;
                }
            }
        }

        return content_encoding;
    }

    DLLLOCAL QoreHashNode* send_internal(ExceptionSink* xsink, const char* mname, const char* meth, const char* mpath,
        const QoreHashNode* headers, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback = nullptr, QoreObject* obj = nullptr,
        OutputStream* os = nullptr, InputStream* is = nullptr, size_t max_chunk_size = 0,
        const ResolvedCallReferenceNode* trailer_callback = nullptr, bool streaming = false);

    //! Send a request and get response using HTTP/2
    DLLLOCAL QoreHashNode* sendHttp2MessageAndGetResponse(const char* mname, const char* meth, const char* mpath,
        const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback,
        QoreHashNode* info, int timeout_ms, int& code, bool& aborted, ExceptionSink* xsink);

    // Parse Alt-Svc header value and update the cache
    DLLLOCAL void parseAltSvc(const char* value, const char* host, int port);

    // Look up Alt-Svc entry for a given origin; returns nullptr if not found or expired
    DLLLOCAL std::optional<AltSvcEntry> lookupAltSvc(const char* host, int port);

    //! Remove Alt-Svc entry (used on QUIC disconnect for clean slate)
    DLLLOCAL void removeAltSvc(const char* host, int port) {
        std::string origin = std::string(host) + ":" + std::to_string(port);
        alt_svc_cache.erase(origin);
    }

    //! Mark Alt-Svc entry as failed with a backoff period
    /** Prevents repeated QUIC upgrade attempts to a broken endpoint.
        The entry remains in the cache but lookupAltSvc() will skip it
        until the backoff expires.  New Alt-Svc headers from the server
        do not reset the backoff — only expiry does.
    */
    DLLLOCAL void markAltSvcFailed(const char* host, int port) {
        std::string origin = std::string(host) + ":" + std::to_string(port);
        auto it = alt_svc_cache.find(origin);
        if (it != alt_svc_cache.end()) {
            // 5-minute backoff before retrying QUIC for this origin
            it->second.retry_after_epoch = q_epoch() + 300;
        }
    }

    // Establish a QUIC/HTTP3 connection to the given server
    DLLLOCAL int connectQuic(ExceptionSink* xsink, con_info& connection);

    // Send an HTTP/3 request and get the response
    DLLLOCAL QoreHashNode* sendHttp3MessageAndGetResponse(const char* mname, const char* meth, const char* mpath,
        const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback,
        QoreHashNode* info, int timeout_ms, int& code, ExceptionSink* xsink);

    DLLLOCAL void addProxyAuthorization(const QoreHashNode* headers, QoreHashNode& h, ExceptionSink* xsink) const {
        if (proxy_connection.username.empty())
            return;

        QoreValue pauth{};
        // check for "Proxy-Authorization" header
        if (headers) {
            ConstHashIterator hi(headers);
            while (hi.next()) {
                if (!strcasecmp(hi.getKey(), "Proxy-Authorization")) {
                    pauth = hi.getReferenced();
                    h.setKeyValue("Proxy-Authorization", pauth, xsink);
                    assert(!*xsink);
                    break;
                }
            }
        }

        if (!pauth) {
            QoreString tmp;
            tmp.sprintf("%s:%s", proxy_connection.username.c_str(), proxy_connection.password.c_str());
            QoreStringNode* auth_str = new QoreStringNode("Basic ");
            auth_str->concatBase64(&tmp);
            h.setKeyValue("Proxy-Authorization", auth_str, xsink);
            assert(!*xsink);
        }
    }

    int getDiscardMessageBody(ExceptionSink* xsink, QoreHashNode& ans, int timeout_ms) {
        const char* te = get_string_header(xsink, ans, "transfer-encoding");
        if (*xsink) {
            return -1;
        }

        // get response body, if any
        const char* cl = get_string_header(xsink, ans, "content-length");
        if (*xsink) {
            return -1;
        }

        int len = cl ? atoi(cl) : 0;
        Queue* event_queue = msock->socket->getQueue();
        // do not try to get a body in any case if Content-Length: 0 is sent
        if (cl && event_queue) {
            do_content_length_event(event_queue, msock->socket->priv, len);
        }

        if (te && !strcasecmp(te, "chunked")) { // check for chunked response body
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_START);
            ReferenceHolder<QoreHashNode> nah(msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink,
                "HTTPClient", QORE_SOURCE_HTTPCLIENT), xsink);
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_END);
            if (*xsink) {
                return -1;
            }
            return 0;
        }

        if (len) {
            qore_offset_t rc;
            QoreStringNodeHolder bstr(msock->socket->priv->recv(xsink, len, timeout_ms, rc,
                    QORE_SOURCE_HTTPCLIENT));
            if (*xsink) {
                return -1;
            }
        }

        return 0;
    }

    // Set additional characters to encode in the URI path
    DLLLOCAL void setEncodeChar(const char c) {
        pct_encoding_map_t::iterator i = pct_encoding_map.find(c);
        // ignore if already in the global map
        if (i == pct_encoding_map.end()) {
            pct_encoding_set_t::iterator j = local_pct_encoding_set.find(c);
            // ignore if already in the local set
            if (j == local_pct_encoding_set.end()) {
                local_pct_encoding_set.insert(j, c);
            }
        }
    }

    DLLLOCAL static qore_httpclient_priv* get(QoreHttpClientObject& client) {
        return client.http_priv;
    }

    DLLLOCAL static const qore_httpclient_priv* get(const QoreHttpClientObject& client) {
        return client.http_priv;
    }
};

// setup default headers
// Brotli, Zstd, and LZ4 are required since Qore 2.3
#define QORE_HTTP_ACCEPT_ENCODING "br,zstd,deflate,gzip,bzip2"

header_map_t qore_httpclient_priv::static_default_headers = {
    {"Accept", "text/html"},
    {"Content-Type", "text/html"},
    {"Connection", "Keep-Alive"},
    {"User-Agent", "Qore-HTTP-Client/" PACKAGE_VERSION},
    {"Accept-Encoding", QORE_HTTP_ACCEPT_ENCODING},
};

// RFC-1738: encode space, <, >, ", #, %, {, }, |, \, ^, ~, [, ], `
qore_httpclient_priv::pct_encoding_map_t qore_httpclient_priv::pct_encoding_map = {
    {' ', "%20"},
    {'<', "%3C"},
    {'>', "%3E"},
    {'"', "%22"},
    {'#', "%23"},
    {'%', "%25"},
    {'{', "%7B"},
    {'}', "%7D"},
    {'|', "%7C"},
    {'\\', "%5C"},
    {'^', "%5E"},
    {'~', "%7E"},
    {'[', "%5B"},
    {']', "%5D"},
    {'`', "%60"},
};

constexpr int HS_NONE = 0;
constexpr int HS_R = 1;
constexpr int HS_RN = 2;
constexpr int HS_RNR = 3;

class HttpClientRecvHeaderPollState : public AbstractPollState {
public:
    DLLLOCAL HttpClientRecvHeaderPollState(ExceptionSink* xsink, qore_httpclient_priv* http,
            bool exit_early = false) : http(http), hdr(
                new QoreStringNode(http->msock->socket->priv->enc ? http->msock->socket->priv->enc : http->enc)
            ), exit_early(exit_early) {
        assert(http->msock->m.trylock());
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
#ifdef DEBUG
        qore_socket_private* spriv = http->msock->socket->priv;
        assert(http->msock->m.trylock());
        assert(spriv->isOpen());
#endif

        return readHeaderIntern(xsink);
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        return hdr.release();
    }

protected:
    qore_httpclient_priv* http;
    QoreStringNodeHolder hdr;
    int state = HS_NONE;
    bool exit_early;

    // returns -1 = err, 0 = data available in buffer, 1 = wait read, 2 = wait write
    DLLLOCAL int doRecv(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;
        OptionalNonBlockingHelper nbh(*spriv, true, xsink);
        if (*xsink) {
            return -1;
        }

        while (true) {
            ssize_t rc;
            if (spriv->ssl) {
                size_t real_io = 0;
                rc = spriv->ssl->doNonBlockingIo(xsink, "read", spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, SslAction::READ,
                    real_io);
                if (*xsink) {
                    return -1;
                }
                if (!rc) {
                    assert(!spriv->bufoffset);
                    spriv->buflen = real_io;
                }
                assert(!rc || rc == 1 || rc == 2 || rc == 3 || rc == -1);
                return rc;
            } else {
                rc = ::recv(spriv->sock, spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, 0);
                if (!rc) {
                    return 0;
                }
                if (rc > 0) {
                    assert(!spriv->bufoffset);
                    spriv->buflen = rc;
                    break;
                }
                sock_get_error();
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN
#ifdef EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                ) {
                    return SOCK_POLLIN;
                }
                xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
                return -1;
            }
        }
        return 0;
    }

    DLLLOCAL int readHeaderIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;
        while (true) {
            char c;
            if (spriv->readByteFromBuffer(c)) {
                int rc = doRecv(xsink);
                if (!rc) {
                    if (!spriv->buflen) {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "header (read %zu byte%s; '%s')", hdr->size(), hdr->size() == 1 ? "" : "s", hdr->c_str());
                        return -1;
                    }
                    continue;
                }
                if (*xsink) {
                    return -1;
                }
                return rc;
            }

            // check if we can progress to the next state
            if (c == '\n') {
                if (state == HS_R) {
                    if (exit_early && hdr->empty()) {
                        return 0;
                    }
                    state = HS_RN;
                    continue;
                }
                if (state == HS_RNR) {
                    break;
                }
            } else if (c == '\r') {
                if (state == HS_NONE) {
                    state = HS_R;
                    continue;
                }
                if (state == HS_RN) {
                    state = HS_RNR;
                    continue;
                }
            }

            if (state != HS_NONE) {
                switch (state) {
                    case HS_R: hdr->concat("\r"); break;
                    case HS_RN: hdr->concat("\r\n"); break;
                    case HS_RNR: hdr->concat("\r\n\r"); break;
                }
                state = HS_NONE;
            }
            hdr->concat(c);
            if (hdr->size() >= QORE_MAX_HEADER_SIZE) {
                xsink->raiseException("SOCKET-HTTP-ERROR", "header size cannot exceed %zu bytes", hdr->size());
                return -1;
            }
        }
        hdr->concat('\n');

        //printd(5, "HttpClientRecvHeaderPollState::readHeaderIntern() read hdr: '%s'\n", hdr->c_str());

        return 0;
    }
};

constexpr int PSC_READING_SIZE = 0;
constexpr int PSC_READING_CHUNK = 1;
constexpr int PSC_READING_CHUNK_EOR = 2;
constexpr int PSC_READING_TRAILERS = 3;

class HttpClientRecvChunkedPollState : public HttpClientRecvHeaderPollState {
public:
    DLLLOCAL HttpClientRecvChunkedPollState(ExceptionSink* xsink, qore_httpclient_priv* http)
            : HttpClientRecvHeaderPollState(xsink, http, true), chunked_body(new BinaryNode) {
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
#ifdef DEBUG
        qore_socket_private* spriv = http->msock->socket->priv;
        assert(http->msock->m.trylock());
        assert(spriv->isOpen());
#endif

        int rc;
        while (true) {
            //printd(5, "HttpClientRecvChunkedPollState::continuePoll() pstate: %d\n", pstate);
            if (pstate == PSC_READING_SIZE) {
                rc = readSizeIntern(xsink);
                //printd(5, "rc: %d *xsink: %d chunk size: %ld\n", rc, (bool)*xsink, chunk_size);
            } else if (pstate == PSC_READING_CHUNK) {
                rc = readChunkIntern(xsink);
            } else if (pstate == PSC_READING_CHUNK_EOR) {
                rc = readChunkEorIntern(xsink);
            } else if (pstate == PSC_READING_TRAILERS) {
                hdr->clear();
                rc = readHeaderIntern(xsink);
                //printd(5, "rc: %d (*xsink: %d) trailer: '%s' last chunk size: %ld\n", rc, (bool)*xsink,
                //    hdr->c_str(), chunk_size);
                if (!rc) {
                    if (chunk_size || !hdr->empty()) {
                        pstate = PSC_READING_SIZE;
                        hdr->clear();
                    } else {
                        break;
                    }
                }
            } else {
                assert(false);
            }
            if (rc) {
                break;
            }
        }

        return rc;
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        return chunked_body.release();
    }

private:
    int pstate = PSC_READING_SIZE;
    SimpleRefHolder<BinaryNode> chunked_body;
    size_t chunk_size = 0;
    size_t chunk_received = 0;
    // read \r while reading size
    bool got_r = false;

    DLLLOCAL int readSizeIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;

        while (true) {
            char c;
            if (spriv->readByteFromBuffer(c)) {
                int rc = doRecv(xsink);
                if (!rc) {
                    if (!spriv->buflen) {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "chunk");
                        return -1;
                    }
                    continue;
                }
                if (*xsink) {
                    //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() doRecv() return -1\n");
                    return -1;
                }
                return rc;
            }

            if (c == '\r') {
                if (got_r) {
                    xsink->raiseException("READ-HTTP-CHUNK-ERROR", "unexpected \\r character found in chunked input "
                        "while reading chunk size");
                    return -1;
                }
                got_r = true;
                continue;
            }
            if (c == '\n') {
                if (got_r) {
                    if (!hdr) {
                        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "chunk has no size");
                        return -1;
                    }

                    //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() hdr: '%s'\n", hdr->c_str());

                    // terminate string at ';' char if present
                    ssize_t i = hdr->find(';');
                    if (i >= 0) {
                        hdr->terminate(i);
                    }
                    chunk_size = strtoll(hdr->c_str(), nullptr, 16);
                    spriv->do_chunked_read(QORE_EVENT_HTTP_CHUNK_SIZE, chunk_size, hdr->size(),
                        QORE_SOURCE_HTTPCLIENT);
                    hdr->clear();
                    if (!chunk_size) {
                        pstate = PSC_READING_TRAILERS;
                    } else if (chunk_size < 0) {
                        xsink->raiseException("READ-HTTP-CHUNK-ERROR", "negative value given for chunk size (%ld)",
                            chunk_size);
                        return -1;
                    } else {
                        chunk_received = 0;
                        pstate = PSC_READING_CHUNK;
                    }
                    break;
                }
            }
            if (got_r) {
                xsink->raiseException("READ-HTTP-CHUNK-ERROR", "unexpected character (ASCII %d) found in chunked "
                    "input after \\r character while reading chunk size", (int)c);
                return -1;
            }
            hdr->concat(c);
        }
        return 0;
    }

    DLLLOCAL int readChunkIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;

        int rc;
        while (true) {
            size_t chunk_needed = chunk_size - chunk_received;

            // first take any data in the socket buffer
            if (spriv->buflen) {
                if (spriv->buflen <= chunk_needed) {
                    chunked_body->append(spriv->rbuf + spriv->bufoffset, spriv->buflen);
                    chunk_received += spriv->buflen;
                    chunk_needed -= spriv->buflen;
                    spriv->buflen = 0;
                    spriv->bufoffset = 0;
                } else {
                    chunked_body->append(spriv->rbuf + spriv->bufoffset, chunk_needed);
                    chunk_received += chunk_needed;
                    spriv->buflen -= chunk_needed;
                    spriv->bufoffset += chunk_needed;
                }

                if (chunk_received == chunk_size) {
                    pstate = PSC_READING_CHUNK_EOR;
                    got_r = false;
                    //printd(5, "HttpClientRecvChunkedPollState::readChunkIntern() chunk: %ld received: %ld "
                    //    "needed: %ld total read: %ld\n", chunk_size, chunk_received, chunk_needed,
                    //    chunked_body->size());
                    return 0;
                }
            }

            rc = doRecv(xsink);
            if (!rc) {
                if (!spriv->buflen) {
                    xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                        "chunk data");
                    return -1;
                }
                continue;
            }
            if (*xsink) {
                return -1;
            }
            break;
        }

        return rc;
    }

    DLLLOCAL int readChunkEorIntern(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;

        while (true) {
            char c;
            if (spriv->readByteFromBuffer(c)) {
                int rc = doRecv(xsink);
                if (!rc) {
                    if (!spriv->buflen) {
                        xsink->raiseException("SOCKET-HTTP-ERROR", "remote end closed connection while reading "
                            "chunk data trailing bytes");
                        return -1;
                    }
                    continue;
                }
                if (*xsink) {
                    //printd(5, "HttpClientRecvChunkedPollState::readSizeIntern() doRecv() return -1\n");
                    return -1;
                }
                return rc;
            }

            if (c == '\r' && !got_r) {
                got_r = true;
                continue;
            } else if (c == '\n' && got_r) {
                got_r = false;
                pstate = PSC_READING_SIZE;
                break;
            }

            xsink->raiseException("READ-HTTP-CHUNK-ERROR", "unexpected character with ASCII %d found in chunked "
                "input while end of chunk data", (int)c);
            return -1;
        }
        return 0;
    }
};

class HttpClientRecvUntilClosePollState : public AbstractPollState {
public:
    DLLLOCAL HttpClientRecvUntilClosePollState(ExceptionSink* xsink, qore_httpclient_priv* http) : http(http),
            body(new BinaryNode) {
        assert(http->msock->m.trylock());
    }

    /** returns:
        - SOCK_POLLIN = wait for read and call this again
        - SOCK_POLLOUT = wait for write and call this again
        - 0 = done
        - < 0 = error (exception raised)
    */
    DLLLOCAL virtual int continuePoll(ExceptionSink* xsink) {
        qore_socket_private* spriv = http->msock->socket->priv;
        assert(http->msock->m.trylock());

        assert(spriv->isOpen());

        OptionalNonBlockingHelper nbh(*spriv, true, xsink);
        if (*xsink) {
            return -1;
        }

        // first take any data in the socket buffer
        if (spriv->buflen) {
            body->append(spriv->rbuf + spriv->bufoffset, spriv->buflen);
            spriv->buflen = 0;
            spriv->bufoffset = 0;
        }
        // socket buffer must be empty
        assert(!spriv->buflen);
        assert(!spriv->bufoffset);

        while (true) {
            ssize_t rc;
            if (spriv->ssl) {
                size_t real_io = 0;
                rc = spriv->ssl->doNonBlockingIo(xsink, "read", spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, SslAction::READ,
                    real_io);
                if (*xsink) {
                    return -1;
                }
                if (!rc) {
                    if (!real_io) {
                        break;
                    }
                    body->append(spriv->rbuf, real_io);
                    continue;
                }
                assert(!rc || rc == 1 || rc == 2 || rc == 3 || rc == -1);
                return rc;
            } else {
                rc = ::recv(spriv->sock, spriv->rbuf, DEFAULT_SOCKET_BUFSIZE, 0);
                assert(rc);
                if (rc > 0) {
                    body->append(spriv->rbuf, rc);
                    continue;
                }
                if (!rc) {
                    break;
                }
                sock_get_error();
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN
#ifdef EWOULDBLOCK
                    || errno == EWOULDBLOCK
#endif
                ) {
                    return SOCK_POLLIN;
                }
                xsink->raiseErrnoException("SOCKET-RECV-ERROR", errno, "error while executing Socket::recv()");
                return -1;
            }
        }
        return 0;
    }

    //! Returns the data read
    DLLLOCAL virtual QoreValue takeOutput() {
        return body.release();
    }

protected:
    qore_httpclient_priv* http;
    SimpleRefHolder<BinaryNode> body;
};

// states: none [-> connecting [-> connecting-ssl]] -> sending -> receiving-header [-> receiving-body] [-> connecting-proxy-ssl] -> [received | connected]
/**
    state transitions:
    - none
      -> connecting
         -> connecting-ssl
            -> sending...
      -> sending
         -> receiving-header
            -> receiving->body
               -> received
            -> connecting...
            -> sending...
            -> connecting->proxy-ssl
               -> sending...
*/
HttpClientConnectSendRecvPollOperation::HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink,
        QoreHttpClientObject* client) : client(client), request_headers(nullptr), proxy_headers(nullptr),
        info(new QoreHashNode(autoTypeInfo), nullptr), response_headers(nullptr) {
    connect_only = true;

    SafeLocker sl(client->priv->m);

    if (client->priv->setNonBlock(xsink)) {
        assert(*xsink);
        return;
    }
    set_non_block = true;

    connection = client->http_priv->connection;

    if (client->http_priv->msock->socket->isOpen()) {
        client->http_priv->disconnect_unlocked();
    }

    if (client->http_priv->proxy_connection.has_url()) {
        proxy_headers = client->http_priv->setProxyHeaders(xsink, connection, *request_headers, use_proxy_connect,
            proxy_path);
        if (proxy_headers) {
            assert(use_proxy_connect);
            assert(proxy_path);
        }
    }

    // NOTE: this will always connect, as we closed the socket above
    connectOrSend(xsink);
}

HttpClientConnectSendRecvPollOperation::HttpClientConnectSendRecvPollOperation(ExceptionSink* xsink, QoreHttpClientObject* client,
        std::string method, std::string path, const void* data, size_t size, const QoreHashNode* headers,
        const QoreEncoding* enc) : client(client), method(method), path(path), data(size && data ? data : nullptr),
        size(size), request_headers(nullptr), proxy_headers(nullptr), info(new QoreHashNode(autoTypeInfo), nullptr),
        response_headers(nullptr) {
    connect_only = false;
    {
        const char* m = client->http_priv->checkMethod(xsink, method.c_str(), bodyp);
        if (*xsink) {
            return;
        }
        method = m;
    }

    SafeLocker sl(client->priv->m);

    if (client->priv->setNonBlock(xsink)) {
        assert(*xsink);
        return;
    }
    set_non_block = true;

    connection = client->http_priv->connection;

    request_headers = client->http_priv->getRequestHeaders(xsink, headers, enc, data && size, false, keep_alive,
        host_override);
    if (*xsink) {
        return;
    }

    if (!client->http_priv->proxy_connected && client->http_priv->proxy_connection.has_url()) {
        proxy_headers = client->http_priv->setProxyHeaders(xsink, connection, *request_headers, use_proxy_connect,
            proxy_path);
        if (proxy_headers) {
            assert(use_proxy_connect);
            assert(proxy_path);
        }
    }
    printd(5, "HttpClientConnectSendRecvPollOperation::HttpClientConnectSendRecvPollOperation() this: %p "
        "proxy connected: %d proxy: %d use_proxy_connect: %d proxy_path: %s\n", this, client->http_priv->proxy_connected,
        client->http_priv->proxy_connection.has_url(), use_proxy_connect, proxy_path ? proxy_path : "n/a");

    connectOrSend(xsink);
}

void HttpClientConnectSendRecvPollOperation::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        if (set_non_block) {
            client->clearNonBlock();
        }
        if (info) {
            info.release()->deref(xsink);
        }
        client->deref(xsink);
        delete this;
    }
}

QoreValue HttpClientConnectSendRecvPollOperation::getOutput() const {
    ReferenceHolder<QoreHashNode> rv(nullptr);
    if (http_response_code != -1) {
        rv = new QoreHashNode(autoTypeInfo);
        rv->setKeyValue("code", http_response_code, nullptr);
    }
    if (!info->empty()) {
        if (!rv) {
            rv = new QoreHashNode(autoTypeInfo);
        }
        rv->setKeyValue("info", info->hashRefSelf(), nullptr);
    }
    if (request_headers) {
        if (!rv) {
            rv = new QoreHashNode(autoTypeInfo);
        }
        rv->setKeyValue("request-headers", request_headers->refSelf(), nullptr);
    }
    if (recv_data_holder) {
        if (!rv) {
            rv = new QoreHashNode(autoTypeInfo);
        }
        rv->setKeyValue("response-body", recv_data_holder->refSelf(), nullptr);
    }
    return rv.release();
}

void HttpClientConnectSendRecvPollOperation::abort(ExceptionSink* xsink) {
    if (set_non_block) {
        set_non_block = false;
        AutoLocker al(client->priv->m);
        poll_state.reset();  // cleanup poll state while fds still valid
        client->priv->clearNonBlock();
        client->http_priv->disconnect_unlocked();
        state = SPS_NONE;
    }
}

// states: none [-> connecting [-> connecting-ssl]] -> sending -> receiving-header [-> receiving-body] [-> connecting-proxy-ssl] -> [received | connected]
/**
    state transitions:
    - none
      -> connecting
         -> connecting-ssl
            -> sending...
      -> sending
         -> receiving-header
            -> receiving->body
               -> received
            -> connecting...
            -> sending...
            -> connecting->proxy-ssl
               -> sending...
*/
QoreHashNode* HttpClientConnectSendRecvPollOperation::continuePoll(ExceptionSink* xsink) {
    QoreHashNode* rv = nullptr;

    SafeLocker sl(client->priv->m);

    if (state == SPS_NONE || state == SPS_RECEIVED) {
        // throw an exception and exit if the object is no longer valid
        if (client->priv->checkValid(xsink)) {
            return nullptr;
        }
    } else if (state == SPS_H3_ACTIVE) {
        // HTTP/3: TCP socket is closed but QUIC session is alive; only check validity
        if (client->priv->checkValid(xsink)) {
            return nullptr;
        }
    } else {
        // throw an exception and exit if the object is no longer open or valid
        if (client->priv->checkOpen(xsink)) {
            return nullptr;
        }
    }

    while (true) {
        //printd(5, "HttpClientConnectSendRecvPollOperation::continuePoll() this: %p %s xsink: %d poll_state: %d "
        //    "connection: %s:%d\n", this, getStateImpl(), (bool)*xsink, (bool)poll_state, connection.host.c_str(),
        //    connection.port);

        if (state == SPS_CONNECTING) {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);

            if (connectDone(xsink)) {
                break;
            }
            continue;
        } else if (state == SPS_CONNECTING_SSL) {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);

            if (startSend(xsink)) {
                break;
            }
            continue;
       } else if (state == SPS_CONNECTING_PROXY_SSL) {
            assert(!client->http_priv->proxy_connected);
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);
            client->http_priv->proxy_connected = true;
            // remove "Proxy-Authorization" header
            if (request_headers) {
                if (!request_headers->is_unique()) {
                    request_headers = request_headers->copy();
                }
                request_headers->removeKey("Proxy-Authorization", xsink);
                assert(!*xsink);
            }
            assert(!use_proxy_connect);
            assert(!proxy_path);
            assert(client->http_priv->proxy_connected);
            send_data_holder = nullptr;

            if (startSend(xsink)) {
                break;
            }

            continue;
        } else if (state == SPS_SENDING) {
            int rc = checkContinuePoll(xsink);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(!poll_state);
            poll_state.reset(new HttpClientRecvHeaderPollState(xsink, client->http_priv));
            if (*xsink) {
                break;
            }
            state = SPS_RECEIVING_HEADER;
            continue;
        } else if (state == SPS_RECEIVING_HEADER) {
            // do not delete poll state when complete
            int rc = checkContinuePoll(xsink, true);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(poll_state);

            if (processResponse(xsink)) {
                break;
            }

            continue;
        } else if (state == SPS_RECEIVING_BODY) {
            // do not delete poll state when complete
            int rc = checkContinuePoll(xsink, true);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            assert(poll_state);

            if (processReceivedBody(xsink)) {
                break;
            }

            continue;
        } else if (state == SPS_H2_ACTIVE) {
            // HTTP/2 bidirectional send/receive
            int rc = checkContinuePoll(xsink, true);
            if (rc != 0) {
                rv = *xsink ? nullptr : getSocketPollInfoHash(xsink, rc);
                break;
            }
            // Poll completed - process HTTP/2 response
            if (processH2Response(xsink)) {
                break;
            }
            continue;
        } else if (state == SPS_H3_ACTIVE) {
            // HTTP/3 bidirectional send/receive via QUIC
            int rc = checkContinuePoll(xsink, true);
            if (rc != 0) {
                ReferenceHolder<QoreHashNode> h3_rv(*xsink ? nullptr
                    : getSocketPollInfoHash(xsink, rc), xsink);
                // Set QUIC-aware poll timeout so retransmission timers fire
                // even when no packets arrive
                if (h3_rv) {
                    Http3ClientPollState* h3_poll
                        = static_cast<Http3ClientPollState*>(poll_state.get());
                    ngtcp2_tstamp expiry = h3_poll->getLastExpiry();
                    if (expiry != UINT64_MAX) {
                        ngtcp2_tstamp now = QuicSession::timestamp();
                        int64_t timeout_ms;
                        if (expiry <= now) {
                            timeout_ms = 1;
                        } else {
                            timeout_ms = static_cast<int64_t>((expiry - now) / 1000000);
                            if (timeout_ms < 1) {
                                timeout_ms = 1;
                            }
                        }
                        h3_rv->setKeyValue("poll_timeout_ms", timeout_ms, xsink);
                    }
                }
                rv = *xsink ? nullptr : h3_rv.release();
                break;
            }
            // Poll completed - process HTTP/3 response
            if (processH3Response(xsink)) {
                break;
            }
            continue;
        } else if (state == SPS_RECEIVED || state == SPS_CONNECTED) {
            if (close_connection) {
                client->http_priv->disconnect_unlocked();
            }
            break;
        } else if (state == SPS_NONE) {
            // operation canceled
            break;
        } else {
            assert(false);
        }
        break;
    }

    if (*xsink) {
        assert(!rv);
        if (poll_state) {
            poll_state.reset();
        }
        client->priv->clearNonBlock();
        set_non_block = false;
        state = SPS_NONE;
    }

    return rv;
}

int HttpClientConnectSendRecvPollOperation::connectOrSend(ExceptionSink* xsink) {
    //printd(5, "HttpClientConnectSendRecvPollOperation::connectOrSend() this: %p connection: %s:%d\n", this,
    //    connection.host.c_str(), connection.port);

    assert(client->http_priv->msock->m.trylock());

    // HTTP/3: QUIC session is already established; TCP socket is closed but QUIC fd is live
    if (client->http_priv->http3_active && client->http_priv->quic_session) {
        if (connect_only) {
            setFinal(SPS_CONNECTED);
            return 0;
        }
        return startSend(xsink) ? -1 : 0;
    }

    if (!client->http_priv->msock->socket->isOpen()) {
        poll_state.reset(client->http_priv->msock->socket->startConnect(xsink,
            client->http_priv->socketpath.c_str()));
        if (*xsink) {
            return -1;
        }
        if (poll_state) {
            state = SPS_CONNECTING;
            return 0;
        }
        if (connectDone(xsink)) {
            return -1;
        }
        return 0;
    }

    if (startSend(xsink)) {
        return -1;
    }
    return 0;
}

int HttpClientConnectSendRecvPollOperation::processResponse(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());
    assert(poll_state);

    // get response string
    QoreStringNodeHolder rhdr(poll_state->takeOutput().get<QoreStringNode>());
    poll_state.reset();
    qore_socket_private* spriv = client->http_priv->msock->socket->priv;
    response_headers = spriv->processHttpHeaderString(xsink, rhdr, *info, QORE_SOURCE_HTTPCLIENT);
    if (*xsink) {
        return -1;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() this: %p got: '%s'\n", this,
    //    rhdr->c_str());

    // check HTTP status code
    QoreValue n = response_headers->getKeyValue("status_code");
    if (n.isNothing()) {
        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "no HTTP status code received in response");
        return -1;
    }

    http_response_code = (int)n.getAsBigInt();
    // continue processing if "100 Continue" response received (ignore this response)
    if (http_response_code == 100) {
        recv_data_holder = nullptr;
        return startSend(xsink);
    }

    info->setKeyValue("response-headers", response_headers->refSelf(), xsink);
    assert(!*xsink);

    if (!response_headers->is_unique()) {
        response_headers = response_headers->copy();
    }

    // process content-type
    if (client->http_priv->processContentType(xsink, **response_headers)) {
        return -1;
    }

    // do not read any message body for messages that cannot have one
    // rfc 2616 4.4 p1 (http://tools.ietf.org/html/rfc2616#section-4.4)
    /*
        1.Any response message which "MUST NOT" include a message-body (such
        as the 1xx, 204, and 304 responses and any response to a HEAD
        request) is always terminated by the first empty line after the
        header fields, regardless of the entity-header fields present in
        the message.
    */

    // do not process message body if no message body can be sent
    if (!bodyp || (http_response_code >= 100 && http_response_code < 200) || (http_response_code == 204)
        || (http_response_code == 304)) {
        return responseDone(xsink);
    }

    // process message body

    // get Connection header
    const char* conn = get_string_header(xsink, **response_headers, "connection", true);
    if (*xsink) {
        return -1;
    }
    close_connection = conn && !strcasecmp(conn, "close");

    // get Transfer-Encoding header
    bool chunked = false;
    const char* transfer_encoding = get_string_header(xsink, **response_headers, "transfer-encoding");
    if (*xsink) {
        return -1;
    }
    if (transfer_encoding) {
        if (!strcasecmp(transfer_encoding, "chunked")) {
            chunked = true;
        } else {
            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "cannot handle Transfer-Encoding: %s; expecting "
                "'chunked'", transfer_encoding);
            return -1;
        }
    }

    // get response body, if any
    const char* content_length = get_string_header(xsink, **response_headers, "content-length");
    if (*xsink) {
        return -1;
    }
    ssize_t len = content_length ? strtoll(content_length, nullptr, 10) : 0;

    // issue #3691: read the body until the socket is closed if we have Connection: close
    if (!chunked && close_connection && !content_length) {
        poll_state.reset(new HttpClientRecvUntilClosePollState(xsink, client->http_priv));
        state = SPS_RECEIVING_BODY;
        //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() created "
        //    "HttpClientRecvUntilClosePollState\n", len);
        return 0;
    }

    if (content_length) {
        Queue* event_queue = client->priv->socket->getQueue();
        if (event_queue) {
            do_content_length_event(event_queue, spriv, len);
        }
    }

    if (chunked) {
        poll_state.reset(new HttpClientRecvChunkedPollState(xsink, client->http_priv));
        state = SPS_RECEIVING_BODY;
        //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() created "
        //    "HttpClientRecvChunkedPollState (Transfer-Encoding: %s Content-Length: %ld)\n", transfer_encoding, len);
        return 0;
    }

    if (len > 0) {
        poll_state.reset(new SocketRecvPollState(xsink, spriv, len));
        state = SPS_RECEIVING_BODY;
        //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() created SocketRecvPollState "
        //    "(Content-Length: %ld)\n", len);
        return 0;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::processResponse() this: %p calling responseDone()\n", this);
    return responseDone(xsink);
}

int HttpClientConnectSendRecvPollOperation::processReceivedBody(ExceptionSink* xsink) {
    recv_data_holder = poll_state->takeOutput().get<BinaryNode>();
    poll_state.reset();

    const char* content_encoding = get_string_header(xsink, **response_headers, "content-encoding");
    if (*xsink) {
        return -1;
    }

    if (content_encoding) {
        // check for misuse of this header by including a character encoding value
        if (!strncasecmp(content_encoding, "iso", 3) || !strncasecmp(content_encoding, "utf-", 4)) {
            client->http_priv->msock->socket->setEncoding(QEM.findCreate(content_encoding));
        } else {
            qore_uncompress_to_binary_t dec = nullptr;
            // only decode message bodies automatically if there is no receive callback
            if (!strcasecmp(content_encoding, "deflate") || !strcasecmp(content_encoding, "x-deflate")) {
                dec = qore_inflate_to_binary;
            } else if (!strcasecmp(content_encoding, "gzip") || !strcasecmp(content_encoding, "x-gzip")) {
                dec = qore_gunzip_to_binary;
            } else if (!strcasecmp(content_encoding, "bzip2") || !strcasecmp(content_encoding, "x-bzip2")) {
                dec = qore_bunzip2_to_binary;
            } else if (!strcasecmp(content_encoding, "br")) {
                dec = qore_unbrotli_to_binary;
            } else if (!strcasecmp(content_encoding, "zstd")) {
                dec = qore_unzstd_to_binary;
            } else {
                xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "don't know how to handle content-encoding "
                    "'%s'", content_encoding);
                return -1;
            }

            int64 body_size = recv_data_holder->size();
            recv_data_holder = dec(*recv_data_holder, xsink);
            if (*xsink) {
                xsink->appendLastDescription(": while decompressing '%s' Content-Encoding with size " QLLD,
                    content_encoding, body_size);
                return -1;
            }
        }
    }

    assert(info);
    info->setKeyValue("response-body", recv_data_holder->refSelf(), xsink);

    //printd(5, "HttpClientConnectSendRecvPollOperation::processReceivedBody() this: %p received body: %ld "
    //    "(Content-Encoding: '%s')\n", this, recv_data_holder ? recv_data_holder->size() : 0l,
    //    content_encoding ? content_encoding : "n/a");

    // warning, the following line will output raw binary data
    //printd(5, "HttpClientConnectSendRecvPollOperation::processReceivedBody() this: %p body received: '%s'\n", this,
    //    recv_data_holder ? recv_data_holder->getPtr() : "n/a");

    return responseDone(xsink);
}

int HttpClientConnectSendRecvPollOperation::processH2Response(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());
    assert(poll_state);

    Http2ClientPollState* h2_poll = static_cast<Http2ClientPollState*>(poll_state.get());

    // Get HTTP status code
    http_response_code = h2_poll->getStatusCode();
    if (http_response_code < 0) {
        xsink->raiseException("HTTP2-ERROR", "no status code in HTTP/2 response");
        return -1;
    }

    // Get response headers
    response_headers = h2_poll->getResponseHeaders();
    if (!response_headers) {
        response_headers = new QoreHashNode(autoTypeInfo);
    }

    ReferenceHolder<QoreHashNode> response_headers_raw(response_headers->copy(), xsink);
    info->setKeyValue("response-headers-raw", response_headers_raw.release(), xsink);
    if (*xsink) {
        return -1;
    }

    set_http2_response_info(xsink, **response_headers, **info, http_response_code);
    if (*xsink) {
        return -1;
    }

    if (client->http_priv->processContentType(xsink, **response_headers)) {
        return -1;
    }
    set_body_content_type_info(xsink, **response_headers, **info);
    if (*xsink) {
        return -1;
    }

    info->setKeyValue("response-headers", response_headers->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }

    if (!response_headers->is_unique()) {
        response_headers = response_headers->copy();
    }

    // Get response body
    recv_data_holder = h2_poll->getResponseBody(xsink);
    if (*xsink) {
        return -1;
    }

    // Process Content-Encoding (decompression) if present
    if (recv_data_holder && response_headers) {
        const char* content_encoding = get_string_header(xsink, **response_headers, "content-encoding");
        if (*xsink) {
            return -1;
        }
        if (content_encoding) {
            // check for misuse of this header by including a character encoding value
            if (!strncasecmp(content_encoding, "iso", 3) || !strncasecmp(content_encoding, "utf-", 4)) {
                client->http_priv->msock->socket->setEncoding(QEM.findCreate(content_encoding));
            } else {
                qore_uncompress_to_binary_t dec = get_binary_decoder_for_content_encoding(
                    content_encoding, xsink);
                if (*xsink) {
                    return -1;
                }
                if (dec) {
                    int64 body_size = recv_data_holder->size();
                    recv_data_holder = dec(*recv_data_holder, xsink);
                    if (*xsink) {
                        xsink->appendLastDescription(": while decompressing '%s' Content-Encoding with size " QLLD,
                            content_encoding, body_size);
                        return -1;
                    }
                }
            }
        }
    }

    if (recv_data_holder) {
        info->setKeyValue("response-body", recv_data_holder->refSelf(), xsink);
        if (*xsink) {
            return -1;
        }
    }

    // Transfer HTTP/2 session to client for potential reuse
    Http2SessionPtr session = h2_poll->takeSession();
    if (session) {
        client->http_priv->setH2Session(session);
    }

    // Clear the poll state
    poll_state.reset();

    // Handle redirects
    if (!client->http_priv->redirect_passthru && http_response_code >= 300 && http_response_code < 400
        && http_response_code != 304) {
        return redirect(xsink);
    }

    setFinal(SPS_RECEIVED);
    return 0;
}

int HttpClientConnectSendRecvPollOperation::processH3Response(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());
    assert(poll_state);

    Http3ClientPollState* h3_poll = static_cast<Http3ClientPollState*>(poll_state.get());

    // Get HTTP status code
    http_response_code = h3_poll->getStatusCode();
    if (http_response_code < 0) {
        xsink->raiseException("HTTP3-ERROR", "no status code in HTTP/3 response");
        return -1;
    }

    // Get response headers
    response_headers = h3_poll->getResponseHeaders();
    if (!response_headers) {
        response_headers = new QoreHashNode(autoTypeInfo);
    }

    ReferenceHolder<QoreHashNode> response_headers_raw(response_headers->copy(), xsink);
    info->setKeyValue("response-headers-raw", response_headers_raw.release(), xsink);
    if (*xsink) {
        return -1;
    }

    // Set HTTP/3 response info (status_message, http_version, response-uri)
    const char* status_msg = get_http_status_message(http_response_code);
    response_headers->setKeyValue("status_code", http_response_code, xsink);
    if (*xsink) {
        return -1;
    }
    response_headers->setKeyValue("status_message", new QoreStringNode(status_msg), xsink);
    if (*xsink) {
        return -1;
    }
    response_headers->setKeyValue("http_version", new QoreStringNode("3"), xsink);
    if (*xsink) {
        return -1;
    }

    QoreStringNode* response_uri = new QoreStringNode();
    response_uri->sprintf("HTTP/3 %d %s", http_response_code, status_msg);
    info->setKeyValue("response-uri", response_uri, xsink);
    if (*xsink) {
        return -1;
    }

    if (client->http_priv->processContentType(xsink, **response_headers)) {
        return -1;
    }
    set_body_content_type_info(xsink, **response_headers, **info);
    if (*xsink) {
        return -1;
    }

    info->setKeyValue("response-headers", response_headers->refSelf(), xsink);
    if (*xsink) {
        return -1;
    }

    if (!response_headers->is_unique()) {
        response_headers = response_headers->copy();
    }

    // Get response body
    recv_data_holder = h3_poll->getResponseBody();

    // Process Content-Encoding (decompression) if present
    if (recv_data_holder && response_headers) {
        const char* content_encoding = get_string_header(xsink, **response_headers, "content-encoding");
        if (*xsink) {
            return -1;
        }
        if (content_encoding) {
            // check for misuse of this header by including a character encoding value
            if (!strncasecmp(content_encoding, "iso", 3) || !strncasecmp(content_encoding, "utf-", 4)) {
                client->http_priv->msock->socket->setEncoding(QEM.findCreate(content_encoding));
            } else {
                qore_uncompress_to_binary_t dec = get_binary_decoder_for_content_encoding(
                    content_encoding, xsink);
                if (*xsink) {
                    return -1;
                }
                if (dec) {
                    int64 body_size = recv_data_holder->size();
                    recv_data_holder = dec(*recv_data_holder, xsink);
                    if (*xsink) {
                        xsink->appendLastDescription(": while decompressing '%s' Content-Encoding with size " QLLD,
                            content_encoding, body_size);
                        return -1;
                    }
                }
            }
        }
    }

    if (recv_data_holder) {
        info->setKeyValue("response-body", recv_data_holder->refSelf(), xsink);
        if (*xsink) {
            return -1;
        }
    }

    // Clear the poll state (destructor restores fd blocking mode)
    poll_state.reset();

    // Handle redirects
    if (!client->http_priv->redirect_passthru && http_response_code >= 300 && http_response_code < 400
        && http_response_code != 304) {
        return redirect(xsink);
    }

    setFinal(SPS_RECEIVED);
    return 0;
}

int HttpClientConnectSendRecvPollOperation::responseDone(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    // issue #3116: pass a 304 Not Modified message back to the caller without processing
    if (!client->http_priv->redirect_passthru && http_response_code >= 300 && http_response_code < 400
        && http_response_code != 304) {
        //printd(5, "HttpClientConnectSendRecvPollOperation::responseDone() code: %d redirecting\n",
        //    http_response_code);
        return redirect(xsink);
    }

    if (use_proxy_connect) {
        //printd(5, "HttpClientConnectSendRecvPollOperation::responseDone() code: %d connecting SSL (proxy connect: %d "
        //    "proxy path: %s)\n", http_response_code, use_proxy_connect, proxy_path);

        use_proxy_connect = false;
        proxy_path = nullptr;

        // set client target for SNI
        client->http_priv->msock->socket->priv->client_target = client->http_priv->connection.host;

        poll_state.reset(client->http_priv->msock->socket->startSslConnect(xsink,
            client->http_priv->msock->cert, client->http_priv->msock->pk));
        if (*xsink) {
            return -1;
        }

        state = SPS_CONNECTING_PROXY_SSL;
        return 0;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::responseDone() code: %d done -> received\n",
    //    http_response_code);
    setFinal(SPS_RECEIVED);
    return 0;
}

int HttpClientConnectSendRecvPollOperation::redirect(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    // if we need to redirect to a new host, then disconnect the current socket
    if (!client->http_priv->proxy_connection.has_url()) {
        client->http_priv->disconnect_unlocked();
    } else if (use_proxy_connect) {
        use_proxy_connect = false;
        proxy_path = nullptr;
        client->http_priv->disconnect_unlocked();
    }
    // purge outbound message buffer
    send_data_holder = nullptr;

    host_override = false;
    const QoreStringNode* mess = response_headers->getKeyValue("status_message").get<QoreStringNode>();
    const QoreStringNode* loc = get_string_header_node(xsink, **response_headers, "location");
    if (*xsink) {
        return -1;
    }
    const char* location = loc && !loc->empty() ? loc->c_str() : 0;
    if (!location) {
        const char* msg = mess && !mess->empty() ? mess->c_str() : "<no message>";
        xsink->raiseException("HTTP-CLIENT-REDIRECT-ERROR", "no redirect location given for status code %d: "
            "message: '%s'", http_response_code, msg);
        return -1;
    }

    qore_socket_private* spriv = client->http_priv->msock->socket->priv;
    Queue* event_queue = client->priv->socket->getQueue();
    if (event_queue) {
        do_redirect_event(event_queue, spriv, loc, mess);
    }

    if (++redirect_count > (unsigned)client->http_priv->max_redirects) {
        const char* msg = mess && !mess->empty() ? mess->c_str() : "<no message>";
        xsink->raiseException("HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", "maximum redirections (%d) exceeded; "
            "redirect code %d to '%s' ignored (message: '%s')", client->http_priv->max_redirects, http_response_code,
            location, msg);
        return -1;
    }

    if (client->http_priv->redirectUrlUnlocked(location, connection, xsink)) {
        assert(*xsink);
        const char* msg = mess && !mess->empty() ? mess->c_str() : "<no message>";
        xsink->appendLastDescription(": while setting URL for redirect location '%s' (code %d: message: '%s')",
            location, http_response_code, msg);
        return -1;
    }

    if (!path_already_encoded) {
        path_already_encoded = true;
    }

    // set redirect info in info hash
    QoreString tmp;
    tmp.sprintf("redirect-%d", redirect_count);
    info->setKeyValue(tmp.c_str(), loc->refSelf(), xsink);
    assert(!*xsink);
    tmp.clear();
    tmp.sprintf("redirect-message-%d", redirect_count);
    info->setKeyValue(tmp.c_str(), mess ? mess->refSelf() : 0, xsink);
    assert(!*xsink);

    //printd(5, "HttpClientConnectSendRecvPollOperation::redirect() this: %p redirecting to: '%s'\n", this, location);

    return connectOrSend(xsink);
}

int HttpClientConnectSendRecvPollOperation::connectDone(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    bool connect_ssl = client->http_priv->proxy_connection.has_url()
        ? client->http_priv->proxy_connection.ssl
        : client->http_priv->connection.ssl;
    if (connect_ssl) {
        // Set up ALPN protocols based on current HTTP/2 mode and global settings
        // Always re-evaluate to handle mode changes on reused connections
        int http2_mode = client->http_priv->http2_mode;
        int global_mode = qore_global_http2_mode.load(std::memory_order_relaxed);
        bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
        bool http2_enabled = (http2_mode == HTTP2_MODE_AUTO || http2_mode == HTTP2_MODE_REQUIRED)
            && global_mode != HTTP2_MODE_DISABLED && !lib_disabled;

        if (http2_enabled) {
            // Set ALPN protocols for HTTP/2
            ReferenceHolder<QoreListNode> protocols(new QoreListNode(autoTypeInfo), xsink);
            if (http2_mode == HTTP2_MODE_REQUIRED) {
                // HTTP/2 only
                protocols->push(new QoreStringNode("h2"), xsink);
                if (*xsink) {
                    return -1;
                }
            } else {
                // Auto mode: prefer h2, fall back to http/1.1
                protocols->push(new QoreStringNode("h2"), xsink);
                if (*xsink) {
                    return -1;
                }
                protocols->push(new QoreStringNode("http/1.1"), xsink);
                if (*xsink) {
                    return -1;
                }
            }
            client->http_priv->msock->socket->setAlpnProtocols(*protocols, xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            // HTTP/2 is disabled - clear any previously set ALPN protocols
            client->http_priv->msock->socket->clearAlpnProtocols();
        }

        poll_state.reset(client->http_priv->msock->socket->startSslConnect(xsink,
            client->http_priv->msock->cert, client->http_priv->msock->pk));
        if (*xsink) {
            return -1;
        }
        state = SPS_CONNECTING_SSL;
        return 0;
    }
    if (startSend(xsink)) {
        return -1;
    }
    return 0;
}

int HttpClientConnectSendRecvPollOperation::startSend(ExceptionSink* xsink) {
    assert(client->http_priv->msock->m.trylock());

    // Check if HTTP/3 is active (QUIC session already established)
    if (client->http_priv->http3_active && client->http_priv->quic_session) {
        // For connect-only mode, we're done
        if (connect_only) {
            setFinal(SPS_CONNECTED);
            return 0;
        }

        // Convert QoreHashNode headers to strcase_str_map_t for HTTP/3
        strcase_str_map_t h3_headers;

        // Set host field automatically if not overridden
        if (!host_override && request_headers) {
            request_headers->setKeyValue("Host", client->http_priv->getHostHeaderValueUnlocked(connection),
                xsink);
            if (*xsink) {
                return -1;
            }
        }

        // Convert headers from QoreHashNode to map
        if (request_headers) {
            ConstHashIterator hi(*request_headers);
            while (hi.next()) {
                const char* key = hi.getKey();
                QoreValue val = hi.get();
                if (val.getType() == NT_STRING) {
                    h3_headers[key] = val.get<const QoreStringNode>()->c_str();
                } else if (val.getType() == NT_LIST) {
                    const QoreListNode* l = val.get<const QoreListNode>();
                    std::string joined;
                    for (size_t i = 0; i < l->size(); ++i) {
                        QoreValue elem = l->retrieveEntry(i);
                        if (elem.getType() == NT_STRING) {
                            if (!joined.empty()) {
                                joined += ", ";
                            }
                            joined += elem.get<const QoreStringNode>()->c_str();
                        }
                    }
                    if (!joined.empty()) {
                        h3_headers[key] = std::move(joined);
                    }
                }
            }
        }

        // Convert Host to :authority pseudo-header (RFC 9114 Section 4.3.1)
        auto host_it = h3_headers.find("Host");
        if (host_it != h3_headers.end()) {
            h3_headers[":authority"] = host_it->second;
            h3_headers.erase(host_it);
        }

        // Set Content-Length if body present
        if (data && size > 0) {
            auto cl_it = h3_headers.find("content-length");
            if (cl_it == h3_headers.end()) {
                h3_headers["content-length"] = std::to_string(size);
            }
        }

        // Get the request path
        qore_socket_private* spriv = client->http_priv->msock->socket->priv;
        QoreString pathstr(spriv->enc);
        client->http_priv->getMsgPath(xsink, connection, path.c_str(), pathstr, path_already_encoded);
        if (*xsink) {
            return -1;
        }

        // Create HTTP/3 poll state
        poll_state.reset(new Http3ClientPollState(xsink, client->http_priv->quic_session,
            client->http_priv->quic_fd, client->http_priv->quic_local_addr,
            client->http_priv->quic_local_addrlen,
            method.c_str(), pathstr.c_str(), h3_headers, data, size));
        if (*xsink) {
            return -1;
        }

        state = SPS_H3_ACTIVE;
        return 0;
    }

    // Check if HTTP/2 was negotiated via ALPN during the SSL connection
    // First check global mode - even if ALPN negotiated h2, respect the global setting
    int global_mode = qore_global_http2_mode.load(std::memory_order_relaxed);
    bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
    bool http2_globally_disabled = (global_mode == HTTP2_MODE_DISABLED || lib_disabled);

    if (!http2_globally_disabled && client->http_priv->msock->socket->isHttp2()) {
        // Set http2_active flag on the client
        client->http_priv->http2_active = true;
        // HTTP/2 sends many small frames as separate writes; enable TCP_NODELAY
        // to prevent Nagle + delayed ACK interaction causing ~40ms per-request delays
        client->http_priv->msock->socket->setNoDelay(1);

        // For connect-only mode, we're done after HTTP/2 connection is established
        if (connect_only) {
            setFinal(SPS_CONNECTED);
            return 0;
        }

        // Convert QoreHashNode headers to vector of pairs for HTTP/2
        // Using vector<pair> allows duplicate header names (e.g., multiple cookie entries)
        std::vector<std::pair<std::string, std::string>> h2_headers;

        // Set host field automatically if not overridden
        if (!host_override && request_headers) {
            request_headers->setKeyValue("Host", client->http_priv->getHostHeaderValueUnlocked(connection),
                xsink);
            if (*xsink) {
                return -1;
            }
        }

        // Convert headers from QoreHashNode to vector of pairs
        // HTTP/2 supports multiple header fields with the same name as separate entries
        if (request_headers) {
            ConstHashIterator hi(*request_headers);
            while (hi.next()) {
                const char* key = hi.getKey();
                QoreValue val = hi.get();
                if (val.getType() == NT_STRING) {
                    h2_headers.emplace_back(key, val.get<const QoreStringNode>()->c_str());
                } else if (val.getType() == NT_LIST) {
                    // Emit separate header entries for each list value
                    const QoreListNode* l = val.get<const QoreListNode>();
                    for (size_t i = 0; i < l->size(); ++i) {
                        QoreValue lv = l->retrieveEntry(i);
                        if (lv.getType() == NT_STRING) {
                            h2_headers.emplace_back(key, lv.get<const QoreStringNode>()->c_str());
                        }
                    }
                }
            }
        }

        // Get the request path
        qore_socket_private* spriv = client->http_priv->msock->socket->priv;
        QoreString pathstr(spriv->enc);
        client->http_priv->getMsgPath(xsink, connection, path.c_str(), pathstr, path_already_encoded);
        if (*xsink) {
            return -1;
        }

        // Determine scheme
        const char* scheme = connection.ssl ? "https" : "http";

        // Create HTTP/2 poll state
        poll_state.reset(new Http2ClientPollState(xsink, spriv, method.c_str(), pathstr.c_str(),
            h2_headers, data, size, scheme));
        if (*xsink) {
            return -1;
        }

        // Session transfer to client happens when poll completes (in processH2Response)
        state = SPS_H2_ACTIVE;
        return 0;
    }

    // Check if HTTP/2 is active from a previous operation (shouldn't happen in poll mode normally)
    if (client->http_priv->http2_active) {
        xsink->raiseException("HTTP2-POLL-ERROR", "HTTP/2 session already active; "
            "use the synchronous send() method or disable HTTP/2 with http_version=1.1");
        return -1;
    }

    qore_socket_private* spriv = client->http_priv->msock->socket->priv;

    assert(!poll_state);
    if (!send_data_holder) {
        if (use_proxy_connect) {
            QoreString hdr(spriv->enc);
            spriv->getSendHttpMessageHeaders(hdr, *info, "CONNECT",
                proxy_path, client->http_priv->http11 ? "1.1" : "1.0", *proxy_headers, 0, QORE_SOURCE_HTTPCLIENT);
            send_data_holder = getMessage(hdr);
        } else if (connect_only) {
            setFinal(SPS_CONNECTED);
            return 0;
        } else {
            // set host field automatically if not overridden
            if (!host_override) {
                request_headers->setKeyValue("Host", client->http_priv->getHostHeaderValueUnlocked(connection),
                    xsink);
                assert(!*xsink);
                //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p set host: %s\n", this,
                //    request_headers->getKeyValue("Host").get<QoreStringNode>()->c_str());
            }

            QoreString pathstr(spriv->enc);
            client->http_priv->getMsgPath(xsink, connection, path.c_str(), pathstr, path_already_encoded);
            if (*xsink) {
                return -1;
            }
            //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p %s %s\n", this, method.c_str(),
            //    pathstr.c_str());
            QoreString hdr(spriv->enc);
            spriv->getSendHttpMessageHeaders(hdr, *info, method.c_str(),
                pathstr.c_str(), client->http_priv->http11 ? "1.1" : "1.0", *request_headers, size,
                QORE_SOURCE_HTTPCLIENT);
            send_data_holder = getMessage(hdr);
            if (data) {
                assert(size);
                send_data_holder->append(data, size);
            }
        }
    }

    assert(send_data_holder);
    poll_state.reset(new SocketSendPollState(xsink, spriv, (const char*)send_data_holder->getPtr(),
        send_data_holder->size()));
    if (*xsink) {
        return -1;
    }

    //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p sending %ld bytes\n", this,
    //    send_data_holder->size());

    // NOTE: the following line will print binary data
    //printd(5, "HttpClientConnectSendRecvPollOperation::startSend() this: %p sending: '%s'\n", this,
    //    send_data_holder->getPtr());

    state = SPS_SENDING;
    assert(poll_state);
    return 0;
}

BinaryNode* HttpClientConnectSendRecvPollOperation::getMessage(QoreString& hdr) {
    size_t strsize = hdr.size();
    return new BinaryNode(hdr.giveBuffer(), strsize);
}

void HttpClientConnectSendRecvPollOperation::setFinal(int final_state) {
    assert(client->http_priv->msock->m.trylock());

    state = final_state;
    client->priv->clearNonBlock();
    set_non_block = false;
}

// static initialization
void QoreHttpClientObject::static_init() {
    // setup static members of QoreHttpClientObject class
    method_map.insert(method_map_t::value_type("OPTIONS", true));
    // FIXME: GET should not take a message body; this should be false
    // but it cannot be changed or it would break backwards compatibility
    // appropriate notes have been added to the API docs
    method_map.insert(method_map_t::value_type("GET", true));
    method_map.insert(method_map_t::value_type("HEAD", false));
    method_map.insert(method_map_t::value_type("POST", true));
    method_map.insert(method_map_t::value_type("PUT", true));
    method_map.insert(method_map_t::value_type("DELETE", true));
    method_map.insert(method_map_t::value_type("TRACE", true));
    method_map.insert(method_map_t::value_type("CONNECT", true));
    // PATCH: https://tools.ietf.org/html/rfc5789
    method_map.insert(method_map_t::value_type("PATCH", true));

    header_ignore.insert("Content-Length");
}

QoreHttpClientObject::QoreHttpClientObject() : http_priv(new qore_httpclient_priv(priv)) {
    http_priv->setSocketPath(http_priv->connection);
}

QoreHttpClientObject::~QoreHttpClientObject() {
    delete http_priv;
}

void QoreHttpClientObject::deref(ExceptionSink* xsink) {
    if (ROdereference()) {
        cleanup(xsink);
        delete this;
    }
}

QoreObject* QoreHttpClientObject::startPollConnect(ExceptionSink* xsink, QoreObject* self) {
    return http_priv->startPollConnect(xsink, self, this);
}

QoreObject* QoreHttpClientObject::startPollSendRecv(ExceptionSink* xsink, QoreObject* self, const QoreString* method,
            const QoreString* path, const AbstractQoreNode* data_save, const void* data, size_t size,
            const QoreHashNode* headers, const QoreEncoding* enc) {
    return http_priv->startPollSendRecv(xsink, self, this, method, path, data_save, data, size, headers, enc);
}

void QoreHttpClientObject::setDefaultPort(int def_port) {
    http_priv->default_port = def_port;
}

const char* QoreHttpClientObject::getDefaultPath() const {
    return http_priv->default_path.empty() ? 0 : http_priv->default_path.c_str();
}

const char* QoreHttpClientObject::getConnectionPath() const {
    SafeLocker sl(priv->m);
    return http_priv->connection.path.empty() ? getDefaultPath() : http_priv->connection.path.c_str();
}

void QoreHttpClientObject::setConnectionPath(const char* path) {
    SafeLocker sl(priv->m);
    if (path && path[0]) {
        http_priv->connection.path = path;
    } else {
        http_priv->connection.path.clear();
    }
}

void QoreHttpClientObject::setDefaultPath(const char* def_path) {
    // issue #2610: assigning a std::string a nullptr causes a crash
    http_priv->default_path = def_path ? def_path : "";
}

void QoreHttpClientObject::setTimeout(int to) {
    http_priv->timeout = to;
}

int QoreHttpClientObject::getTimeout() const {
    return http_priv->timeout;
}

void QoreHttpClientObject::setEncoding(const QoreEncoding* qe) {
    http_priv->setEncoding(qe);
}

const QoreEncoding* QoreHttpClientObject::getEncoding() const {
    return http_priv->getEncoding();
}

int QoreHttpClientObject::setOptions(const QoreHashNode* opts, ExceptionSink* xsink) {
    // process new protocols
    QoreValue n = opts->getKeyValue("protocols");

    if (n.getType() == NT_HASH) {
        const QoreHashNode* h = n.get<const QoreHashNode>();
        ConstHashIterator hi(h);
        while (hi.next()) {
            const QoreValue v = hi.get();
            qore_type_t vtype = v.getType();
            if (vtype != NT_HASH && vtype != NT_INT) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "value of protocol hash key '%s' is not a hash or "
                    "an int", hi.getKey());
                return -1;
            }
            bool need_ssl = false;
            int need_port;
            if (vtype == NT_INT) {
                need_port = (int)v.getAsBigInt();
            } else {
                const QoreHashNode* vh = v.get<const QoreHashNode>();
                need_port = (int)vh->getKeyValue("port").getAsBigInt();
                if (!need_port) {
                    xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "'port' key in protocol hash key '%s' is "
                        "missing or zero", hi.getKey());
                    return -1;
                }
                need_ssl = vh->getKeyValue("ssl").getAsBool();
            }
            http_priv->prot_map[hi.getKey()] = make_protocol(need_port, need_ssl);
        }
    }

    n = opts->getKeyValue("max_redirects");
    if (!n.isNothing())
        http_priv->max_redirects = (int)n.getAsBigInt();

    n = opts->getKeyValue("default_port");
    if (!n.isNothing()) {
        http_priv->default_port = (int)n.getAsBigInt();
    } else {
        http_priv->default_port = HTTPCLIENT_DEFAULT_PORT;
    }

    // check if proxy is true
    n = opts->getKeyValue("proxy");
    if (n.getType() == NT_STRING && http_priv->setProxyUrlUnlocked((n.get<const QoreStringNode>())->c_str(), xsink)) {
        return -1;
    }

    // parse url option if present
    n = opts->getKeyValue("url");
    if (n.getType() == NT_STRING && http_priv->setUrlUnlocked((n.get<const QoreStringNode>())->c_str(), xsink)) {
        return -1;
    }

    // set username and password, if applicable
    if (http_priv->connection.username.empty() && http_priv->connection.password.empty()) {
        n = opts->getKeyValue("username");
        if (n.getType() == NT_STRING) {
            const QoreValue p = opts->getKeyValue("password");
            if (p.getType() == NT_STRING) {
                http_priv->setUserPassword(n.get<const QoreStringNode>()->c_str(),
                    p.get<const QoreStringNode>()->c_str());
            }
        }
    }

    n = opts->getKeyValue("default_path");
    if (n.getType() == NT_STRING) {
        http_priv->default_path = (n.get<const QoreStringNode>())->c_str();
    }

    // set default timeout if given in option hash - accept relative date/time values as well as integers
    n = opts->getKeyValue("timeout");
    if (!n.isNothing()) {
        http_priv->timeout = get_ms_zero(n);
    }

    n = opts->getKeyValue("http_version");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string version ('auto', '1.0', '1.1', "
                "'2.0', '3.0') as value for the \"http_version\" key in the options hash");
            return -1;
        }
        if (setHTTPVersion((n.get<const QoreStringNode>())->c_str(), xsink)) {
            return -1;
        }
    }

    n = opts->getKeyValue("http3_mode");
    if (!n.isNothing()) {
        if (n.getType() == NT_STRING) {
            const char* str = n.get<const QoreStringNode>()->c_str();
            int mode = parseHttp3ModeString(str);
            if (mode < 0) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "invalid http3_mode value '%s'; "
                    "valid values are: 'disabled', 'auto', 'required'", str);
                return -1;
            }
            http_priv->http3_mode = mode;
        } else if (n.getType() == NT_INT) {
            int mode = (int)n.getAsBigInt();
            if (mode < HTTP3_MODE_DISABLED || mode > HTTP3_MODE_REQUIRED) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "invalid http3_mode value %d; "
                    "valid values are: 0 (disabled), 1 (auto), 2 (required)", mode);
                return -1;
            }
            http_priv->http3_mode = mode;
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string or int for the "
                "\"http3_mode\" key in the options hash");
            return -1;
        }
    }

    n = opts->getKeyValue("headers");
    if (!n.isNothing()) {
        if (n.getType() != NT_HASH) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting hash of headers as value for the "
                "\"headers\" key in the options hash");
            return -1;
        }
        addDefaultHeaders(n.get<const QoreHashNode>());
    }

    n = opts->getKeyValue("event_queue");
    if (n.getType() == NT_OBJECT) {
        const QoreObject* o = n.get<const QoreObject>();
        Queue* q = static_cast<Queue*>(o->getReferencedPrivateData(CID_QUEUE, xsink));
        if (*xsink)
            return -1;

        if (q) { // pass reference from QoreObject::getReferencedPrivateData() to function
            priv->socket->setEventQueue(xsink, q, QoreValue(), false);
        }
    }

    n = opts->getKeyValue("connect_timeout");
    if (!n.isNothing()) {
        http_priv->connect_timeout_ms = (int)get_ms_zero(n);
    }

    if (http_priv->connection.path.empty() && !http_priv->default_path.empty()) {
        http_priv->connection.path = http_priv->default_path;
    }

    // additional HTTP methods for customized extensions like WebDAV
    n = opts->getKeyValue("additional_methods");
    if (!n.isNothing()) {
        if (n.getType() != NT_HASH) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "Option \"additional_methods\" requires a hash as a "
                "value; got: %s", n.getTypeName());
            return -1;
        }
        ConstHashIterator hi(n.get<const QoreHashNode>());
        while (hi.next()) {
            http_priv->addHttpMethod(hi.getKey(), hi.get().getAsBool());
        }
    }

    // issue #3693: assume HTTP encoding
    n = opts->getKeyValue("assume_encoding");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string as value for the \"assume_encoding\" "
                "key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        const QoreStringNode* val = n.get<const QoreStringNode>();
        qore_socket_private::get(*priv->socket)->setAssumedEncoding(!val->empty() ? val->c_str() : nullptr);
    }

    n = opts->getKeyValue("ssl_cert_data");
    if (n) {
        SimpleRefHolder<QoreSSLCertificate> cert;
        if (n.getType() == NT_BINARY) {
            cert = new QoreSSLCertificate(n.get<const BinaryNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else if (n.getType() == NT_STRING) {
            cert = new QoreSSLCertificate(n.get<const QoreStringNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting \"string\" or \"binary\" value assigned to "
                "\"ssl_cert_data\" HTTPClient option; got type \"%s\" instead", n.getTypeName());
            return -1;
        }

        assert(!priv->cert);
        priv->cert = cert.release();
    } else {
        n = opts->getKeyValue("ssl_cert_path");
        if (!n.isNothing()) {
            if (n.getType() != NT_STRING) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string filename as value for the "
                    "\"ssl_cert_path\" key in the options hash; got type \"%s\" instead", n.getTypeName());
                return -1;
            }
            const QoreStringNode* path = n.get<const QoreStringNode>();
            if (runtime_check_parse_option(PO_NO_FILESYSTEM)) {
                xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the \"ssl_cert_path\" option = \"%s\" "
                    "when sandboxing restriction PO_NO_FILESYSTEM is set", path->c_str());
                return -1;
            }

            // read in certificate file and set the certificate
            QoreFile f;
            if (f.open2(xsink, path->c_str())) {
                return -1;
            }

            QoreString pem;
            if (f.read(pem, -1, xsink)) {
                return -1;
            }

            SimpleRefHolder<QoreSSLCertificate> cert(new QoreSSLCertificate(&pem, xsink));
            if (*xsink) {
                return -1;
            }

            assert(!priv->cert);
            priv->cert = cert.release();
        }
    }

    const char* key_password = nullptr;
    n = opts->getKeyValue("ssl_key_password");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string value for the \"ssl_key_password\" "
                "key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        key_password = n.get<const QoreStringNode>()->c_str();
    }

    n = opts->getKeyValue("ssl_key_data");
    if (n) {
        SimpleRefHolder<QoreSSLPrivateKey> pk;
        if (n.getType() == NT_BINARY) {
            // no private key possible with keys in DER format
            pk = new QoreSSLPrivateKey(n.get<const BinaryNode>(), xsink);
            if (*xsink) {
                return -1;
            }
        } else if (n.getType() == NT_STRING) {
            pk = new QoreSSLPrivateKey(n.get<const QoreStringNode>(), key_password, xsink);
            if (*xsink) {
                return -1;
            }
        } else {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting \"string\" or \"binary\" value assigned to "
                "\"ssl_key_data\" HTTPClient option; got type \"%s\" instead", n.getTypeName());
            return -1;
        }

        assert(!priv->pk);
        priv->pk = pk.release();
    } else {
        n = opts->getKeyValue("ssl_key_path");
        if (!n.isNothing()) {
            if (n.getType() != NT_STRING) {
                xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting string filename as value for the "
                    "\"ssl_key_path\" key in the options hash; got type \"%s\" instead", n.getTypeName());
                return -1;
            }
            const QoreStringNode* path = n.get<const QoreStringNode>();
            if (runtime_check_parse_option(PO_NO_FILESYSTEM)) {
                xsink->raiseException("ILLEGAL-FILESYSTEM-ACCESS", "cannot use the \"ssl_key_path\" option = \"%s\" "
                    "when sandboxing restriction PO_NO_FILESYSTEM is set", path->c_str());
                return -1;
            }

            // read in private key file and set the private key
            QoreFile f;
            if (f.open2(xsink, path->c_str())) {
                return -1;
            }

            QoreString pem;
            if (f.read(pem, -1, xsink)) {
                return -1;
            }

            SimpleRefHolder<QoreSSLPrivateKey> pk(new QoreSSLPrivateKey(&pem, key_password, xsink));
            if (*xsink) {
                return -1;
            }

            assert(!priv->pk);
            priv->pk = pk.release();
        }
    }

    n = opts->getKeyValue("ssl_verify_cert");
    if (n.getAsBool()) {
        priv->socket->setSslVerifyMode(SSL_VERIFY_PEER);
    }

    n = opts->getKeyValue("error_passthru");
    if (n.getAsBool()) {
        http_priv->error_passthru = true;
    }

    n = opts->getKeyValue("redirect_passthru");
    if (n.getAsBool()) {
        http_priv->redirect_passthru = true;
    }

    n = opts->getKeyValue("encoding_passthru");
    if (n.getAsBool()) {
        http_priv->encoding_passthru = true;
    }

    n = opts->getKeyValue("pre_encoded_urls");
    if (n.getAsBool()) {
        http_priv->pre_encoded_urls = true;
    }

    // issue #4773: allow the set of automatically-percent-encoded characters to be expanded
    n = opts->getKeyValue("encode_chars");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting a string as the value for the "
                "\"encode_chars\" key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        const QoreStringNode* chars = n.get<const QoreStringNode>();
        for (size_t i = 0, e = chars->size(); i < e; ++i) {
            http_priv->setEncodeChar((*chars)[i]);
        }
    }

    // issue #3978: allow the output encoding to be set as an option
    n = opts->getKeyValue("encoding");
    if (!n.isNothing()) {
        if (n.getType() != NT_STRING) {
            xsink->raiseException("HTTP-CLIENT-OPTION-ERROR", "expecting a string encoding as the value for the "
                "\"encoding\" key in the options hash; got type \"%s\" instead", n.getTypeName());
            return -1;
        }
        const QoreStringNode* enc_str = n.get<const QoreStringNode>();
        const QoreEncoding* enc = QEM.findCreate(enc_str);
        priv->socket->setEncoding(enc);
        http_priv->enc = enc;
    }

    return 0;
}

void QoreHttpClientObject::setConnectTimeout(int ms) {
    SafeLocker sl(priv->m);
    http_priv->connect_timeout_ms = ms < 0 ? -1 : ms;
}

int QoreHttpClientObject::getConnectTimeout() const {
    return http_priv->connect_timeout_ms;
}

int QoreHttpClientObject::setURL(const char* str, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    // disconnect immediately if not using a proxy
    if (!http_priv->proxy_connection.has_url())
        http_priv->disconnect_unlocked();
    return http_priv->setUrlUnlocked(str, xsink);
}

QoreStringNode* QoreHttpClientObject::getUrl(int64 code) {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.has_url()) {
        return nullptr;
    }
    return http_priv->connection.get_url(code);
}

int QoreHttpClientObject::setHTTPVersion(const char* version, ExceptionSink* xsink) {
    int rc = 0;
    SafeLocker sl(priv->m);
    if (!strcmp(version, "1.0")) {
        http_priv->http11 = false;
        http_priv->http2_mode = HTTP2_MODE_DISABLED;
        http_priv->http3_mode = HTTP3_MODE_DISABLED;
    } else if (!strcmp(version, "1.1")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_DISABLED;
        http_priv->http3_mode = HTTP3_MODE_DISABLED;
    } else if (!strcasecmp(version, "auto")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_AUTO;
        // ALPN protocols are set at connect time based on both object mode and global mode
    } else if (!strcmp(version, "2.0") || !strcmp(version, "2")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_REQUIRED;
        // ALPN protocols are set at connect time based on both object mode and global mode
    } else if (!strcmp(version, "3.0") || !strcmp(version, "3")) {
        http_priv->http11 = true;
        http_priv->http2_mode = HTTP2_MODE_DISABLED;
        http_priv->http3_mode = HTTP3_MODE_REQUIRED;
    } else {
        xsink->raiseException("HTTP-VERSION-ERROR", "only 'auto', '1.0', '1.1', '2.0', '2', '3.0', '3' are "
            "valid (value passed: '%s')", version);
        rc = -1;
    }
    return rc;
}

const char* QoreHttpClientObject::getHTTPVersion() const {
    // Return the configured version, not the active version
    if (http_priv->http3_mode == HTTP3_MODE_REQUIRED) {
        return "3.0";
    }
    switch (http_priv->http2_mode) {
        case HTTP2_MODE_AUTO:
            return "auto";
        case HTTP2_MODE_REQUIRED:
            return "2.0";
        default:
            return http_priv->http11 ? "1.1" : "1.0";
    }
}

void QoreHttpClientObject::setHTTP11(bool val) {
    http_priv->http11 = val;
}

bool QoreHttpClientObject::isHTTP11() const {
    return http_priv->http11;
}

bool QoreHttpClientObject::isHttp2Enabled() const {
    return http_priv->http2_mode != HTTP2_MODE_DISABLED;
}

void QoreHttpClientObject::setHttp2Enabled(bool enable) {
    // Map old API to new http2_mode
    http_priv->http2_mode = enable ? HTTP2_MODE_AUTO : HTTP2_MODE_DISABLED;
    // ALPN protocols are set at connect time based on both object mode and global mode
    // This allows the global mode to override the object setting at runtime
}

void QoreHttpClientObject::setHttp2Mode(int mode, ExceptionSink* xsink) {
    if (mode < HTTP2_MODE_DISABLED || mode > HTTP2_MODE_H2C_UPGRADE) {
        xsink->raiseException("HTTP2-MODE-ERROR", "invalid HTTP/2 mode %d; valid modes are: 0 (disabled), "
            "1 (auto), 2 (required), 3 (h2c-direct), 4 (h2c-upgrade)", mode);
        return;
    }
    http_priv->http2_mode = mode;
    // ALPN protocols are set at connect time based on both object mode and global mode
    // This allows the global mode to override the object setting at runtime
}

int QoreHttpClientObject::getHttp2Mode() const {
    return http_priv->http2_mode;
}

void QoreHttpClientObject::setHttp3Mode(int mode, ExceptionSink* xsink) {
    if (mode < HTTP3_MODE_DISABLED || mode > HTTP3_MODE_REQUIRED) {
        xsink->raiseException("HTTP3-MODE-ERROR", "invalid HTTP/3 mode %d; valid modes are: 0 (disabled), "
            "1 (auto), 2 (required)", mode);
        return;
    }
    // NOTE: Setting to DISABLED does not disconnect an active QUIC session;
    // the mode change takes effect on the next request cycle when the existing
    // connection drops or the caller explicitly calls disconnect().
    http_priv->http3_mode = mode;
}

int QoreHttpClientObject::getHttp3Mode() const {
    return http_priv->http3_mode;
}

bool QoreHttpClientObject::isHttp3Active() const {
    return http_priv->http3_active;
}

int QoreHttpClientObject::getPollableDescriptor() const {
    if (http_priv->http3_active.load() && http_priv->quic_fd >= 0) {
        return http_priv->quic_fd;
    }
    return QoreSocketObject::getPollableDescriptor();
}

bool QoreHttpClientObject::isHttp2Active() const {
    return http_priv->http2_active;
}

QoreHashNode* QoreHttpClientObject::getHttp2Settings() const {
    if (!http_priv->http2_active) {
        return nullptr;
    }
    // Return current HTTP/2 settings
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), nullptr);
    rv->setKeyValue("header_table_size", 4096, nullptr);
    rv->setKeyValue("enable_push", true, nullptr);
    rv->setKeyValue("max_concurrent_streams", 100, nullptr);
    rv->setKeyValue("initial_window_size", 65535, nullptr);
    rv->setKeyValue("max_frame_size", 16384, nullptr);
    return rv.release();
}

void QoreHttpClientObject::setHttp2Settings(const QoreHashNode* settings, ExceptionSink* xsink) {
    // HTTP/2 settings will be applied when establishing the connection
    // For now, just validate the settings
    if (!settings) {
        return;
    }
    // Settings validation would go here
}

void QoreHttpClientObject::setHttp2StreamPriority(int32_t stream_id, int32_t weight, int32_t dependency,
        bool exclusive, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 is not active");
        return;
    }
    http_priv->getH2Session()->submitPriority(stream_id, dependency, weight, exclusive, xsink);
}

QoreStringNode* QoreHttpClientObject::getHttpVersion() const {
    if (http_priv->http2_active) {
        return new QoreStringNode("HTTP/2");
    }
    return new QoreStringNode(http_priv->http11 ? "HTTP/1.1" : "HTTP/1.0");
}

QoreHashNode* QoreHttpClientObject::sendHttp2Connect(const char* path, const QoreHashNode* headers,
        const char* protocol, QoreHashNode* info, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    // Ensure we're connected with HTTP/2
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        // Try to connect first
        if (http_priv->connect_unlocked(xsink, http_priv->connection)) {
            return nullptr;
        }
        if (!http_priv->http2_active || !http_priv->getH2Session()) {
            xsink->raiseException("HTTP2-ERROR", "HTTP/2 connection required for extended CONNECT");
            return nullptr;
        }
    }

    // RFC 8441: Check if server supports extended CONNECT before sending.
    // Some nghttp2 versions silently drop :protocol when ENABLE_CONNECT_PROTOCOL
    // is not advertised, causing the CONNECT to be processed as a bare tunnel
    // request with no response sent to the client.
    if (http_priv->getH2Session()->isExtendedConnectRejected()) {
        xsink->raiseException("HTTP2-CONNECT-ERROR",
            "server does not support extended CONNECT "
            "(ENABLE_CONNECT_PROTOCOL not advertised in SETTINGS)");
        return nullptr;
    }

    // Build headers map with :protocol for extended CONNECT (RFC 8441)
    strcase_str_map_t h2_headers;
    h2_headers[":protocol"] = protocol;

    // Add Host header with port when needed (used to derive :authority)
    SimpleRefHolder<QoreStringNode> host_header(http_priv->getHostHeaderValueUnlocked(http_priv->connection));
    if (host_header) {
        h2_headers["host"] = host_header->c_str();
    }

    // Copy custom headers
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                h2_headers[key] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    // Submit CONNECT request
    int32_t stream_id = http_priv->getH2Session()->submitRequest("CONNECT", path, h2_headers,
        nullptr, 0, xsink);
    if (stream_id < 0) {
        return nullptr;
    }

    // Send the request (use blocking version for client-side operations)
    if (http_priv->getH2Session()->sendPendingDataBlocking(http_priv->timeout, xsink) < 0) {
        return nullptr;
    }

    // Read the response with overall timeout
    int64 deadline_ms = http_priv->timeout < 0 ? -1 : q_clock_getmillis() + http_priv->timeout;
    while (true) {
        int recv_timeout = http_priv->timeout;
        if (deadline_ms >= 0) {
            int64 now_ms = q_clock_getmillis();
            if (now_ms >= deadline_ms) {
                xsink->raiseException("HTTP2-CONNECT-ERROR",
                    "timeout waiting for HTTP/2 CONNECT response (timeout: %d ms)",
                    (int)http_priv->timeout);
                return nullptr;
            }
            recv_timeout = static_cast<int>(deadline_ms - now_ms);
        }
        int rv = http_priv->getH2Session()->receiveData(recv_timeout, xsink);
        if (rv < 0) {
            return nullptr;
        }
        // Flush pending output (SETTINGS_ACK, etc.) after processing received frames.
        // Following nginx pattern: always send pending data after recv processing.
        // Recompute timeout from deadline since receiveData() may have consumed time.
        // NOTE: unlike the loop entry deadline check above, we use zero-timeout (non-blocking)
        // rather than raising an error when the deadline has passed, because this flush is a
        // necessary consequence of processing inbound frames — the pending data (typically small
        // control frames like SETTINGS_ACK) must be sent for the protocol to proceed, and a
        // non-blocking send will almost always succeed for small frames in the kernel buffer.
        int flush_timeout = recv_timeout;
        if (deadline_ms >= 0) {
            int64 now_ms = q_clock_getmillis();
            flush_timeout = now_ms >= deadline_ms ? 0 : static_cast<int>(deadline_ms - now_ms);
        }
        if (http_priv->getH2Session()->sendPendingDataBlocking(flush_timeout, xsink) < 0) {
            return nullptr;
        }
        if (rv == 1) {
            // Connection was closed by peer - check if we got a response first
            Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
            if (stream && stream->headers_complete) {
                // Process the response we received before connection close
                // Fall through to normal processing
            } else {
                // Connection closed without receiving a complete response
                xsink->raiseException("HTTP2-CONNECT-ERROR",
                    "HTTP/2 connection closed by peer before CONNECT response was received");
                return nullptr;
            }
        }

        // Check if SETTINGS revealed that server doesn't support extended CONNECT.
        // This catches the case where SETTINGS are received after the CONNECT was sent;
        // some nghttp2 versions silently drop :protocol, so no RST_STREAM will arrive.
        if (http_priv->getH2Session()->isExtendedConnectRejected()) {
            // Cancel the submitted stream to prevent leaking it in the session map
            // and notify the server to stop processing the bare CONNECT.
            // sendPendingDataBlocking() triggers onStreamCloseCallback which removes
            // the stream from the session map.
            http_priv->getH2Session()->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
            if (!*xsink) {
                http_priv->getH2Session()->sendPendingDataBlocking(0, xsink);
            }
            // Clear cleanup errors — the real error is missing ENABLE_CONNECT_PROTOCOL
            xsink->clear();
            xsink->raiseException("HTTP2-CONNECT-ERROR",
                "server does not support extended CONNECT "
                "(ENABLE_CONNECT_PROTOCOL not advertised in SETTINGS)");
            return nullptr;
        }

        // Check if the stream was reset (e.g., RST_STREAM from server)
        {
            Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
            if (stream && stream->reset) {
                xsink->raiseException("HTTP2-CONNECT-ERROR",
                    "HTTP/2 CONNECT request was rejected by the server "
                    "(stream reset with error code %d)", (int)stream->error_code);
                return nullptr;
            }
        }

        // Check if we have a response
        Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
        if (stream && stream->headers_complete) {
            // Build response hash
            ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
            rv->setKeyValue("status_code", stream->status_code, xsink);
            rv->setKeyValue("stream_id", stream_id, xsink);

            // Add response headers (handle duplicate headers per RFC 7540)
            rv->setKeyValue("headers", httpMultiHeadersToQoreHash(stream->headers), xsink);

            // Store stream ID on both HTTPClient and socket for isDataAvailable()
            http_priv->setActiveH2StreamId(stream_id);

            // Add info if requested
            if (info) {
                info->setKeyValue("http2", true, xsink);
                info->setKeyValue("stream_id", stream_id, xsink);
                info->setKeyValue("status_code", stream->status_code, xsink);
            }

            // RFC 8441: 200 OK means CONNECT succeeded
            if (stream->status_code != 200) {
                xsink->raiseException("HTTP2-CONNECT-ERROR",
                    "HTTP/2 CONNECT request failed with status %d", stream->status_code);
                return nullptr;
            }

            return rv.release();
        }
    }
}

// Forward declaration — defined later in the file
static int driveQuicIo(QuicSession* session, int fd,
        const struct sockaddr_storage& local_addr, socklen_t local_addrlen,
        int poll_timeout_ms, ExceptionSink* xsink);

QoreHashNode* QoreHttpClientObject::sendHttp3Connect(const char* path, const QoreHashNode* headers,
        const char* protocol, QoreHashNode* info, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    // Ensure we're connected with HTTP/3 (QUIC)
    if (!http_priv->http3_active || !http_priv->quic_session) {
        // Establish QUIC connection directly (not TCP+TLS)
        if (http_priv->connectQuic(xsink, http_priv->connection)) {
            return nullptr;
        }
        if (!http_priv->http3_active || !http_priv->quic_session) {
            xsink->raiseException("HTTP3-ERROR", "HTTP/3 connection required for extended CONNECT");
            return nullptr;
        }
    }

    // RFC 9220: Check if server supports extended CONNECT before sending
    if (http_priv->quic_session->isExtendedConnectRejected()) {
        xsink->raiseException("HTTP3-CONNECT-ERROR",
            "server does not support extended CONNECT "
            "(enable_connect_protocol not advertised in SETTINGS)");
        return nullptr;
    }

    // Build headers map
    strcase_str_map_t h3_headers;

    // Add Host header with port when needed (used to derive :authority)
    SimpleRefHolder<QoreStringNode> host_header(http_priv->getHostHeaderValueUnlocked(http_priv->connection));
    if (host_header) {
        h3_headers["host"] = host_header->c_str();
    }

    // Copy custom headers
    if (headers) {
        ConstHashIterator hi(headers);
        while (hi.next()) {
            const char* key = hi.getKey();
            QoreValue val = hi.get();
            if (val.getType() == NT_STRING) {
                h3_headers[key] = val.get<const QoreStringNode>()->c_str();
            }
        }
    }

    // Submit CONNECT request with :protocol pseudo-header
    int64_t stream_id = http_priv->quic_session->submitConnectRequest(path, h3_headers,
        protocol, xsink);
    if (stream_id < 0) {
        return nullptr;
    }

    // Helper to cancel the CONNECT stream on error paths — cleans up streaming_body_data_
    // and connect_tunnel_active entries that would otherwise leak until connection close
    auto cancel_stream = [&]() {
        if (http_priv->quic_session && !http_priv->quic_session->isClosed()) {
            ExceptionSink tmp_xsink;
            http_priv->quic_session->cancelStream(stream_id, NGHTTP3_H3_REQUEST_CANCELLED, &tmp_xsink);
            // ignore cancelStream errors — we're already on an error path
        }
    };

    // Drive I/O to send the request and wait for response
    int64 deadline_ms = http_priv->timeout < 0 ? -1 : q_clock_getmillis() + http_priv->timeout;

    while (true) {
        if (http_priv->quic_session->isClosed()) {
            xsink->raiseException("HTTP3-CONNECT-ERROR",
                "QUIC connection closed while waiting for CONNECT response");
            http_priv->disconnectQuic();
            return nullptr;
        }

        int remaining_ms = 100;
        if (deadline_ms >= 0) {
            int64 now_ms = q_clock_getmillis();
            if (now_ms >= deadline_ms) {
                cancel_stream();
                xsink->raiseException("HTTP3-CONNECT-ERROR",
                    "timeout waiting for HTTP/3 CONNECT response (timeout: %d ms)",
                    (int)http_priv->timeout);
                return nullptr;
            }
            remaining_ms = std::min(100, static_cast<int>(deadline_ms - now_ms));
        }

        if (driveQuicIo(http_priv->quic_session.get(), http_priv->quic_fd,
                http_priv->quic_local_addr, http_priv->quic_local_addrlen,
                remaining_ms, xsink)) {
            http_priv->disconnectQuic();
            return nullptr;
        }

        // Check if SETTINGS revealed that server doesn't support extended CONNECT
        if (http_priv->quic_session->isExtendedConnectRejected()) {
            cancel_stream();
            xsink->raiseException("HTTP3-CONNECT-ERROR",
                "server does not support extended CONNECT "
                "(enable_connect_protocol not advertised in SETTINGS)");
            return nullptr;
        }

        // Check if we have a completed stream (CONNECT response headers received)
        if (http_priv->quic_session->hasCompletedStreams()) {
            break;
        }
    }

    // Get the completed stream
    std::unique_ptr<QuicStreamInfo> stream = http_priv->quic_session->takeCompletedStream();
    if (!stream) {
        cancel_stream();
        xsink->raiseException("HTTP3-CONNECT-ERROR",
            "no response received for CONNECT stream %" PRId64, stream_id);
        return nullptr;
    }

    // Verify we got the right stream
    if (stream->stream_id != stream_id) {
        cancel_stream();
        xsink->raiseException("HTTP3-CONNECT-ERROR",
            "unexpected stream %" PRId64 " in CONNECT response (expected %" PRId64 ")",
            stream->stream_id, stream_id);
        return nullptr;
    }

    // Build response hash
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(autoTypeInfo), xsink);
    rv->setKeyValue("status_code", stream->status_code, xsink);
    rv->setKeyValue("stream_id", stream_id, xsink);
    rv->setKeyValue("http_version", new QoreStringNode("3"), xsink);

    // Add response headers
    rv->setKeyValue("headers", httpMultiHeadersToQoreHash(stream->headers), xsink);

    // Add info if requested
    if (info) {
        info->setKeyValue("http3", true, xsink);
        info->setKeyValue("stream_id", stream_id, xsink);
        info->setKeyValue("status_code", stream->status_code, xsink);
    }

    // RFC 9220: 200 OK means CONNECT succeeded
    if (stream->status_code != 200) {
        cancel_stream();
        xsink->raiseException("HTTP3-CONNECT-ERROR",
            "HTTP/3 CONNECT request failed with status %d", stream->status_code);
        return nullptr;
    }

    return rv.release();
}

// Thread safety: all operations here are serialized under priv->m (SafeLocker).
// Http2Session methods (sendStreamData, sendPendingDataBlocking, receiveData,
// wantWrite) acquire their own Http2Session::m internally, but priv->m is
// always acquired first, maintaining a consistent lock order: priv->m → h2::m.
// SSL_read (receiveData) and SSL_write (sendPendingDataBlocking) are therefore
// serialized and cannot run concurrently for the same connection.
int QoreHttpClientObject::sendHttp2StreamData(int32_t stream_id, const BinaryNode* data,
        bool end_stream, int timeout_ms, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 is not active");
        return -1;
    }

    const void* ptr = data ? data->getPtr() : nullptr;
    size_t len = data ? data->size() : 0;

    int h2rv = http_priv->getH2Session()->sendStreamData(stream_id, ptr, len, end_stream, xsink);
    if (h2rv < 0) {
        return -1;
    }
    if (h2rv > 0) {
        xsink->raiseException("HTTP2-FLOW-CONTROL",
            "stream %d buffer full: data dropped", stream_id);
        return -1;
    }

    // Send pending data (use blocking version for client-side operations)
    int rv = http_priv->getH2Session()->sendPendingDataBlocking(timeout_ms, xsink);
    if (*xsink) {
        return rv;
    }

    // Process any incoming frames (non-blocking) to handle RST_STREAM,
    // WINDOW_UPDATE, etc. Without this, the client can deadlock when
    // the server resets a stream while the client is still sending body
    // data — the client never discovers the reset because it never reads.
    {
        ExceptionSink recv_xsink;
        http_priv->getH2Session()->receiveData(0, &recv_xsink);
        if (recv_xsink) {
            // Advisory receive probe failed — log but don't propagate, since
            // the primary send operation already succeeded
            printd(2, "sendHttp2StreamData() recv probe exception: %s: %s\n",
                recv_xsink.getExceptionErr().get<const QoreStringNode>()->c_str(),
                recv_xsink.getExceptionDesc().get<const QoreStringNode>()->c_str());
        }
        // Flush any frames generated by receive processing (e.g., ACKs,
        // WINDOW_UPDATEs) — only if receiveData actually generated frames
        if (!recv_xsink && http_priv->getH2Session()->wantWrite()) {
            http_priv->getH2Session()->sendPendingDataBlocking(100, &recv_xsink);
        }
    }

    return rv;
}

BinaryNode* QoreHttpClientObject::readHttp2StreamData(int32_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        xsink->raiseException("HTTP2-ERROR", "HTTP/2 is not active");
        return nullptr;
    }

    // First check if data is already in the buffer (from isHttp2DataAvailable)
    Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
    if (stream && !stream->body.empty()) {
        // Copy data to a new BinaryNode (append copies data internally)
        SimpleRefHolder<BinaryNode> rv(new BinaryNode());
        rv->append(stream->body.data(), stream->body.size());
        stream->body.clear();
        return rv.release();
    }

    // No data in buffer, try to receive data
    int recv_rv = http_priv->getH2Session()->receiveData(timeout_ms, xsink);
    if (recv_rv < 0) {
        return nullptr;
    }

    // Flush pending output (WINDOW_UPDATE frames generated by receiveData).
    // With no_auto_window_update, WINDOW_UPDATE is only sent when sendPendingData is called.
    {
        Http2Session* h2 = http_priv->getH2Session();
        if (h2) {
            ExceptionSink flush_xsink;
            h2->sendPendingDataBlocking(100, &flush_xsink);
            // Ignore flush errors — they don't affect the read path
        }
    }

    // Get stream data again
    Http2Session* h2_check = http_priv->getH2Session();
    stream = h2_check ? h2_check->getStream(stream_id) : nullptr;

    if (recv_rv == 1) {
        // Connection closed by peer (EOF) — return buffered data if any,
        // otherwise raise an exception so the caller knows the connection was lost
        if (stream && !stream->body.empty()) {
            SimpleRefHolder<BinaryNode> rv(new BinaryNode());
            rv->append(stream->body.data(), stream->body.size());
            stream->body.clear();
            return rv.release();
        }
        xsink->raiseException("HTTP2-EOF", "HTTP/2 connection closed by peer");
        return nullptr;
    }
    if (!stream || stream->body.empty()) {
        return nullptr;
    }

    // Copy data to a new BinaryNode (append copies data internally)
    SimpleRefHolder<BinaryNode> rv(new BinaryNode());
    rv->append(stream->body.data(), stream->body.size());
    stream->body.clear();
    return rv.release();
}

int32_t QoreHttpClientObject::getHttp2StreamId() const {
    return http_priv->h2_stream_id;
}

bool QoreHttpClientObject::hasHttp2StreamData(int32_t stream_id) const {
    SafeLocker sl(priv->m);
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        return false;
    }
    Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
    return stream && !stream->body.empty();
}

bool QoreHttpClientObject::isHttp2StreamClosed(int32_t stream_id) const {
    SafeLocker sl(priv->m);
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        return true;
    }
    return http_priv->getH2Session()->isStreamClosed(stream_id);
}

bool QoreHttpClientObject::isHttp3StreamClosed(int64_t stream_id) const {
    SafeLocker sl(priv->m);
    if (!http_priv->http3_active || !http_priv->quic_session) {
        return true;
    }
    return http_priv->quic_session->isStreamComplete(stream_id);
}

bool QoreHttpClientObject::isHttp2DataAvailable(int32_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    // Threading design:
    // - SSL objects are not thread-safe: SSL_peek/SSL_read and SSL_write must not run concurrently
    // - The sender thread (sendHttp2StreamData) holds priv->m when doing SSL_write
    // - Therefore ALL SSL operations here must be under priv->m
    // - The raw fd poll (asyncIoWait) uses poll()/select() only — no SSL — safe without locks
    //
    // Flow:
    // 1. Under lock: check HTTP/2 stream buffer and SSL/socket buffered data
    // 2. Without lock: raw fd poll (thread-safe, no SSL involvement)
    // 3. Under lock: process received HTTP/2 frames via SSL_read

    SafeLocker sl(priv->m);

    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        // Fall back to socket-level check if not in HTTP/2 mode
        return http_priv->msock->socket->isDataAvailable(xsink, timeout_ms);
    }

    // Step 1a: Check HTTP/2 stream buffer for already-decoded data
    Http2StreamInfo* stream = http_priv->getH2Session()->getStream(stream_id);
    if (stream && !stream->body.empty()) {
        return true;
    }

    // Step 1b: Check if socket read buffer or SSL layer has already-decrypted data
    // These checks are safe under priv->m (no concurrent SSL operations possible)
    Http2Session* h2_buffered = http_priv->getH2Session();
    if (h2_buffered && h2_buffered->hasSocketBufferedData()) {
        // Buffered data exists — process HTTP/2 frames immediately under lock
        Http2Session* h2 = h2_buffered;
        if (h2) {
            int rv = h2->receiveData(100, xsink);
            if (rv < 0 && *xsink) {
                return false;
            }
            if (rv == 1) {
                // EOF: connection closed by peer
                xsink->raiseException("HTTP2-EOF", "HTTP/2 connection closed by peer");
                return false;
            }
            // Flush pending output (WINDOW_UPDATE frames generated by receiveData)
            // With no_auto_window_update, WINDOW_UPDATE is queued by onDataChunkRecvCallback
            // but only sent when sendPendingData is called
            h2 = http_priv->getH2Session();
            if (h2) {
                h2->sendPendingDataBlocking(100, xsink);
                if (*xsink) {
                    xsink->clear();
                }
            }
            h2 = http_priv->getH2Session();
            if (h2) {
                stream = h2->getStream(stream_id);
                if (stream && !stream->body.empty()) {
                    return true;
                }
            }
        }
        return false;
    }

    // Step 2: No buffered data — poll the raw file descriptor for new data
    // asyncIoWait() uses poll()/select()/kqueue() on the raw fd only — no SSL operations
    // Safe to call without locks; the sender can freely do SSL_write() concurrently
    QoreSocket* sock = http_priv->msock->socket;
    sl.unlock();

    int rc = sock->asyncIoWait(timeout_ms, true, false);

    sl.lock();

    // Step 3: Re-check state after re-acquiring lock
    if (!http_priv->http2_active || !http_priv->getH2Session()) {
        return false;
    }

    // Step 3a: Always check SSL pending/socket buffer after re-acquiring lock.
    // While the lock was released, the sender's SSL_write() may have read incoming
    // TLS records into the SSL buffer (TLS renegotiation, key update, etc.).
    // Also covers the case where asyncIoWait() timed out but data is now available.
    if (rc <= 0) {
        // Poll timed out or errored — check if SSL/socket has buffered data
        Http2Session* h2_recheck = http_priv->getH2Session();
        if (h2_recheck && h2_recheck->hasSocketBufferedData()) {
            Http2Session* h2 = h2_recheck;
            if (h2) {
                int rv = h2->receiveData(100, xsink);
                if (rv < 0 && *xsink) {
                    return false;
                }
                if (rv == 1) {
                    // EOF: connection closed by peer
                    xsink->raiseException("HTTP2-EOF", "HTTP/2 connection closed by peer");
                    return false;
                }
                // Flush pending WINDOW_UPDATE
                h2 = http_priv->getH2Session();
                if (h2) {
                    h2->sendPendingDataBlocking(100, xsink);
                    if (*xsink) {
                        xsink->clear();
                    }
                }
                h2 = http_priv->getH2Session();
                if (h2) {
                    stream = h2->getStream(stream_id);
                    if (stream && !stream->body.empty()) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // Step 3b: Raw data detected — process HTTP/2 frames under lock
    Http2Session* h2_session = http_priv->getH2Session();
    int rv = h2_session->receiveData(100, xsink);

    if (rv < 0 && *xsink) {
        return false;
    }
    if (rv == 1) {
        // EOF: connection closed by peer
        xsink->raiseException("HTTP2-EOF", "HTTP/2 connection closed by peer");
        return false;
    }

    // Flush pending output (WINDOW_UPDATE frames generated by receiveData).
    // With no_auto_window_update, WINDOW_UPDATE is only sent when sendPendingData is called.
    // Without this flush, the server's flow control window stays at 0 and it cannot send data.
    h2_session = http_priv->getH2Session();
    if (h2_session) {
        h2_session->sendPendingDataBlocking(100, xsink);
        if (*xsink) {
            xsink->clear();
        }
    }

    // Re-check session after receiveData + sendPendingData
    h2_session = http_priv->getH2Session();
    if (!h2_session) {
        return false;
    }

    stream = h2_session->getStream(stream_id);
    return stream && !stream->body.empty();
}

// NOTE: Unlike sendHttp2StreamData(), this method does NOT need a separate
// non-blocking receive call to handle stream resets.  driveQuicIo() is
// inherently bidirectional — it reads incoming UDP packets (which may carry
// RESET_STREAM / STOP_SENDING) and writes outgoing ones in a single call.
// Therefore the HTTP/2 flow-control deadlock cannot occur over HTTP/3.
int QoreHttpClientObject::sendHttp3StreamData(int64_t stream_id, const BinaryNode* data,
        bool end_stream, int timeout_ms, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    if (!http_priv->http3_active || !http_priv->quic_session) {
        xsink->raiseException("HTTP3-ERROR", "HTTP/3 is not active");
        return -1;
    }

    const void* ptr = data ? data->getPtr() : nullptr;
    size_t len = data ? data->size() : 0;

    int rv = http_priv->quic_session->sendStreamData(stream_id, ptr, len, end_stream, xsink);
    if (*xsink || rv < 0) {
        return -1;
    }

    // Drive I/O to flush the data — also processes incoming frames (bidirectional)
    if (driveQuicIo(http_priv->quic_session.get(), http_priv->quic_fd,
            http_priv->quic_local_addr, http_priv->quic_local_addrlen,
            timeout_ms, xsink)) {
        return -1;
    }
    return 0;
}

BinaryNode* QoreHttpClientObject::readHttp3StreamData(int64_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    if (!http_priv->http3_active || !http_priv->quic_session) {
        xsink->raiseException("HTTP3-ERROR", "HTTP/3 is not active");
        return nullptr;
    }

    // First check if data is already in the connect stream buffer
    {
        QoreValue val = http_priv->quic_session->readConnectStreamData(stream_id, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (val.getType() == NT_BINARY) {
            return static_cast<BinaryNode*>(val.takeIfNode());
        }
    }

    // No data available — drive I/O and try again
    if (driveQuicIo(http_priv->quic_session.get(), http_priv->quic_fd,
            http_priv->quic_local_addr, http_priv->quic_local_addrlen,
            timeout_ms, xsink)) {
        return nullptr;
    }

    {
        QoreValue val = http_priv->quic_session->readConnectStreamData(stream_id, xsink);
        if (*xsink) {
            return nullptr;
        }
        if (val.getType() == NT_BINARY) {
            return static_cast<BinaryNode*>(val.takeIfNode());
        }
    }
    return nullptr;
}

int QoreHttpClientObject::setProxyURL(const char* proxy, ExceptionSink* xsink)  {
    SafeLocker sl(priv->m);

    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    http_priv->disconnect_unlocked();
    if (!proxy || !proxy[0]) {
        http_priv->proxy_connection.clear();
        return 0;
    }
    return http_priv->setProxyUrlUnlocked(proxy, xsink);
}

QoreStringNode* QoreHttpClientObject::getProxyURL()  {
    SafeLocker sl(priv->m);

    if (!http_priv->proxy_connection.has_url())
        return nullptr;

    return http_priv->proxy_connection.get_url();
}

QoreStringNode* QoreHttpClientObject::getSafeProxyURL()  {
    SafeLocker sl(priv->m);

    if (!http_priv->proxy_connection.has_url()) {
        return nullptr;
    }

    return http_priv->proxy_connection.get_url(URL_MASK_PASSWORD);
}

void QoreHttpClientObject::clearProxyURL() {
    SafeLocker sl(priv->m);
    http_priv->proxy_connection.clear();
    http_priv->setSocketPath(http_priv->connection);
}

QoreStringNode* QoreHttpClientObject::getUsername() const {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.username.empty()) {
        return new QoreStringNode(http_priv->connection.username);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getPassword() const {
    SafeLocker sl(priv->m);
    if (!http_priv->connection.password.empty()) {
        return new QoreStringNode(http_priv->connection.password);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getProxyUsername() const {
    SafeLocker sl(priv->m);
    if (!http_priv->proxy_connection.username.empty()) {
        return new QoreStringNode(http_priv->proxy_connection.username);
    }
    return nullptr;
}

QoreStringNode* QoreHttpClientObject::getProxyPassword() const {
    SafeLocker sl(priv->m);
    if (!http_priv->proxy_connection.password.empty()) {
        return new QoreStringNode(http_priv->proxy_connection.password);
    }
    return nullptr;
}

void QoreHttpClientObject::setSecure(bool is_secure) {
    lock();
    http_priv->connection.ssl = is_secure;
    unlock();
}

bool QoreHttpClientObject::isSecure() const {
    return http_priv->connection.ssl;
}

void QoreHttpClientObject::setProxySecure(bool is_secure) {
    lock();
    http_priv->proxy_connection.ssl = is_secure;
    unlock();
}

bool QoreHttpClientObject::isProxySecure() const {
    return http_priv->proxy_connection.ssl;
}

int QoreHttpClientObject::connect(ExceptionSink* xsink) {
    SafeLocker sl(priv->m);

    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    http_priv->disconnect_unlocked();
    return http_priv->connect_unlocked(xsink, http_priv->connection);
}

void QoreHttpClientObject::disconnect() {
    SafeLocker sl(priv->m);
    http_priv->disconnect_unlocked();
}

QoreHashNode* qore_httpclient_priv::sendMessageAndGetResponse(con_info& connection, const char* mname,
        const char* meth, const char* mpath,
        const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback, QoreHashNode* info, bool with_connect, int timeout_ms,
        int& code, bool& aborted, bool path_already_encoded, ExceptionSink* xsink) {
    // issue #3978: make sure and reset output encoding if any is set
    if (enc) {
        msock->socket->setEncoding(enc);
    }

    QoreString pathstr(msock->socket->getEncoding());
    const char* msgpath = with_connect ? mpath : getMsgPath(xsink, connection, mpath, pathstr, path_already_encoded);
    if (*xsink) {
        return nullptr;
    }

    if (!msock->socket->isOpen()) {
        if (persistent) {
            xsink->raiseException("PERSISTENCE-ERROR", "the current connection has been temporarily marked as "
                "persistent, but has been disconnected");
            return nullptr;
        }

        if (connect_unlocked(xsink, connection)) {
            // if we have an info hash then write the request-uri key for reporting/logging purposes
            if (info) {
                info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/%s", meth,
                    msgpath && msgpath[0] ? msgpath : "/", http11 ? "1.1" : "1.0"), 0);
            }
            return nullptr;
        }
    }

    // Use HTTP/3 if already active
    if (http3_active && quic_session) {
        return sendHttp3MessageAndGetResponse(mname, meth, msgpath, nh, body, data, size,
            send_callback, is, max_chunk_size, trailer_callback,
            info, timeout_ms, code, xsink);
    }

    // Try HTTP/3 upgrade if Alt-Svc is cached.  On QUIC failure, the entry
    // is marked with a 5-minute backoff so we don't retry on every request
    // (the penalty is a blocking handshake timeout per attempt).
    if (!*xsink && !http3_active && http3_mode != HTTP3_MODE_DISABLED && connection.ssl) {
        auto alt = lookupAltSvc(connection.host.c_str(), connection.port);
        if (alt.has_value() || http3_mode == HTTP3_MODE_REQUIRED) {
            if (!connectQuic(xsink, connection)) {
                return sendHttp3MessageAndGetResponse(mname, meth, msgpath, nh, body, data, size,
                    send_callback, is, max_chunk_size, trailer_callback,
                    info, timeout_ms, code, xsink);
            }
            if (http3_mode == HTTP3_MODE_REQUIRED) {
                return nullptr;
            }
            // QUIC connect failed — mark the Alt-Svc entry with a 5-minute
            // backoff so we don't retry on every request (RFC 9369 §3.2:
            // clients SHOULD cache failures and avoid repeated attempts).
            markAltSvcFailed(connection.host.c_str(), connection.port);
            // Log QUIC error details before clearing for HTTP/1.x fallback.
            // xsink->clear() is intentional: the QUIC failure is non-fatal when
            // http3_mode == HTTP3_MODE_AUTO — we retry with TCP/TLS below.
            {
                QoreStringValueHelper err(xsink->getExceptionErr());
                QoreStringValueHelper desc(xsink->getExceptionDesc());
                printd(2, "QoreHttpClientObject: QUIC connection failed (%s: %s), "
                    "falling back to HTTP/1.x\n", err->c_str(), desc->c_str());
            }
            xsink->clear();
        }
    }

    // Use HTTP/2 if active (after HTTP/3 upgrade attempt above)
    if (http2_active && getH2Session()) {
        return sendHttp2MessageAndGetResponse(mname, meth, msgpath, nh, body, data, size,
            send_callback, is, max_chunk_size, trailer_callback,
            info, timeout_ms, code, aborted, xsink);
    }

    // send the message (HTTP/1.x path)
    int rc = msock->socket->priv->sendHttpMessage(xsink, info, "HTTPClient", mname, meth, msgpath,
        http11 ? "1.1" : "1.0", &nh, body, data, size, send_callback, is, max_chunk_size, trailer_callback,
        QORE_SOURCE_HTTPCLIENT, timeout_ms, &msock->m, &aborted);

    //printd(5, "qore_httpclient_priv::sendMessageAndGetResponse() '%s' path: '%s' data: %p size: %d "
    //    "send_callback: %p is: %p aborted: %d rc: %d\n", meth, msgpath, data, (int)size, send_callback, is,
    //    aborted, rc);

    // do not exit immediately if the transfer was aborted with a streaming send unless the socket was already closed
    if (rc && (!send_callback || !aborted || !msock->socket->isOpen())) {
        assert(*xsink);
        if (rc == QSE_NOT_OPEN) {
            disconnect_unlocked();
        }
        return nullptr;
    }

    // issue #3564 in case an outbound chunked transfer was aborted and we have incoming data,
    // change the timeout to 5 seconds to avoid stalling I/O in case of a slow or incomplete response,
    // because anyway the connection must be aborted; we just try to quickly read any possible incoming
    // HTTP response for error reporting
    if (aborted && (timeout < 0 || timeout > ABORTED_TIMEOUT_MS)) {
        timeout = ABORTED_TIMEOUT_MS;
        //printd(5, "qore_httpclient_priv::sendMessageAndGetResponse() aborted: %d timeout: %d open: %d\n", aborted,
        //    timeout, msock->socket->isOpen());
    }

    // if the transfer was aborted with a streaming send, but the socket is still open, then try to read a response
    ReferenceHolder<QoreHashNode> response_hash(xsink);
    while (true) {
        qore_offset_t rc;
        response_hash = msock->socket->priv->readHTTPHeader(xsink, info, timeout, rc, QORE_SOURCE_HTTPCLIENT,
            "response-headers-raw");
        if (!(*response_hash)) {
            disconnect_unlocked();
            assert(*xsink);
            return nullptr;
        }

        // check HTTP status code
        QoreValue v = response_hash->getKeyValue("status_code");
        if (v.isNothing()) {
            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR", "no HTTP status code received in response");
            return nullptr;
        }

        code = (int)v.getAsBigInt();
        // continue processing if "100 Continue" response received (ignore this response)
        if (code == 100) {
            continue;
        }

        break;
    }

    // only clear exceptions if a streaming (ie chunked) send was aborted and we really got a response from the remote
    if (*xsink) {
        assert(aborted);
        xsink->clear();
    }

    // Parse Alt-Svc header from HTTP/1.x response for future HTTP/3 upgrades
    if (http3_mode != HTTP3_MODE_DISABLED && *response_hash) {
        QoreValue alt_svc_val = response_hash->getKeyValue("alt-svc");
        if (alt_svc_val.getType() == NT_STRING) {
            parseAltSvc(alt_svc_val.get<const QoreStringNode>()->c_str(),
                connection.host.c_str(), connection.port);
        }
    }

    return response_hash.release();
}

QoreHashNode* qore_httpclient_priv::sendHttp2MessageAndGetResponse(const char* mname, const char* meth,
        const char* mpath, const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback,
        QoreHashNode* info, int timeout_ms, int& code, bool& aborted, ExceptionSink* xsink) {
    Http2Session* h2_session = getH2Session();
    assert(h2_session);

    // Convert request headers to std::map
    strcase_str_map_t headers;
    ConstHashIterator hi(nh);
    while (hi.next()) {
        const char* key = hi.getKey();
        QoreValue val = hi.get();
        if (val.getType() == NT_STRING) {
            headers[key] = val.get<const QoreStringNode>()->c_str();
        } else if (val.getType() == NT_LIST) {
            // RFC 9113 Section 8.2.3: combine multi-value headers with comma
            const QoreListNode* l = val.get<const QoreListNode>();
            std::string joined;
            for (size_t i = 0; i < l->size(); ++i) {
                QoreValue elem = l->retrieveEntry(i);
                if (elem.getType() == NT_STRING) {
                    if (!joined.empty()) {
                        joined += ", ";
                    }
                    joined += elem.get<const QoreStringNode>()->c_str();
                }
            }
            if (!joined.empty()) {
                headers[key] = std::move(joined);
            }
        }
    }

    // Convert Host to :authority pseudo-header (RFC 7540 Section 8.1.2.3)
    auto host_it = headers.find("Host");
    if (host_it != headers.end()) {
        headers[":authority"] = host_it->second;
        headers.erase(host_it);
    }

    // Determine streaming mode
    bool streaming = send_callback || is;

    // Get request body data (not used in streaming mode)
    const void* body_data = nullptr;
    size_t body_len = 0;
    if (!streaming) {
        body_data = data;
        body_len = size;
        if (!body_data && body) {
            body_data = body->c_str();
            body_len = body->size();
        }
    }

    // Set Content-Length if body present and header not already set (non-streaming only)
    if (body_len > 0) {
        auto cl_it = headers.find("content-length");
        if (cl_it == headers.end()) {
            headers["content-length"] = std::to_string(body_len);
        }
    }

    // Add ";charset=xxx" to Content-Type for non-ISO-8859-1 text bodies
    if (!streaming && body && body_len > 0) {
        auto ct_it = headers.find("content-type");
        if (ct_it != headers.end()) {
            // Only add charset if not already present and not a boundary type
            if (ct_it->second.find("charset=") == std::string::npos
                    && ct_it->second.find("boundary=") == std::string::npos) {
                const QoreEncoding* enc = msock->socket->getEncoding();
                if (enc != QCS_ISO_8859_1) {
                    QoreString code(enc->getCode());
                    code.tolwr();
                    ct_it->second += ";charset=";
                    ct_it->second += code.c_str();
                }
            }
        }
    }

    // Record request in info if provided
    if (info) {
        info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/2", meth, mpath), nullptr);
    }

    // Submit the HTTP/2 request — streaming=true sets up deferred data provider
    int32_t stream_id = h2_session->submitRequest(meth, mpath, headers, body_data, body_len, xsink, streaming);
    if (stream_id < 0) {
        return nullptr;
    }

    ASYNC_IO_TRACE("H2 CLIENT submitRequest stream=%d\n", stream_id);

    // Send pending data (the request headers) - use blocking version for client-side operations
    if (h2_session->sendPendingDataBlocking(timeout_ms, xsink)) {
        return nullptr;
    }

    ASYNC_IO_TRACE("H2 CLIENT sendPendingDataBlocking done stream=%d\n", stream_id);

    // Streaming body: feed chunks from send_callback or InputStream
    int64 deadline_ms = timeout_ms < 0 ? -1 : q_clock_getmillis() + timeout_ms;
    if (streaming) {
        if (send_callback) {
            // Chunk-feed loop using send_callback
            bool done = false;
            while (!done) {
                // Check timeout
                if (deadline_ms >= 0 && q_clock_getmillis() >= deadline_ms) {
                    xsink->raiseException("HTTP2-TIMEOUT",
                        "timeout sending HTTP/2 streaming request body (timeout: %d ms)", timeout_ms);
                    h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
                    return nullptr;
                }

                // Non-blocking receive probe: process any pending server frames
                // (e.g. an error response) before calling the send_callback
                h2_session->receiveData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                // Check if the server sent a complete response for this stream
                if (h2_session->isStreamComplete(stream_id)) {
                    aborted = true;
                    break;
                }

                // Call the callback (no lock held — safe for arbitrary Qore code)
                ValueHolder res(send_callback->execValue(nullptr, xsink), xsink);
                if (*xsink) {
                    h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
                    return nullptr;
                }

                // Process callback return value
                const void* chunk_data = nullptr;
                size_t chunk_len = 0;
                switch (res->getType()) {
                    case NT_STRING: {
                        const QoreStringNode* str = res->get<const QoreStringNode>();
                        if (str->empty()) {
                            done = true;
                        } else {
                            chunk_data = str->c_str();
                            chunk_len = str->size();
                        }
                        break;
                    }
                    case NT_BINARY: {
                        const BinaryNode* b = res->get<const BinaryNode>();
                        if (b->empty()) {
                            done = true;
                        } else {
                            chunk_data = b->getPtr();
                            chunk_len = b->size();
                        }
                        break;
                    }
                    case NT_NOTHING:
                    case NT_NULL:
                        done = true;
                        break;
                    default:
                        xsink->raiseException("HTTP2-CALLBACK-ERROR",
                            "send_callback returned type '%s'; expected 'string', 'binary', or NOTHING",
                            res->getTypeName());
                        h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
                        return nullptr;
                }

                if (chunk_data && chunk_len > 0) {
                    // Send chunk data with backpressure handling
                    while (true) {
                        int srv = h2_session->sendStreamData(stream_id, chunk_data, chunk_len, false, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        if (srv == 0) {
                            break;  // data accepted
                        }
                        if (srv == 1) {
                            // Buffer full — drive I/O to drain and retry
                            int remaining_ms = -1;
                            if (deadline_ms >= 0) {
                                remaining_ms = static_cast<int>(deadline_ms - q_clock_getmillis());
                                if (remaining_ms <= 0) {
                                    xsink->raiseException("HTTP2-TIMEOUT",
                                        "timeout during HTTP/2 streaming body backpressure");
                                    return nullptr;
                                }
                            }
                            h2_session->receiveData(remaining_ms, xsink);
                            if (*xsink) {
                                return nullptr;
                            }
                            // Check for early server response during backpressure
                            if (h2_session->isStreamComplete(stream_id)) {
                                aborted = true;
                                break;
                            }
                            h2_session->sendPendingDataBlocking(remaining_ms, xsink);
                            if (*xsink) {
                                return nullptr;
                            }
                            continue;
                        }
                        return nullptr;  // error
                    }
                    if (aborted) {
                        break;
                    }
                    // Flush the chunk
                    h2_session->sendPendingDataBlocking(timeout_ms, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    // Check for early server response after flush
                    if (h2_session->isStreamComplete(stream_id)) {
                        aborted = true;
                        break;
                    }
                }
            }
        } else if (is) {
            // Chunk-feed loop using InputStream
            size_t chunk_size = max_chunk_size > 0 ? max_chunk_size : 65536;
            while (true) {
                // Check timeout
                if (deadline_ms >= 0 && q_clock_getmillis() >= deadline_ms) {
                    xsink->raiseException("HTTP2-TIMEOUT",
                        "timeout sending HTTP/2 streaming request body (timeout: %d ms)", timeout_ms);
                    h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
                    return nullptr;
                }

                // Non-blocking receive probe before reading InputStream
                h2_session->receiveData(0, xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (h2_session->isStreamComplete(stream_id)) {
                    aborted = true;
                    break;
                }

                // Read from InputStream (no lock held — safe for arbitrary Qore code)
                SimpleRefHolder<BinaryNode> buf(is->readHelper(chunk_size, xsink));
                if (*xsink) {
                    h2_session->submitRstStream(stream_id, NGHTTP2_CANCEL, xsink);
                    return nullptr;
                }
                if (!buf) {
                    break;  // EOF
                }

                // Send the chunk with backpressure handling
                while (true) {
                    int srv = h2_session->sendStreamData(stream_id, buf->getPtr(), buf->size(), false, xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    if (srv == 0) {
                        break;
                    }
                    if (srv == 1) {
                        int remaining_ms = -1;
                        if (deadline_ms >= 0) {
                            remaining_ms = static_cast<int>(deadline_ms - q_clock_getmillis());
                            if (remaining_ms <= 0) {
                                xsink->raiseException("HTTP2-TIMEOUT",
                                    "timeout during HTTP/2 streaming body backpressure");
                                return nullptr;
                            }
                        }
                        h2_session->receiveData(remaining_ms, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        if (h2_session->isStreamComplete(stream_id)) {
                            aborted = true;
                            break;
                        }
                        h2_session->sendPendingDataBlocking(remaining_ms, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        continue;
                    }
                    return nullptr;
                }
                if (aborted) {
                    break;
                }
                h2_session->sendPendingDataBlocking(timeout_ms, xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }

        // Skip trailers and EOF when send was aborted by early server response
        if (!aborted) {
            // Handle trailers if provided
            if (trailer_callback) {
                ValueHolder trailer_val(trailer_callback->execValue(nullptr, xsink), xsink);
                if (*xsink) {
                    return nullptr;
                }
                if (trailer_val->getType() == NT_HASH) {
                    const QoreHashNode* trailers = trailer_val->get<const QoreHashNode>();
                    strcase_str_map_t trailer_map;
                    ConstHashIterator ti(*trailers);
                    while (ti.next()) {
                        QoreValue v = ti.get();
                        if (v.getType() == NT_STRING) {
                            trailer_map[ti.getKey()] = v.get<const QoreStringNode>()->c_str();
                        }
                    }
                    if (!trailer_map.empty()) {
                        h2_session->submitTrailers(stream_id, trailer_map, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                        h2_session->sendPendingDataBlocking(timeout_ms, xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                    }
                }
            }

            // Signal EOF (no trailers) — close the stream for writing
            if (!trailer_callback) {
                h2_session->sendStreamData(stream_id, nullptr, 0, true, xsink);
                if (*xsink) {
                    return nullptr;
                }
                h2_session->sendPendingDataBlocking(timeout_ms, xsink);
                if (*xsink) {
                    return nullptr;
                }
            }
        }
    }

    // Ensure the request body is fully sent, driving flow control if necessary
    if (!aborted && body_len > 0) {
        while (h2_session->hasPendingBodyData(stream_id)) {
            int remaining_ms = -1;
            if (deadline_ms >= 0) {
                int64 now_ms = q_clock_getmillis();
                if (now_ms >= deadline_ms) {
                    xsink->raiseException("HTTP2-TIMEOUT", "timeout sending HTTP/2 request body (timeout: %d ms)", (int)timeout_ms);
                    return nullptr;
                }
                remaining_ms = static_cast<int>(deadline_ms - now_ms);
            }
            int rv = h2_session->receiveData(remaining_ms, xsink);
            if (rv < 0 || *xsink) {
                return nullptr;
            }
            if (rv == 1) {
                xsink->raiseException("HTTP2-ERROR",
                    "HTTP/2 connection closed by peer while sending request body");
                return nullptr;
            }
            if (h2_session->sendPendingDataBlocking(remaining_ms, xsink)) {
                return nullptr;
            }
        }
    }
    if (!aborted) {
        while (h2_session->wantWrite() || h2_session->hasPendingData()) {
            int remaining_ms = -1;
            if (deadline_ms >= 0) {
                int64 now_ms = q_clock_getmillis();
                if (now_ms >= deadline_ms) {
                    xsink->raiseException("HTTP2-TIMEOUT", "timeout waiting to send HTTP/2 request data (timeout: %d ms)", (int)timeout_ms);
                    return nullptr;
                }
                remaining_ms = static_cast<int>(deadline_ms - now_ms);
            }
            // Non-blocking drain: only read incoming flow-control frames (WINDOW_UPDATE)
            // that might unblock the send.  Using remaining_ms here would block the
            // client for the full timeout before sending any request data.
            int rv = h2_session->receiveData(0, xsink);
            if (rv < 0 || *xsink) {
                return nullptr;
            }
            if (rv == 1) {
                xsink->raiseException("HTTP2-ERROR",
                    "HTTP/2 connection closed by peer while sending request data");
                return nullptr;
            }
            if (h2_session->sendPendingDataBlocking(remaining_ms, xsink)) {
                return nullptr;
            }
        }
    }

    // Flush any pending SETTINGS_ACK / WINDOW_UPDATE before entering the blocking
    // receive loop.
    ASYNC_IO_TRACE("H2 CLIENT pre-loop flush stream=%d\n", stream_id);
    {
        int rv = h2_session->receiveData(0, xsink);
        if (rv < 0 || *xsink) {
            return nullptr;
        }
        if (h2_session->sendPendingDataBlocking(0, xsink)) {
            return nullptr;
        }
    }

    // Receive data until we have a complete response
    while (!h2_session->hasCompletedStreams()) {
        int remaining_ms = -1;
        if (deadline_ms >= 0) {
            int64 now_ms = q_clock_getmillis();
            if (now_ms >= deadline_ms) {
                xsink->raiseException("HTTP2-TIMEOUT", "timeout waiting for HTTP/2 response (timeout: %d ms)", (int)timeout_ms);
                return nullptr;
            }
            remaining_ms = static_cast<int>(deadline_ms - now_ms);
        }
        int rv = h2_session->receiveData(remaining_ms, xsink);
        // Send any pending data (e.g., WINDOW_UPDATE, ACKs) after every receive.
        // This is critical for HTTP/2 flow control — without WINDOW_UPDATE frames,
        // the server will stop sending data when the receive window is exhausted.
        // Recalculate remaining time: receiveData() may have consumed most of the
        // timeout, leaving sendPendingDataBlocking() with a stale value.
        {
            int send_remaining_ms = -1;
            if (deadline_ms >= 0) {
                send_remaining_ms = static_cast<int>(deadline_ms - q_clock_getmillis());
                if (send_remaining_ms < 0) {
                    send_remaining_ms = 0;
                }
            }
            h2_session->sendPendingDataBlocking(send_remaining_ms, xsink);
        }
        if (*xsink) {
            return nullptr;
        }
        if (rv == 0 && remaining_ms >= 0) {
            // no data yet; loop until the overall deadline expires
            continue;
        }
        if (rv) {
            if (*xsink) {
                return nullptr;
            }
            // Check if session is in GOAWAY state
            if (h2_session->isGoawayReceived()) {
                xsink->raiseException("HTTP2-GOAWAY", "server sent GOAWAY frame");
                return nullptr;
            }
            break;
        }
    }

    // Get the completed stream
    std::unique_ptr<Http2StreamInfo> stream = h2_session->takeCompletedStream();
    if (!stream) {
        xsink->raiseException("HTTP2-ERROR", "no response received for stream %d", stream_id);
        return nullptr;
    }

    code = stream->status_code;

    // HTTP/2: status code 0 means no :status pseudo-header was received;
    // this typically indicates the connection was reset or is stale
    if (code == 0) {
        // Build minimal response hash for the exception arg
        ReferenceHolder<QoreHashNode> err_response(new QoreHashNode(autoTypeInfo), xsink);
        err_response->setKeyValue("status_code", 0, xsink);
        err_response->setKeyValue("http_version", new QoreStringNode("2"), xsink);
        if (stream->reset) {
            err_response->setKeyValue("status_message", new QoreStringNode("Stream Reset"), xsink);
            xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", err_response.release(),
                "HTTP/2 stream %d was reset by the remote end (HTTP/2 error code %d); "
                "the connection is stale and must be re-established",
                stream_id, (int)stream->error_code);
        } else {
            err_response->setKeyValue("status_message",
                new QoreStringNode("No Response"), xsink);
            xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", err_response.release(),
                "HTTP/2 stream %d closed without a response (no :status header received); "
                "the connection is likely stale and must be re-established",
                stream_id);
        }
        return nullptr;
    }

    // Build response hash similar to HTTP/1.x format
    ReferenceHolder<QoreHashNode> response(new QoreHashNode(autoTypeInfo), xsink);

    // Add status code and message
    response->setKeyValue("status_code", code, xsink);

    // Map status codes to messages (HTTP/2 only transmits status code, not message)
    const char* status_msg = get_http_status_message(code);
    response->setKeyValue("status_message", new QoreStringNode(status_msg), xsink);
    response->setKeyValue("http_version", new QoreStringNode("2"), xsink);

    // Add response headers (handle duplicate headers per RFC 7540)
    // Merge into the response hash directly (headers are at the top level)
    {
        ReferenceHolder<QoreHashNode> h2h(httpMultiHeadersToQoreHash(stream->headers), xsink);
        ConstHashIterator hi(*(*h2h));
        while (hi.next()) {
            response->setKeyValue(hi.getKey(), hi.get().refSelf(), xsink);
        }
    }

    // Add body if present (must copy data as stream->body will be freed)
    if (!stream->body.empty()) {
        BinaryNode* body = new BinaryNode();
        body->append(stream->body.data(), stream->body.size());
        response->setKeyValue("body", body, xsink);
    }

    // Parse Alt-Svc header from HTTP/2 response for future HTTP/3 upgrades
    if (http3_mode != HTTP3_MODE_DISABLED) {
        auto alt_svc_it = stream->headers.find("alt-svc");
        if (alt_svc_it != stream->headers.end() && !alt_svc_it->second.empty()) {
            parseAltSvc(alt_svc_it->second[0].c_str(), connection.host.c_str(), connection.port);
        }
    }

    if (info) {
        info->setKeyValue("response-headers", response->refSelf(), xsink);
        // Set response-uri for compatibility with HTTP/1.x clients (e.g., rest -l)
        QoreStringNode* response_uri = new QoreStringNode();
        response_uri->sprintf("HTTP/2 %d %s", code, status_msg);
        info->setKeyValue("response-uri", response_uri, xsink);
    }

    return response.release();
}

void qore_httpclient_priv::parseAltSvc(const char* value, const char* host, int port) {
    // Parse Alt-Svc header: h3=":443"; ma=3600, h3-29=":443"; ma=3600
    // We only care about h3= entries (not h3-29 or other drafts)
    // NOTE: alternate hostnames (e.g. h3="other.host:443") are ignored per RFC 7838;
    // only the port is extracted.  CDN redirect scenarios would need hostname support.
    const char* p = value;
    while (*p) {
        // Skip whitespace
        while (*p && isspace(*p)) {
            ++p;
        }
        if (!*p) {
            break;
        }

        // Check for "clear" directive
        if (!strncmp(p, "clear", 5)) {
            // Clear all Alt-Svc entries for this origin
            std::string origin = std::string(host) + ":" + std::to_string(port);
            alt_svc_cache.erase(origin);
            return;
        }

        // Parse protocol-id=alt-authority
        const char* proto_start = p;
        while (*p && *p != '=') {
            ++p;
        }
        if (!*p) {
            break;
        }
        std::string proto(proto_start, p - proto_start);
        ++p; // skip '='

        // Parse quoted alt-authority: ":<port>"
        if (*p != '"') {
            // Skip to next entry
            while (*p && *p != ',') {
                ++p;
            }
            if (*p == ',') {
                ++p;
            }
            continue;
        }
        ++p; // skip opening quote

        // Parse ":port"
        int alt_port = 0;
        if (*p == ':') {
            ++p;
            long val = strtol(p, nullptr, 10);
            alt_port = (val > 0 && val <= 65535) ? static_cast<int>(val) : 0;
            while (*p && *p != '"') {
                ++p;
            }
        } else {
            // Parse host:port format (RFC 7838 alt-authority)
            int alt_port_val = 0;
            if (*p == '[') {
                // IPv6 bracketed: [addr]:port
                while (*p && *p != ']' && *p != '"') {
                    ++p;
                }
                if (*p == ']') {
                    ++p;
                    if (*p == ':') {
                        long val = strtol(p + 1, nullptr, 10);
                        alt_port_val = (val > 0 && val <= 65535) ? static_cast<int>(val) : 0;
                    }
                }
                while (*p && *p != '"') {
                    ++p;
                }
            } else {
                const char* colon = nullptr;
                while (*p && *p != '"') {
                    if (*p == ':') {
                        colon = p;
                    }
                    ++p;
                }
                if (colon) {
                    long val = strtol(colon + 1, nullptr, 10);
                    alt_port_val = (val > 0 && val <= 65535) ? static_cast<int>(val) : 0;
                }
            }
            alt_port = alt_port_val;
        }
        if (*p == '"') {
            ++p;
        }

        // Parse optional parameters (ma=, persist=, etc.)
        int64 max_age = 86400; // default: 24 hours per RFC 7838
        while (*p && *p != ',') {
            // Skip whitespace and semicolons
            while (*p && (*p == ';' || isspace(*p))) {
                ++p;
            }
            if (!strncmp(p, "ma=", 3)) {
                p += 3;
                long long val = strtoll(p, nullptr, 10);
                // Clamp to [0, 86400*100] per RFC 7838 (max ~100 days)
                if (val < 0) {
                    val = 0;
                } else if (val > 8640000) {
                    val = 8640000;
                }
                max_age = val;
                while (*p && *p != ';' && *p != ',') {
                    ++p;
                }
            } else {
                // Skip unknown parameter
                while (*p && *p != ';' && *p != ',') {
                    ++p;
                }
            }
        }
        if (*p == ',') {
            ++p;
        }

        // Only store h3 (not h3-29 or other draft versions)
        if (proto == "h3" && alt_port > 0) {
            std::string origin = std::string(host) + ":" + std::to_string(port);
            auto it = alt_svc_cache.find(origin);
            if (it != alt_svc_cache.end()) {
                // Update port and expiry but preserve retry_after backoff
                it->second.port = alt_port;
                it->second.expiry_epoch = q_epoch() + max_age;
            } else {
                alt_svc_cache[origin] = AltSvcEntry{alt_port, q_epoch() + max_age, 0};
            }
        }
    }
}

std::optional<qore_httpclient_priv::AltSvcEntry> qore_httpclient_priv::lookupAltSvc(const char* host, int port) {
    std::string origin = std::string(host) + ":" + std::to_string(port);
    auto it = alt_svc_cache.find(origin);
    if (it == alt_svc_cache.end()) {
        return std::nullopt;
    }
    // Check if entry has expired
    if (it->second.expiry_epoch <= q_epoch()) {
        alt_svc_cache.erase(it);
        return std::nullopt;
    }
    // Check if retry backoff is active (after QUIC failure)
    if (it->second.retry_after_epoch > q_epoch()) {
        return std::nullopt;
    }
    return it->second;
}

// Synchronous QUIC connect + handshake for the HTTPClient send path.
// This is analogous to TCP connect + TLS handshake: it blocks under msock->m
// until the handshake completes or times out.  The asynchronous (poll-based)
// path uses SocketQuicClientPollOperation instead.
int qore_httpclient_priv::connectQuic(ExceptionSink* xsink, con_info& connection) {
    // Resolve server address
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    std::string port_str = std::to_string(connection.port);
    // Check Alt-Svc cache for the QUIC port
    auto alt = lookupAltSvc(connection.host.c_str(), connection.port);
    if (alt.has_value()) {
        port_str = std::to_string(alt->port);
    }

    struct addrinfo* res = nullptr;
    int rv = getaddrinfo(connection.host.c_str(), port_str.c_str(), &hints, &res);
    if (rv != 0) {
        xsink->raiseException("HTTP3-CONNECT-ERROR", "failed to resolve host '%s': %s",
            connection.host.c_str(), gai_strerror(rv));
        return -1;
    }
    ON_BLOCK_EXIT(freeaddrinfo, res);

    // Create UDP socket; set close-on-exec to prevent FD leak on fork/exec
#ifdef SOCK_CLOEXEC
    int fd = socket(res->ai_family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
#else
    int fd = socket(res->ai_family, SOCK_DGRAM, IPPROTO_UDP);
#endif
    if (fd < 0) {
        xsink->raiseException("HTTP3-CONNECT-ERROR", "failed to create UDP socket: %s", strerror(errno));
        return -1;
    }
#ifndef SOCK_CLOEXEC
    fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

    // Bind to ephemeral port
    struct sockaddr_storage local_addr{};
    socklen_t local_addrlen;
    if (res->ai_family == AF_INET6) {
        auto* addr = reinterpret_cast<struct sockaddr_in6*>(&local_addr);
        addr->sin6_family = AF_INET6;
        addr->sin6_addr = in6addr_any;
        addr->sin6_port = 0;
        local_addrlen = sizeof(struct sockaddr_in6);
    } else {
        auto* addr = reinterpret_cast<struct sockaddr_in*>(&local_addr);
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = INADDR_ANY;
        addr->sin_port = 0;
        local_addrlen = sizeof(struct sockaddr_in);
    }

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&local_addr), local_addrlen) < 0) {
        xsink->raiseException("HTTP3-CONNECT-ERROR", "failed to bind UDP socket: %s", strerror(errno));
        ::close(fd);
        return -1;
    }

    // Connect to the remote server so the kernel selects the correct local
    // interface and getsockname() returns a specific address (not wildcard).
    // This is standard for QUIC clients (see ngtcp2 examples).
    // Also filters incoming packets to only those from the connected peer.
    if (::connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        xsink->raiseException("HTTP3-CONNECT-ERROR", "UDP connect() failed: %s", strerror(errno));
        ::close(fd);
        return -1;
    }

    // Get actual local address after connect
    if (getsockname(fd, reinterpret_cast<struct sockaddr*>(&local_addr), &local_addrlen) < 0) {
        xsink->raiseException("HTTP3-CONNECT-ERROR", "getsockname() failed: %s", strerror(errno));
        ::close(fd);
        return -1;
    }
    // Enable per-packet destination address reporting for multi-homed host support
    // Non-fatal: falls back to cached getsockname address if not available
    if (enableQuicPktinfo(fd, local_addr.ss_family) < 0) {
        printd(2, "enableQuicPktinfo() failed for fd %d family %d: %s\n",
            fd, local_addr.ss_family, strerror(errno));
    }

    // Store remote address
    uint16_t quic_port = alt ? static_cast<uint16_t>(alt->port)
        : static_cast<uint16_t>(connection.port);

    // Create QUIC client session
    // QuicSession::createClient() needs a qore_socket_private for TLS context;
    // use a temporary one with the fd set so crypto can access it.
    // RAII guard detaches fd from tmp_sock (preventing ~QoreSocket double-close)
    // AND closes fd on any exit path that doesn't call release().  This covers
    // both Qore-level errors (xsink) and C++ exceptions (e.g. std::bad_alloc
    // from std::make_shared inside createClient).
    QoreSocket tmp_sock;
    tmp_sock.priv->sock = fd;
    struct FdGuard {
        int fd;
        int& sock_ref;
        ~FdGuard() { sock_ref = -1; if (fd >= 0) ::close(fd); }
        void release() { fd = -1; }
    } fd_guard{fd, tmp_sock.priv->sock};
    quic_session = QuicSession::createClient(
        tmp_sock.priv, xsink,
        connection.host.c_str(), quic_port,
        reinterpret_cast<struct sockaddr*>(&local_addr), local_addrlen,
        res->ai_addr, res->ai_addrlen,
        msock->socket->priv->ssl_verify_mode,
        /*enable_0rtt=*/true,
        msock->cert, msock->pk);

    if (!quic_session || *xsink) {
        quic_session.reset();
        return -1;  // fd_guard destructor closes fd
    }
    fd_guard.release();  // success: fd ownership transferred to quic_session

    // Store the fd and local address for I/O
    quic_fd = fd;
    memcpy(&quic_local_addr, &local_addr, local_addrlen);
    quic_local_addrlen = local_addrlen;

    // Drive QUIC handshake
    int64 deadline_ms = connect_timeout_ms < 0 ? -1 : q_clock_getmillis() + connect_timeout_ms;

    while (!quic_session->isHandshakeComplete()) {
        if (quic_session->isClosed()) {
            xsink->raiseException("HTTP3-CONNECT-ERROR", "QUIC connection closed during handshake");
            disconnectQuic();
            return -1;
        }

        // Write pending packets
        QuicPacketBatch pkt_batch;
        int nw = quic_session->writePackets(pkt_batch, xsink);
        if (nw < 0 || *xsink) {
            disconnectQuic();
            return -1;
        }

        // Send packets via connected UDP socket (nullptr dest → kernel uses connected peer)
        if (!pkt_batch.empty()) {
            int sent = sendQuicPacketsBatch(quic_fd, pkt_batch, nullptr, 0);
            if (sent < 0) {
                xsink->raiseException("HTTP3-CONNECT-ERROR", "failed to send QUIC packets: %s",
                    strerror(errno));
                disconnectQuic();
                return -1;
            }
        }

        // 0-RTT: set up HTTP/3 early when 0-RTT TX key is installed
        // This allows HTTP/3 control + QPACK streams to be sent as 0-RTT data
        if (quic_session->isEarlyDataReady() && !quic_session->isHttp3Ready()) {
            rv = quic_session->setupHttp3(xsink);
            if (rv < 0 || *xsink) {
                disconnectQuic();
                return -1;
            }
            // Flush HTTP/3 setup frames as 0-RTT packets
            QuicPacketBatch h3_pkt_batch;
            quic_session->writePackets(h3_pkt_batch, xsink);
            if (*xsink) {
                disconnectQuic();
                return -1;
            }
            if (!h3_pkt_batch.empty()) {
                int sent = sendQuicPacketsBatch(quic_fd, h3_pkt_batch, nullptr, 0);
                if (sent < 0) {
                    xsink->raiseException("HTTP3-CONNECT-ERROR",
                        "failed to send 0-RTT HTTP/3 setup frames: %s", strerror(errno));
                    disconnectQuic();
                    return -1;
                }
            }
        }

        // Check timeout
        int poll_timeout_ms = 100;
        if (deadline_ms >= 0) {
            int64 now_ms = q_clock_getmillis();
            if (now_ms >= deadline_ms) {
                xsink->raiseException("HTTP3-CONNECT-ERROR", "QUIC handshake timed out after %d ms",
                    connect_timeout_ms);
                disconnectQuic();
                return -1;
            }
            poll_timeout_ms = std::min(100, static_cast<int>(deadline_ms - now_ms));
        }

        // Handle timer expiry
        ngtcp2_tstamp expiry = quic_session->getExpiry();
        if (expiry != UINT64_MAX) {
            ngtcp2_tstamp now = QuicSession::timestamp();
            if (expiry <= now) {
                quic_session->handleExpiry(xsink);
                if (*xsink) {
                    disconnectQuic();
                    return -1;
                }
                continue;
            }
            int timer_ms = static_cast<int>((expiry - now) / 1000000);
            if (timer_ms < poll_timeout_ms) {
                poll_timeout_ms = timer_ms > 0 ? timer_ms : 1;
            }
        }

        // Poll for incoming data
        struct pollfd pfd;
        pfd.fd = quic_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, poll_timeout_ms);
        if (pr < 0) {
            if (errno == EINTR) {
                if (qore_check_cancel(xsink, "QUIC handshake")) {
                    disconnectQuic();
                    return -1;
                }
                continue;
            }
            xsink->raiseException("HTTP3-CONNECT-ERROR", "poll error: %s", strerror(errno));
            disconnectQuic();
            return -1;
        }

        if (pr > 0 && (pfd.revents & POLLIN)) {
            // Drain all available packets (not just one), matching driveQuicIo pattern
            // Thread-local to avoid ~2.3KB stack pressure that triggers stack
            // protector failures under GCC 15+ on ARM64
            static thread_local uint8_t buf[QUIC_RECV_BUF_SIZE];
            static thread_local struct sockaddr_storage peer_addr;
            static thread_local uint8_t cmsg_buf[QUIC_CMSG_BUF_SIZE];
            while (true) {
                socklen_t peer_addrlen = sizeof(peer_addr);
                size_t cmsg_len = sizeof(cmsg_buf);
                ssize_t nread = recvQuicPacket(quic_fd, buf, sizeof(buf), MSG_DONTWAIT,
                    reinterpret_cast<struct sockaddr*>(&peer_addr), &peer_addrlen,
                    cmsg_buf, &cmsg_len);
                if (nread < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;  // no more data
                    }
                    xsink->raiseException("HTTP3-CONNECT-ERROR", "recvmsg error: %s", strerror(errno));
                    disconnectQuic();
                    return -1;
                }
                if (nread == 0) {
                    break;
                }

                // Use the cached getsockname() address directly — the client socket
                // is connect()ed, so the local address is always quic_local_addr.
                // Do NOT call extractPktinfoAddr(): on macOS, IP_RECVDSTADDR on
                // connected UDP sockets returns 0.0.0.0, causing ngtcp2 path
                // validation failures.
                assert(quic_fd >= 0);
                ngtcp2_path path;
                path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&quic_local_addr);
                path.local.addrlen = quic_local_addrlen;
                path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&peer_addr);
                path.remote.addrlen = peer_addrlen;

                quic_session->readPacket(buf, static_cast<size_t>(nread), path, xsink);
                if (*xsink) {
                    disconnectQuic();
                    return -1;
                }
            }
        }
    }

    // Handle 0-RTT rejection: if HTTP/3 was set up during 0-RTT handshake but
    // early data was rejected, ngtcp2 discards all 0-RTT streams (including HTTP/3
    // control and QPACK streams). Re-initialize the HTTP/3 layer with new streams.
    if (quic_session->isEarlyDataRejected() && quic_session->isHttp3Ready()) {
        printd(2, "connectQuic(): 0-RTT rejected, re-initializing HTTP/3 layer\n");
        rv = quic_session->resetHttp3(xsink);
        if (rv < 0 || *xsink) {
            disconnectQuic();
            return -1;
        }
    }

    // Setup HTTP/3 (no-op if already set up during 0-RTT)
    rv = quic_session->setupHttp3(xsink);
    if (rv < 0 || *xsink) {
        disconnectQuic();
        return -1;
    }

    // Flush any HTTP/3 setup frames
    {
        QuicPacketBatch pkt_batch;
        quic_session->writePackets(pkt_batch, xsink);
        if (*xsink) {
            disconnectQuic();
            return -1;
        }
        if (!pkt_batch.empty()) {
            int sent = sendQuicPacketsBatch(quic_fd, pkt_batch, nullptr, 0);
            if (sent < 0) {
                xsink->raiseException("HTTP3-SETUP-ERROR",
                    "failed to send HTTP/3 setup frames: %s", strerror(errno));
                disconnectQuic();
                return -1;
            }
        }
    }

    http3_active = true;

    // Close the TCP connection — no longer needed after successful QUIC upgrade.
    // The HTTP client will automatically reconnect over TCP if QUIC is later
    // disconnected (e.g. on error fallback in HTTP3_MODE_AUTO).  Keeping the
    // TCP socket open would waste an fd + kernel buffers for the lifetime of
    // the QUIC connection.
    msock->socket->close();

    return 0;
}

// Helper: drive QUIC I/O (send and receive packets) for the given session/fd
// Returns 0 on success, -1 on error; caller must call disconnectQuic() on error
static int driveQuicIo(QuicSession* session, int fd,
        const struct sockaddr_storage& local_addr, socklen_t local_addrlen,
        int poll_timeout_ms, ExceptionSink* xsink) {
    // Write pending packets
    QuicPacketBatch pkt_batch;
    int nw = session->writePackets(pkt_batch, xsink);
    if (nw < 0 || *xsink) {
        return -1;
    }

    // Send packets via connected UDP socket (nullptr dest → kernel uses connected peer)
    if (!pkt_batch.empty()) {
        int sent = sendQuicPacketsBatch(fd, pkt_batch, nullptr, 0);
        if (sent < 0) {
            xsink->raiseException("HTTP3-IO-ERROR", "failed to send QUIC packets: %s",
                strerror(errno));
            return -1;
        }
    }

    // Handle timer expiry
    ngtcp2_tstamp expiry = session->getExpiry();
    if (expiry != UINT64_MAX) {
        ngtcp2_tstamp now = QuicSession::timestamp();
        if (expiry <= now) {
            session->handleExpiry(xsink);
            if (*xsink) {
                return -1;
            }

            // handleExpiry() sets pending_write_ for retransmission/PTO probes;
            // send them immediately so the server receives probes promptly instead
            // of waiting until the next driveQuicIo() iteration (up to 100ms delay)
            pkt_batch.clear();
            nw = session->writePackets(pkt_batch, xsink);
            if (nw < 0 || *xsink) {
                return -1;
            }
            if (!pkt_batch.empty()) {
                int sent = sendQuicPacketsBatch(fd, pkt_batch, nullptr, 0);
                if (sent < 0) {
                    xsink->raiseException("HTTP3-IO-ERROR",
                        "failed to send QUIC packets after expiry: %s",
                        strerror(errno));
                    return -1;
                }
            }
        }
    }

    // Poll for incoming data — clamp poll timeout to the next QUIC timer expiry
    // so retransmission/PTO timers fire promptly even if the caller passes a long timeout
    int actual_timeout_ms = poll_timeout_ms;
    {
        ngtcp2_tstamp next_expiry = session->getExpiry();
        if (next_expiry != UINT64_MAX) {
            ngtcp2_tstamp now = QuicSession::timestamp();
            if (next_expiry <= now) {
                actual_timeout_ms = 0;
            } else {
                int64_t timer_ms = static_cast<int64_t>((next_expiry - now) / 1000000);
                if (timer_ms < actual_timeout_ms) {
                    actual_timeout_ms = static_cast<int>(timer_ms);
                }
            }
        }
    }
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int pr = poll(&pfd, 1, actual_timeout_ms);
    if (pr < 0) {
        if (errno == EINTR) {
            if (qore_check_cancel(xsink, "QUIC I/O")) {
                return -1;
            }
            return 0;
        }
        xsink->raiseException("HTTP3-IO-ERROR", "poll error: %s", strerror(errno));
        return -1;
    }

    if (pr > 0 && (pfd.revents & POLLIN)) {
        // Drain all available packets (not just one)
        // Thread-local to avoid ~2.3KB stack pressure that triggers stack
        // protector failures under GCC 15+ on ARM64
        static thread_local uint8_t buf[QUIC_RECV_BUF_SIZE];
        static thread_local struct sockaddr_storage peer_addr;
        static thread_local uint8_t cmsg_buf[QUIC_CMSG_BUF_SIZE];
        while (true) {
            socklen_t peer_addrlen = sizeof(peer_addr);
            size_t cmsg_len = sizeof(cmsg_buf);
            ssize_t nread = recvQuicPacket(fd, buf, sizeof(buf), MSG_DONTWAIT,
                reinterpret_cast<struct sockaddr*>(&peer_addr), &peer_addrlen,
                cmsg_buf, &cmsg_len);
            if (nread < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // no more data
                }
                xsink->raiseException("HTTP3-IO-ERROR", "recvmsg error: %s", strerror(errno));
                return -1;
            }
            if (nread == 0) {
                break;
            }

            // Use the cached getsockname() address directly — the client
            // socket is connect()ed, so the local address is always local_addr.
            // Do NOT call extractPktinfoAddr(): on macOS, IP_RECVDSTADDR on
            // connected UDP sockets returns 0.0.0.0, causing ngtcp2 path
            // validation failures.
            assert(fd >= 0);
            ngtcp2_path path;
            path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(
                const_cast<struct sockaddr_storage*>(&local_addr));
            path.local.addrlen = local_addrlen;
            path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&peer_addr);
            path.remote.addrlen = peer_addrlen;

            session->readPacket(buf, static_cast<size_t>(nread), path, xsink);
            if (*xsink) {
                return -1;
            }
        }
    }

    return 0;
}

QoreHashNode* qore_httpclient_priv::sendHttp3MessageAndGetResponse(const char* mname, const char* meth,
        const char* mpath, const QoreHashNode& nh, const QoreStringNode* body, const void* data, unsigned size,
        const ResolvedCallReferenceNode* send_callback, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback,
        QoreHashNode* info, int timeout_ms, int& code, ExceptionSink* xsink) {
    assert(quic_session);
    assert(quic_fd >= 0);

    // Convert request headers to std::map; RFC 9114 Section 4.2 prohibited
    // hop-by-hop headers (connection, keep-alive, proxy-connection,
    // transfer-encoding, upgrade) are filtered by QuicSession::submitRequest()
    strcase_str_map_t headers;
    ConstHashIterator hi(nh);
    while (hi.next()) {
        const char* key = hi.getKey();
        QoreValue val = hi.get();
        if (val.getType() == NT_STRING) {
            headers[key] = val.get<const QoreStringNode>()->c_str();
        } else if (val.getType() == NT_LIST) {
            const QoreListNode* l = val.get<const QoreListNode>();
            std::string joined;
            for (size_t i = 0; i < l->size(); ++i) {
                QoreValue elem = l->retrieveEntry(i);
                if (elem.getType() == NT_STRING) {
                    if (!joined.empty()) {
                        joined += ", ";
                    }
                    joined += elem.get<const QoreStringNode>()->c_str();
                }
            }
            if (!joined.empty()) {
                headers[key] = std::move(joined);
            }
        }
    }

    // Convert Host to :authority pseudo-header (RFC 9114 Section 4.3.1)
    auto host_it = headers.find("Host");
    if (host_it != headers.end()) {
        headers[":authority"] = host_it->second;
        headers.erase(host_it);
    }

    // Determine streaming mode
    bool streaming = send_callback || is;

    // Get request body data (not used in streaming mode)
    const void* body_data = nullptr;
    size_t body_len = 0;
    if (!streaming) {
        body_data = data;
        body_len = size;
        if (!body_data && body) {
            body_data = body->c_str();
            body_len = body->size();
        }
    }

    // Set Content-Length if body present and header not already set (non-streaming only)
    if (body_len > 0) {
        auto cl_it = headers.find("content-length");
        if (cl_it == headers.end()) {
            headers["content-length"] = std::to_string(body_len);
        }
    }

    // Add ";charset=xxx" to Content-Type for non-ISO-8859-1 text bodies
    if (!streaming && body && body_len > 0) {
        auto ct_it = headers.find("content-type");
        if (ct_it != headers.end()) {
            // Only add charset if not already present and not a boundary type
            if (ct_it->second.find("charset=") == std::string::npos
                    && ct_it->second.find("boundary=") == std::string::npos) {
                const QoreEncoding* enc = msock->socket->getEncoding();
                if (enc != QCS_ISO_8859_1) {
                    QoreString code(enc->getCode());
                    code.tolwr();
                    ct_it->second += ";charset=";
                    ct_it->second += code.c_str();
                }
            }
        }
    }

    // Record request in info if provided
    if (info) {
        info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/3", meth, mpath), nullptr);
    }

    // Submit the HTTP/3 request
    int64_t stream_id;
    if (streaming) {
        stream_id = quic_session->submitRequestStreaming(meth, mpath, headers, xsink);
    } else {
        stream_id = quic_session->submitRequest(meth, mpath, headers, body_data, body_len, xsink);
    }
    if (stream_id < 0) {
        return nullptr;
    }

    // Drive I/O until we have a complete response
    int64 deadline_ms = timeout_ms < 0 ? -1 : q_clock_getmillis() + timeout_ms;

    // Helper lambda: drive QUIC I/O and check for errors
    auto drive_h3_io = [&]() -> int {
        int remaining_ms = 100;
        if (deadline_ms >= 0) {
            remaining_ms = std::min(100, static_cast<int>(deadline_ms - q_clock_getmillis()));
            if (remaining_ms <= 0) {
                return -2;  // timeout
            }
        }
        return driveQuicIo(quic_session.get(), quic_fd, quic_local_addr, quic_local_addrlen,
                remaining_ms, xsink) ? -1 : 0;
    };

    // Streaming body: feed chunks from send_callback or InputStream
    if (streaming) {
        // Flush headers first
        if (drive_h3_io()) {
            if (!*xsink) {
                xsink->raiseException("HTTP3-TIMEOUT",
                    "timeout sending HTTP/3 streaming request headers");
            }
            disconnectQuic();
            return nullptr;
        }

        if (send_callback) {
            // Chunk-feed loop using send_callback
            bool done = false;
            while (!done) {
                if (deadline_ms >= 0 && q_clock_getmillis() >= deadline_ms) {
                    xsink->raiseException("HTTP3-TIMEOUT",
                        "timeout sending HTTP/3 streaming request body (timeout: %d ms)", timeout_ms);
                    disconnectQuic();
                    return nullptr;
                }

                // Call the callback (no lock held — safe for arbitrary Qore code)
                ValueHolder res(send_callback->execValue(nullptr, xsink), xsink);
                if (*xsink) {
                    quic_session->cancelStream(stream_id, NGHTTP3_H3_INTERNAL_ERROR, xsink);
                    disconnectQuic();
                    return nullptr;
                }

                const void* chunk_data = nullptr;
                size_t chunk_len = 0;
                switch (res->getType()) {
                    case NT_STRING: {
                        const QoreStringNode* str = res->get<const QoreStringNode>();
                        if (str->empty()) {
                            done = true;
                        } else {
                            chunk_data = str->c_str();
                            chunk_len = str->size();
                        }
                        break;
                    }
                    case NT_BINARY: {
                        const BinaryNode* b = res->get<const BinaryNode>();
                        if (b->empty()) {
                            done = true;
                        } else {
                            chunk_data = b->getPtr();
                            chunk_len = b->size();
                        }
                        break;
                    }
                    case NT_NOTHING:
                    case NT_NULL:
                        done = true;
                        break;
                    default:
                        xsink->raiseException("HTTP3-CALLBACK-ERROR",
                            "send_callback returned type '%s'; expected 'string', 'binary', or NOTHING",
                            res->getTypeName());
                        quic_session->cancelStream(stream_id, NGHTTP3_H3_INTERNAL_ERROR, xsink);
                        disconnectQuic();
                        return nullptr;
                }

                if (chunk_data && chunk_len > 0) {
                    while (true) {
                        int srv = quic_session->sendStreamData(stream_id, chunk_data, chunk_len, false, xsink);
                        if (*xsink) {
                            disconnectQuic();
                            return nullptr;
                        }
                        if (srv == 0) {
                            break;
                        }
                        if (srv == 1) {
                            // Buffer full — drive I/O to drain and retry
                            int rv = drive_h3_io();
                            if (rv) {
                                if (!*xsink) {
                                    xsink->raiseException("HTTP3-TIMEOUT",
                                        "timeout during HTTP/3 streaming body backpressure");
                                }
                                disconnectQuic();
                                return nullptr;
                            }
                            continue;
                        }
                        disconnectQuic();
                        return nullptr;
                    }
                    // Flush the chunk
                    if (drive_h3_io()) {
                        if (!*xsink) {
                            xsink->raiseException("HTTP3-TIMEOUT",
                                "timeout flushing HTTP/3 streaming body chunk");
                        }
                        disconnectQuic();
                        return nullptr;
                    }
                }
            }
        } else if (is) {
            // Chunk-feed loop using InputStream
            size_t chunk_size = max_chunk_size > 0 ? max_chunk_size : 65536;
            while (true) {
                if (deadline_ms >= 0 && q_clock_getmillis() >= deadline_ms) {
                    xsink->raiseException("HTTP3-TIMEOUT",
                        "timeout sending HTTP/3 streaming request body (timeout: %d ms)", timeout_ms);
                    disconnectQuic();
                    return nullptr;
                }

                SimpleRefHolder<BinaryNode> buf(is->readHelper(chunk_size, xsink));
                if (*xsink) {
                    quic_session->cancelStream(stream_id, NGHTTP3_H3_INTERNAL_ERROR, xsink);
                    disconnectQuic();
                    return nullptr;
                }
                if (!buf) {
                    break;
                }

                while (true) {
                    int srv = quic_session->sendStreamData(stream_id, buf->getPtr(), buf->size(), false, xsink);
                    if (*xsink) {
                        disconnectQuic();
                        return nullptr;
                    }
                    if (srv == 0) {
                        break;
                    }
                    if (srv == 1) {
                        int rv = drive_h3_io();
                        if (rv) {
                            if (!*xsink) {
                                xsink->raiseException("HTTP3-TIMEOUT",
                                    "timeout during HTTP/3 streaming body backpressure");
                            }
                            disconnectQuic();
                            return nullptr;
                        }
                        continue;
                    }
                    disconnectQuic();
                    return nullptr;
                }
                if (drive_h3_io()) {
                    if (!*xsink) {
                        xsink->raiseException("HTTP3-TIMEOUT",
                            "timeout flushing HTTP/3 streaming body chunk");
                    }
                    disconnectQuic();
                    return nullptr;
                }
            }
        }

        // Handle trailers if provided
        if (trailer_callback) {
            ValueHolder trailer_val(trailer_callback->execValue(nullptr, xsink), xsink);
            if (*xsink) {
                disconnectQuic();
                return nullptr;
            }
            if (trailer_val->getType() == NT_HASH) {
                const QoreHashNode* trailers = trailer_val->get<const QoreHashNode>();
                strcase_str_map_t trailer_map;
                ConstHashIterator ti(*trailers);
                while (ti.next()) {
                    QoreValue v = ti.get();
                    if (v.getType() == NT_STRING) {
                        trailer_map[ti.getKey()] = v.get<const QoreStringNode>()->c_str();
                    }
                }
                if (!trailer_map.empty()) {
                    quic_session->submitTrailers(stream_id, trailer_map, xsink);
                    if (*xsink) {
                        disconnectQuic();
                        return nullptr;
                    }
                }
            }
        }

        // Signal EOF (no trailers) — close the stream for writing
        if (!trailer_callback) {
            quic_session->sendStreamData(stream_id, nullptr, 0, true, xsink);
            if (*xsink) {
                disconnectQuic();
                return nullptr;
            }
        }

        // Flush any remaining data
        if (drive_h3_io()) {
            if (!*xsink) {
                xsink->raiseException("HTTP3-TIMEOUT",
                    "timeout flushing HTTP/3 streaming request EOF");
            }
            disconnectQuic();
            return nullptr;
        }
    }

    while (!quic_session->hasCompletedStreams()) {
        if (quic_session->isClosed()) {
            xsink->raiseException("HTTP3-ERROR", "QUIC connection closed while waiting for response");
            disconnectQuic();
            return nullptr;
        }

        int remaining_ms = 100;
        if (deadline_ms >= 0) {
            int64 now_ms = q_clock_getmillis();
            if (now_ms >= deadline_ms) {
                xsink->raiseException("HTTP3-TIMEOUT", "timeout waiting for HTTP/3 response (timeout: %d ms)",
                    timeout_ms);
                disconnectQuic();
                return nullptr;
            }
            remaining_ms = std::min(100, static_cast<int>(deadline_ms - now_ms));
        }

        if (driveQuicIo(quic_session.get(), quic_fd, quic_local_addr, quic_local_addrlen,
                remaining_ms, xsink)) {
            disconnectQuic();
            return nullptr;
        }
    }

    // Get the completed stream and verify it matches the expected stream_id;
    // drain any stale completed streams from previous requests that were not
    // consumed (e.g. due to timeout or error)
    std::unique_ptr<QuicStreamInfo> stream;
    int drain_count = 0;
    while (true) {
        stream = quic_session->takeCompletedStream();
        if (!stream) {
            xsink->raiseException("HTTP3-ERROR", "no response received for stream %" PRId64, stream_id);
            return nullptr;
        }
        if (stream->stream_id == stream_id) {
            break;
        }
        // Stale stream from a previous request — discard and try next
        printd(2, "sendHttp3MessageAndGetResponse() discarding stale stream %" PRId64
            " (expected %" PRId64 ")\n", stream->stream_id, stream_id);
        if (++drain_count > 100) {
            xsink->raiseException("HTTP3-ERROR",
                "too many stale completed streams while looking for stream %" PRId64, stream_id);
            disconnectQuic();
            return nullptr;
        }
    }

    if (!stream->error_message.empty()) {
        xsink->raiseException("QUIC-BODY-TOO-LARGE", stream->error_message.c_str());
        return nullptr;
    }

    code = stream->status_code;

    if (code == 0) {
        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
            "HTTP/3 stream %" PRId64 " closed without a response (no :status header received)", stream_id);
        disconnectQuic();
        return nullptr;
    }

    // Build response hash in the same format as HTTP/1.x and HTTP/2
    ReferenceHolder<QoreHashNode> response(new QoreHashNode(autoTypeInfo), xsink);

    response->setKeyValue("status_code", code, xsink);

    const char* status_msg = get_http_status_message(code);
    response->setKeyValue("status_message", new QoreStringNode(status_msg), xsink);
    response->setKeyValue("http_version", new QoreStringNode("3"), xsink);

    // Add response headers
    {
        ReferenceHolder<QoreHashNode> h3h(httpMultiHeadersToQoreHash(stream->headers), xsink);
        ConstHashIterator rhi(*(*h3h));
        while (rhi.next()) {
            response->setKeyValue(rhi.getKey(), rhi.get().refSelf(), xsink);
        }
    }

    // Parse Alt-Svc from response headers (for future requests)
    auto alt_svc_it = stream->headers.find("alt-svc");
    if (alt_svc_it != stream->headers.end() && !alt_svc_it->second.empty()) {
        parseAltSvc(alt_svc_it->second[0].c_str(), connection.host.c_str(), connection.port);
    }

    // Add body if present — do NOT decompress here; send_internal() handles
    // Content-Encoding decompression uniformly for HTTP/2 and HTTP/3
    if (!stream->body.empty()) {
        BinaryNode* resp_body = new BinaryNode();
        resp_body->append(stream->body.data(), stream->body.size());
        response->setKeyValue("body", resp_body, xsink);
    }

    if (info) {
        info->setKeyValue("response-headers", response->refSelf(), xsink);
        QoreStringNode* response_uri = new QoreStringNode();
        response_uri->sprintf("HTTP/3 %d %s", code, status_msg);
        info->setKeyValue("response-uri", response_uri, xsink);
    }

    return response.release();
}

void check_headers(const char* str, int len, bool &multipart, QoreHashNode& ans, const QoreEncoding *enc,
        ExceptionSink* xsink) {
    // see if the string starts with "multipart/"
    if (!multipart) {
        if (len > 10 && !strncasecmp(str, "multipart/", 10)) {
            ans.setKeyValue("_qore_multipart", new QoreStringNode(str + 10, len - 10, enc), xsink);
            multipart = true;
        }
    } else {
        if (len > 9 && !strncasecmp(str, "boundary=", 9))
            ans.setKeyValue("_qore_multipart_boundary", new QoreStringNode(str + 9, len - 9, enc), xsink);
        else if (len > 6 && !strncasecmp(str, "start=", 6))
            ans.setKeyValue("_qore_multipart_start", new QoreStringNode(str + 6, len - 6, enc), xsink);
    }
}

QoreHashNode* qore_httpclient_priv::send_internal(ExceptionSink* xsink, const char* mname, const char* meth,
        const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body, const void* data,
        unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream* os,
        InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, bool streaming) {
    assert(!(data && send_callback));
    assert(!(data && is));
    assert(!(is && send_callback));
    assert(!info || info->is_unique());

    // issue #4841: do not override the connection path when sending
    con_info this_connection = connection;

    bool bodyp = false;
    meth = checkMethod(xsink, meth, bodyp);
    if (*xsink) {
        return nullptr;
    }

    // use the default timeout value if a zero value is given in the call
    if (!timeout_ms)
        timeout_ms = timeout;

    SafeLocker sl(msock->m);

    if (msock->checkNonBlock(xsink)) {
        return nullptr;
    }

    Queue* event_queue = msock->socket->getQueue();

    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        msg_body ? msg_body->getEncoding() : nullptr, (data || is || send_callback), (send_callback || is),
        keep_alive, host_override), xsink);

    // save original HTTP method in case we have to issue a CONNECT request to a proxy for an HTTPS connection
    const char* meth_orig = meth;

    bool use_proxy_connect = false;
    const char* proxy_path = nullptr;
    ReferenceHolder<QoreHashNode> proxy_headers(xsink);
    if (!proxy_connected && proxy_connection.has_url()) {
        proxy_headers = setProxyHeaders(xsink, this_connection, *nh, use_proxy_connect, proxy_path);
        if (*xsink) {
            return nullptr;
        }
        if (proxy_headers) {
            meth = "CONNECT";
            assert(use_proxy_connect);
            assert(proxy_path);
        } else {
            assert(!use_proxy_connect);
            assert(!proxy_path);
        }
    }

    int code;
    ReferenceHolder<QoreHashNode> ans(xsink);
    int redirect_count = 0;
    const char* location = nullptr;

    // flag for aborted chunked sends
    bool send_aborted = false;

    ReferenceHolder<QoreHashNode> callback_info(xsink);
    if (recv_callback && !info) {
        callback_info = new QoreHashNode(autoTypeInfo);
        info = *callback_info;
    }

    bool path_already_encoded = false;
    // only reconnect if we already have an open connection
    unsigned retries = msock->socket->isOpen() ? 0 : 1;
    while (true) {
        // set host field automatically if not overridden
        if (!host_override) {
            nh->setKeyValue("Host", getHostHeaderValueUnlocked(this_connection), xsink);
        }

        if (info) {
            info->setKeyValue("headers", nh->copy(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        //printd(5, "qore_httpclient_priv::send_internal() meth: %s proxy_path: %s mpath: %s upc: %d\n", meth,
        //    proxy_path ? proxy_path : "n/a", mpath, use_proxy_connect);
        // send HTTP message and get response header
        if (use_proxy_connect) {
            ans = sendMessageAndGetResponse(this_connection, mname, meth, proxy_path,
                *(*proxy_headers), nullptr, nullptr, 0, nullptr, nullptr, 0, nullptr, info, true, timeout_ms, code,
                send_aborted, false, xsink);
        } else {
            ans = sendMessageAndGetResponse(this_connection, mname, meth, mpath, *(*nh),
                msg_body, data, size, send_callback, is, max_chunk_size, trailer_callback, info, false, timeout_ms,
                code, send_aborted, path_already_encoded, xsink);
        }

        if (!ans) {
            assert(*xsink);
            // issue# 4879: reconnect immediately and try again if the socket was closed the first time
            if (!msock->socket->isOpen() && !retries) {
                ++retries;
                xsink->clear();
                continue;
            }
            return nullptr;
        }

        if (info) {
            info->setKeyValue("response-headers", ans->refSelf(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        if (!ans->is_unique()) {
            ans = ans->copy();
        }

        // issue #3116: pass a 304 Not Modified message back to the caller without processing
        if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
            // only disconnect if we have no proxy or we need to use proxy_connect
            if (!proxy_connection.has_url()) {
                //printd(5, "qore_httpclient_priv::send_internal() disconnecting; no proxy\n");
                disconnect_unlocked();
            } else if (proxy_connected) {
                proxy_connected = false;
            }

            host_override = false;
            const QoreStringNode* mess = ans->getKeyValue("status_message").get<QoreStringNode>();

            const QoreStringNode* loc = get_string_header_node(xsink, **ans, "location");
            if (*xsink) {
                return nullptr;
            }
            const char* location = loc && !loc->empty() ? loc->c_str() : 0;
            if (!location) {
                sl.unlock();
                const char* msg = mess ? mess->c_str() : "<no message>";
                xsink->raiseException("HTTP-CLIENT-REDIRECT-ERROR", "no redirect location given for status code %d: "
                    "message: '%s'", code, msg);
                return nullptr;
            }

            if (event_queue) {
                do_redirect_event(event_queue, msock->socket->priv, loc, mess);
            }

            // issue #4601: get and ignore any message body
            if (msock->socket->priv->isOpen() && strcmp(mname, "HEAD") && code != 204 && code != 304
                && getDiscardMessageBody(xsink, **ans, timeout_ms)) {
                assert(*xsink);
                disconnect_unlocked();
                return nullptr;
            }

            if (++redirect_count > max_redirects) {
                break;
            }

            if (redirectUrlUnlocked(location, this_connection, xsink)) {
                sl.unlock();
                const char* msg = mess ? mess->c_str() : "<no message>";
                xsink->appendLastDescription(": while setting URL for redirect location '%s' (code %d: "
                    "message: '%s')", location, code, msg);
                return nullptr;
            }
            if (!path_already_encoded) {
                path_already_encoded = true;
            }

            if (proxy_connection.has_url()) {
                proxy_headers = setProxyHeaders(xsink, this_connection, *nh, use_proxy_connect, proxy_path);
                if (*xsink) {
                    return nullptr;
                }
                if (proxy_headers) {
                    meth = "CONNECT";
                    assert(use_proxy_connect);
                    assert(proxy_path);
                } else {
                    assert(!use_proxy_connect);
                    assert(!proxy_path);
                }
            }

            // set redirect info in info hash if present
            if (info) {
                QoreString tmp;
                tmp.sprintf("redirect-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), loc->refSelf(), xsink);
                if (*xsink)
                    return nullptr;

                tmp.clear();
                tmp.sprintf("redirect-message-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), mess ? mess->refSelf() : 0, xsink);
            }

            // NOTE: Per RFC 7231, 301/302/303 redirects should change POST/PUT to GET (dropping the body),
            // while 307/308 should preserve the method and body. Currently we always resend with the same
            // method and body, which is incorrect for 301/302/303. The send_callback and body parameters
            // should be cleared for those redirect codes.

            // set mpath to NULL so that the new path will be taken
            mpath = nullptr;
            continue;
        } else if (use_proxy_connect) {
            meth = meth_orig;
            use_proxy_connect = false;
            proxy_path = nullptr;

            // set client target for SNI
            msock->socket->priv->client_target = this_connection.host;

            if (msock->socket->upgradeClientToSSL(xsink, -1, msock->cert, msock->pk)) {
                disconnect_unlocked();
                return nullptr;
            }
            proxy_connected = true;

            // remove "Proxy-Authorization" header
            nh->removeKey("Proxy-Authorization", xsink);
            if (*xsink) {
                return nullptr;
            }

            // try again as if we are talking directly to the client
            continue;
        }

        break;
    }

    if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
        sl.unlock();
        const char* mess = get_string_header(xsink, **ans, "status_message");
        if (!mess) {
            mess = "<no message>";
        }
        if (!location) {
            location = "<no location>";
        }
        xsink->raiseException("HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED", "maximum redirections (%d) exceeded; "
            "redirect code %d to '%s' ignored (message: '%s')", max_redirects, code, location, mess);
        return nullptr;
    }

    // process content-type
    if (processContentType(xsink, **ans)) {
        disconnect_unlocked();
        return nullptr;
    }

    // send headers to recv_callback
    // cannot reference info here; must copy it
    if (recv_callback
        && msock->socket->priv->runHeaderCallback(xsink, "HTTPClient", mname, *recv_callback, &msock->m, *ans,
            info ? info->copy() : nullptr, send_aborted, obj)) {
        return nullptr;
    }

    // ensure ans is unique after the recv_callback may have stored a reference to the headers hash
    if (!ans->is_unique()) {
        ans = ans->copy();
    }

    ValueHolder body(xsink);
    const char* content_encoding = nullptr;

    // do not read any message body for messages that cannot have one
    // rfc 2616 4.4 p1 (http://tools.ietf.org/html/rfc2616#section-4.4)
    /*
        1.Any response message which "MUST NOT" include a message-body (such
        as the 1xx, 204, and 304 responses and any response to a HEAD
        request) is always terminated by the first empty line after the
        header fields, regardless of the entity-header fields present in
        the message.
    */
    //printd(5, "qore_httpclient_priv::send_internal() this: %p bodyp: %d code: %d\n", this, bodyp, code);

    qore_uncompress_to_string_t dec = nullptr;

    const char* conn = get_string_header(xsink, **ans, "connection", true);
    if (*xsink) {
        disconnect_unlocked();
        return nullptr;
    }

    // code >= 300 && < 400 is already handled above

    // For HTTP/2 and HTTP/3, the body is already in the response hash (as binary).
    // Skip the HTTP/1.x body reading logic.
    // NOTE: HTTP/3 currently returns early from sendHttp3MessageAndGetResponse()
    // and never reaches this point; this guard is defensive against future refactoring.
    if (http2_active || http3_active) {
        QoreValue h2_body = ans->getKeyValue("body");
        if (h2_body.getType() == NT_BINARY) {
            const BinaryNode* bin = h2_body.get<const BinaryNode>();
            if (bin && bin->size()) {
                if (os) {
                    // Write body to OutputStream
                    sl.unlock();
                    os->write(bin->getPtr(), bin->size(), xsink);
                    // OutputStream callers (sendChunked) return void; let ans auto-free
                    return nullptr;
                }
                const QoreEncoding* body_enc = msock->socket->getEncoding();
                if (!*xsink && !recv_callback) {
                    // Only process body encoding/conversion when no recv_callback
                    // Check content-encoding for compression
                    qore_uncompress_to_string_t h2_dec = nullptr;
                    content_encoding = normalizeContentEncoding(xsink, **ans, recv_callback, h2_dec);
                    if (!*xsink) {
                        QoreValue body_val = process_binary_body(bin, body_enc, content_encoding, h2_dec,
                            encoding_passthru, xsink);
                        if (!*xsink && body_val) {
                            ans->setKeyValue("body", body_val.getInternalNode(), xsink);
                        }
                    }
                } else if (!*xsink && recv_callback) {
                    // For recv_callback with HTTP/2: match HTTP/1.x behavior where
                    // readHttpChunkedBody delivers string data (no content-encoding)
                    // and readHttpChunkedBodyBinary delivers binary data (with content-encoding).
                    // Exception: Connect protocol (application/connect+*) uses binary frames
                    // with arbitrary bytes in the frame header that cannot survive string conversion.
                    qore_uncompress_to_string_t h2_dec = nullptr;
                    content_encoding = normalizeContentEncoding(xsink, **ans, recv_callback, h2_dec);
                    if (!*xsink && !content_encoding) {
                        // No content-encoding: check if Connect protocol (must stay binary)
                        const char* ct = get_string_header(xsink, **ans, "content-type", true);
                        bool is_connect = ct && !strncasecmp(ct, "application/connect+", 20);
                        if (!*xsink && !is_connect) {
                            // Convert binary to string (matches HTTP/1.x readHttpChunkedBody behavior)
                            QoreValue body_val = process_binary_body(bin, body_enc, nullptr, nullptr,
                                encoding_passthru, xsink);
                            if (!*xsink && body_val) {
                                ans->setKeyValue("body", body_val.getInternalNode(), xsink);
                            }
                        }
                    }
                    // With content-encoding: keep as binary (matches HTTP/1.x
                    // readHttpChunkedBodyBinary behavior)
                }
                if (*xsink) {
                    disconnect_unlocked();
                    return nullptr;
                }
            }
        }
        // For HTTP/2 with OutputStream but no/empty body; let ans auto-free
        if (os) {
            return nullptr;
        }
        // For HTTP/2, set response-body in info if needed
        if (info && !recv_callback) {
            QoreValue body_val = ans->getKeyValue("body");
            if (body_val) {
                info->setKeyValue("response-body", body_val.refSelf(), xsink);
            }
        }
        // For recv_callback, call it with the body data and then signal completion
        if (recv_callback) {
            QoreValue body_val = ans->getKeyValue("body");
            sl.unlock();
            // Call data callback with body
            if (body_val) {
                if (msock->socket->priv->runDataCallback(xsink, "HTTPClient", mname, *recv_callback, nullptr,
                        body_val.getInternalNode(), false)) {
                    return nullptr;
                }
            }
            // Call header callback with no argument to signal completion
            if (msock->socket->priv->runHeaderCallback(xsink, "HTTPClient", mname, *recv_callback, nullptr,
                    nullptr, nullptr, send_aborted, obj)) {
                return nullptr;
            }
            // For recv_callback, return nullptr (data was sent to callback)
            return nullptr;
        }
        // Skip the HTTP/1.x body reading logic
        goto http2_body_done;
    }

    if (bodyp && (code < 100 || code >= 200) && code != 204) {
        // see if we should do a binary or string read
        if (!os) {
            content_encoding = normalizeContentEncoding(xsink, **ans, recv_callback, dec);
            if (*xsink) {
                disconnect_unlocked();
                return nullptr;
            }
        } else {
            content_encoding = get_string_header(xsink, **ans, "content-encoding");
            if (*xsink) {
                disconnect_unlocked();
                return nullptr;
            }
        }

        const char* te = get_string_header(xsink, **ans, "transfer-encoding");
        if (*xsink) {
            disconnect_unlocked();
            return nullptr;
        }

        // get content type, if any
        const char* ct = get_string_header(xsink, **ans, "content-type", true);
        if (*xsink) {
            disconnect_unlocked();
            return nullptr;
        }

        // get response body, if any
        const char* cl = get_string_header(xsink, **ans, "content-length");
        if (*xsink) {
            disconnect_unlocked();
            return nullptr;
        }
        int len = cl ? atoi(cl) : 0;
        // do not try to get a body in any case if Content-Length: 0 is sent
        if (cl) {
            if (!len) {
                getbody = false;
            }
        } else {
            // issue #3691: ready the body if we have Connection: close and a content-type and a potential response
            if (!te && conn && !strcasecmp("close", conn) && strcmp(mname, "HEAD") && code != 204 && code != 304) {
                len = -1;
            }
        }

        if (cl && event_queue) {
            do_content_length_event(event_queue, msock->socket->priv, len);
        }

        // do *not* read a chunked body if streaming mode is enabled or the content-type is "text/event-stream"
        if (te && !strcasecmp(te, "chunked") && !streaming && (!ct || strcmp(ct, "text/event-stream"))) {
            // check for chunked response body
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_START);
            ReferenceHolder<QoreHashNode> nah(xsink);
            if (os) {
                msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "HTTPClient",
                    QORE_SOURCE_HTTPCLIENT, recv_callback, &msock->m, obj, os);
            } else if (recv_callback) {
                if (content_encoding) {
                    msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "HTTPClient",
                        QORE_SOURCE_HTTPCLIENT, recv_callback, &msock->m, obj);
                } else {
                    msock->socket->priv->readHttpChunkedBody(timeout_ms, xsink, "HTTPClient", QORE_SOURCE_HTTPCLIENT,
                        recv_callback, &msock->m, obj);
                }
            } else {
                if (content_encoding) {
                    nah = msock->socket->priv->readHttpChunkedBodyBinary(timeout_ms, xsink, "HTTPClient",
                        QORE_SOURCE_HTTPCLIENT);
                } else {
                    nah = msock->socket->priv->readHttpChunkedBody(timeout_ms, xsink, "HTTPClient",
                        QORE_SOURCE_HTTPCLIENT);
                }
            }
            do_event(event_queue, msock->socket->priv, QORE_EVENT_HTTP_CHUNKED_END);

            if (!nah && !recv_callback) {
                if (!msock->socket->isOpen()) {
                    disconnect_unlocked();
                }
                return nullptr;
            }

            if (info) {
                info->setKeyValue("chunked", true, xsink);
            }

            if (*xsink) {
                return nullptr;
            }

            if (!recv_callback && !os) {
                // merge all keys except the "body" key into ans
                ConstHashIterator hi(*nah);
                while (hi.next()) {
                if (!strcmp(hi.getKey(), "body")) {
                    assert(!body);
                    body = hi.getReferenced();
                    continue;
                }
                ans->setKeyValue(hi.getKey(), hi.getReferenced(), xsink);
                if (*xsink)
                    return nullptr;
                }
            }
        } else if (!streaming && (getbody || len)) {
            if (os) {
                msock->socket->priv->recvToOutputStream(os, len, timeout_ms, xsink, &msock->m,
                    QORE_SOURCE_HTTPCLIENT);
            } else if (content_encoding) {
                qore_offset_t rc;
                SimpleRefHolder<BinaryNode> bobj(msock->socket->priv->recvBinary(xsink, len, timeout_ms, rc,
                    QORE_SOURCE_HTTPCLIENT));
                if (!(*xsink) && bobj) {
                    body = bobj.release();
                }
            } else {
                qore_offset_t rc;
                QoreStringNodeHolder bstr(msock->socket->priv->recv(xsink, len, timeout_ms, rc,
                    QORE_SOURCE_HTTPCLIENT));
                if (!(*xsink) && bstr) {
                    body = bstr.release();
                }
            }

            if (*xsink && !msock->socket->isOpen()) {
                disconnect_unlocked();
            }
            //printf("body: %p\n", body->getInternalNode());
        }
    }

http2_body_done:
    if (*xsink) {
        disconnect_unlocked();
        return nullptr;
    }

    // check for connection: close header
    if (!keep_alive) {
        disconnect_unlocked();
    } else {
        if (conn && !strcasecmp(conn, "close")) {
            disconnect_unlocked();
        }
    }

    sl.unlock();

    // for content-encoding processing we can run unlocked

    // add body to result hash and process content encoding if necessary
    if (body) {
        if (body->getType() == NT_BINARY) {
            const BinaryNode* bin = body->get<BinaryNode>();
            QoreValue processed = process_binary_body(bin, msock->socket->getEncoding(), content_encoding, dec,
                encoding_passthru, xsink);
            if (*xsink) {
                ans = nullptr;
            } else if (processed) {
                body = processed.getInternalNode();
            }
        }

        if (body) {
            if (info) {
                info->setKeyValue("response-body", body.getReferencedValue(), xsink);
            }

            // send data to recv_callback (already unlocked)
            if (recv_callback) {
                // call body callbback and then header callback with no argument
                if (msock->socket->priv->runDataCallback(xsink, "HTTPClient", mname, *recv_callback, nullptr,
                        body->getInternalNode(), false)
                    || msock->socket->priv->runHeaderCallback(xsink, "HTTPClient", mname, *recv_callback, nullptr,
                        nullptr, nullptr, send_aborted, obj))
                    return nullptr;
            } else {
                ans->setKeyValue("body", body.release(), xsink);
            }
        }
    }

    // do not throw an exception if a receive callback is used
    if (!error_passthru && !recv_callback && !*xsink && (code < 100 || code >= 300)) {
        const char* mess = get_string_header(xsink, **ans, "status_message");
        if (!mess) {
            mess = "<no message>";
        }
        assert(!*xsink);

        xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", ans.release(), "HTTP status code %d received: message: "
            "%s", code, mess);
        return nullptr;
    }

    return *xsink || recv_callback || os ? nullptr : ans.release();
}

QoreHashNode* QoreHttpClientObject::send(const char* meth, const char* new_path, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, ExceptionSink* xsink) {
    return http_priv->send_internal(xsink, "send", meth, new_path, headers, nullptr, data, size, nullptr, getbody,
        info, http_priv->timeout, nullptr);
}

QoreHashNode* QoreHttpClientObject::send(const char* meth, const char* new_path, const QoreHashNode* headers,
    const QoreStringNode& body, bool getbody, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    return http_priv->send_internal(xsink, "send", meth, new_path, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, http_priv->timeout, nullptr);
}

QoreHashNode* QoreHttpClientObject::sendAndStream(const char* meth, const char* new_path, const QoreHashNode* headers,
        const void* data, unsigned size, QoreHashNode* info, ExceptionSink* xsink) {
    // streaming=true means don't read the body, leave it on the socket for streaming
    return http_priv->send_internal(xsink, "sendAndStream", meth, new_path, headers, nullptr, data, size, nullptr,
        false, info, http_priv->timeout, nullptr, nullptr, nullptr, nullptr, 0, nullptr, true);
}

QoreHashNode* QoreHttpClientObject::sendAndStream(const char* meth, const char* new_path, const QoreHashNode* headers,
        const QoreStringNode& body, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    // streaming=true means don't read the body, leave it on the socket for streaming
    return http_priv->send_internal(xsink, "sendAndStream", meth, new_path, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, false, info, http_priv->timeout, nullptr, nullptr, nullptr, nullptr, 0, nullptr, true);
}

QoreHashNode* QoreHttpClientObject::sendWithSendCallback(const char* meth, const char* mpath,
        const QoreHashNode* headers, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, ExceptionSink* xsink) {
    return http_priv->send_internal(xsink, "sendWithSendCallback", meth, mpath, headers, nullptr, nullptr, 0,
        send_callback, getbody, info, timeout_ms, nullptr);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers, nullptr, data, size, nullptr,
        getbody, info, timeout_ms, recv_callback, obj);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, timeout_ms, recv_callback, obj);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers, nullptr, data, size, nullptr,
        getbody, info, timeout_ms, recv_callback, obj, os);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers, *tstr, tstr->c_str(), tstr->size(),
        nullptr, getbody, info, timeout_ms, recv_callback, obj, os);
}

void QoreHttpClientObject::sendChunked(const char* meth, const char* mpath, const QoreHashNode* headers, bool getbody,
        QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj,
        OutputStream *os, InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    assert(max_chunk_size);
    http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers, nullptr, nullptr, 0, nullptr,
        getbody, info, timeout_ms, recv_callback, obj, os, is, max_chunk_size, trailer_callback);
}

void QoreHttpClientObject::sendWithCallbacks(const char* meth, const char* mpath, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    http_priv->send_internal(xsink, "sendWithCallbacks", meth, mpath, headers, nullptr, nullptr, 0, send_callback,
        getbody, info, timeout_ms, recv_callback, obj);
}

// returns *string
// @since Qore 0.8.12: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::get(const char* new_path, const QoreHashNode* headers, QoreHashNode* info,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "get", "GET", new_path, headers, nullptr,
        nullptr, 0, nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

QoreHashNode* QoreHttpClientObject::head(const char* new_path, const QoreHashNode* headers, QoreHashNode* info,
        ExceptionSink* xsink) {
   return http_priv->send_internal(xsink, "head", "HEAD", new_path, headers, nullptr, nullptr, 0, nullptr, false,
    info, http_priv->timeout);
}

// returns *string
// @since Qore 0.8.12: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::post(const char* new_path, const QoreHashNode* headers, const void* data,
        unsigned size, QoreHashNode* info, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "post", "POST", new_path, headers, nullptr,
        data, size, nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

// returns *string
// @since Qore 0.9.4: do not send getbody = true which only works with completely broken HTTP servers and small messages and causes deadlocks on correct HTTP servers
AbstractQoreNode* QoreHttpClientObject::post(const char* new_path, const QoreHashNode* headers,
        const QoreStringNode& body, QoreHashNode* info, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return nullptr;
    }
    ReferenceHolder<QoreHashNode> ans(http_priv->send_internal(xsink, "post", "POST", new_path, headers, *tstr,
        tstr->c_str(), tstr->size(), nullptr, false, info, http_priv->timeout), xsink);
    if (!ans) {
        return nullptr;
    }
    return ans->takeKeyValue("body").getInternalNode();
}

void QoreHttpClientObject::addProtocol(const char* prot, int new_port, bool new_ssl) {
    AutoLocker al(priv->m);
    http_priv->prot_map[prot] = make_protocol(new_port, new_ssl);
}

void QoreHttpClientObject::setMaxRedirects(int max) {
    AutoLocker al(priv->m);
    http_priv->max_redirects = max;
}

int QoreHttpClientObject::getMaxRedirects() const {
    return http_priv->max_redirects;
}

void QoreHttpClientObject::setDefaultHeaderValue(const char* header, const char* val) {
    AutoLocker al(priv->m);
    http_priv->default_headers[header] = val;
}

void QoreHttpClientObject::addDefaultHeaders(const QoreHashNode* hdr) {
    AutoLocker al(priv->m);
    ConstHashIterator i(hdr);
    while (i.next()) {
        QoreStringValueHelper str(i.get());
        http_priv->default_headers[i.getKey()] = str->c_str();
    }
}

QoreHashNode* QoreHttpClientObject::getDefaultHeaders() const {
    ReferenceHolder<QoreHashNode> rv(new QoreHashNode(stringTypeInfo), nullptr);
    qore_hash_private* h = qore_hash_private::get(**rv);

    AutoLocker al(priv->m);
    for (auto i : http_priv->default_headers) {
        h->setKeyValueIntern(i.first.c_str(), new QoreStringNode(i.second));
    }

    return rv.release();
}

void QoreHttpClientObject::setEventQueue(Queue *cbq, ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, cbq, QoreValue(), false);
}

void QoreHttpClientObject::setEventQueue(ExceptionSink* xsink, Queue* q, QoreValue arg, bool with_data) {
    AutoLocker al(priv->m);
    priv->socket->setEventQueue(xsink, q, arg, with_data);
}

void QoreHttpClientObject::cleanup(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->close();
    http_priv->disconnectQuic();
    priv->invalidate();
    priv->socket->cleanup(xsink);
}

void QoreHttpClientObject::lock() {
    priv->m.lock();
}

void QoreHttpClientObject::unlock() {
    priv->m.unlock();
}

int QoreHttpClientObject::setNoDelay(bool nd) {
    return http_priv->setNoDelay(nd);
}

bool QoreHttpClientObject::getNoDelay() const {
    return http_priv->getNoDelay();
}

bool QoreHttpClientObject::isConnected() const {
    return http_priv->msock->socket->isOpen();
}

void QoreHttpClientObject::setUserPassword(const char* user, const char* pass) {
    http_priv->setUserPassword(user, pass);
}

void QoreHttpClientObject::clearUserPassword() {
    http_priv->clearUserPassword();
}

void QoreHttpClientObject::setProxyUserPassword(const char* user, const char* pass) {
    http_priv->setProxyUserPassword(user, pass);
}

void QoreHttpClientObject::clearProxyUserPassword() {
    http_priv->clearProxyUserPassword();
}

void QoreHttpClientObject::setPersistent(ExceptionSink* xsink) {
    return http_priv->setPersistent(xsink);
}

void QoreHttpClientObject::clearPersistent() {
    return http_priv->clearPersistent();
}

bool QoreHttpClientObject::isPersistent() const {
    return http_priv->persistent_count ? true : false;
}

unsigned QoreHttpClientObject::getPersistentCount() const {
    return http_priv->persistent_count;
}

void QoreHttpClientObject::clearWarningQueue(ExceptionSink* xsink) {
    AutoLocker al(priv->m);
    priv->socket->clearWarningQueue(xsink);
}

void QoreHttpClientObject::setWarningQueue(ExceptionSink* xsink, int64 warning_ms, int64 warning_bs, Queue* wq,
        QoreValue arg, int64 min_ms) {
    AutoLocker al(priv->m);
    priv->socket->setWarningQueue(xsink, warning_ms, warning_bs, wq, arg, min_ms);
}

QoreHashNode* QoreHttpClientObject::getUsageInfo() const {
    AutoLocker al(priv->m);
    return priv->socket->getUsageInfo();
}

void QoreHttpClientObject::clearStats() {
    AutoLocker al(priv->m);
    priv->socket->clearStats();
}

bool QoreHttpClientObject::setEncodingPassthru(bool set) {
    return http_priv->setEncodingPassthru(set);
}

bool QoreHttpClientObject::getEncodingPassthru() const {
    return http_priv->getEncodingPassthru();
}

bool QoreHttpClientObject::setErrorPassthru(bool set) {
    return http_priv->setErrorPassthru(set);
}

bool QoreHttpClientObject::getErrorPassthru() const {
    return http_priv->getErrorPassthru();
}

bool QoreHttpClientObject::setRedirectPassthru(bool set) {
    return http_priv->setRedirectPassthru(set);
}

bool QoreHttpClientObject::getRedirectPassthru() const {
    return http_priv->getRedirectPassthru();
}

QoreStringNode* QoreHttpClientObject::getHostHeaderValue() const {
    return http_priv->getHostHeaderValue();
}

void QoreHttpClientObject::setAssumedEncoding(const char* enc) {
    AutoLocker al(priv->m);
    qore_socket_private::get(*priv->socket)->setAssumedEncoding(enc);
}

QoreStringNode* QoreHttpClientObject::getAssumedEncoding() const {
    AutoLocker al(priv->m);
    return new QoreStringNode(qore_socket_private::get(*priv->socket)->getAssumedEncoding());
}

bool QoreHttpClientObject::setPreEncodedUrls(bool set) {
    AutoLocker al(priv->m);
    bool rv = http_priv->pre_encoded_urls;
    if (rv != set) {
        http_priv->pre_encoded_urls = set;
    }
    return rv;
}

bool QoreHttpClientObject::getPreEncodedUrls() const {
    AutoLocker al(priv->m);
    return http_priv->pre_encoded_urls;
}

QoreHashNode* QoreHttpClientObject::getConfig() const {
    AutoLocker al(priv->m);
    return http_priv->getConfig(*priv);
}
