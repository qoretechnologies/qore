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
#include <qore/HttpClientConnectionManager.h>
#include <qore/QoreFuture.h>
#include <qore/AsyncCompletionAction.h>
#include "qore/intern/QoreChannel.h"
#include "qore/intern/QoreEventNotifier.h"
#include "qore/intern/QC_EventNotifier.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/AsyncCompletionAction.h"
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/ql_misc.h"
#include "qore/intern/QC_Socket.h"
#include "qore/intern/QC_Queue.h"
#include "qore/intern/QC_SocketPollOperation.h"
#include "qore/intern/QC_SocketPollOperationBase.h"
#include "qore/intern/QoreHttpClientObjectIntern.h"
#include "qore/intern/ql_crypto.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include "qore/intern/QoreHashNodeIntern.h"
#include "qore/intern/ql_compression.h"

#include "qore/intern/qore_socket_private.h"
#include "qore/intern/qore_string_private.h"

#include "qore/intern/Http2Session.h"
#include "qore/intern/QuicSession.h"

#include <poll.h>
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

const char* QoreHttpClientObject::getHttpStatusMessage(int code) {
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
    const char* status_msg = QoreHttpClientObject::getHttpStatusMessage(code);
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

// ============================================================================
// LEGACY RESPONSE SHAPE ADAPTERS
// ----------------------------------------------------------------------------
// The C++ connection manager produces ONE canonical response hash shape for
// every request — the one documented by the HttpClientResponseInfo hashdecl
// in qlib/HttpClientIo/HttpClientIo.qm:
//
//     { status_code, status_message, http_version,
//       headers, headers_raw, body }
//
// Pure-async callers (HttpClientConnectionManager.qc, HttpClientIo,
// RestClientIo, their DataProviders) consume that shape directly.  No code
// in that path is allowed to add ad-hoc fields, flatten nested data, or
// duplicate values into multiple slots.
//
// TWO legacy callers still expect different response shapes, and they are
// fed by the adapters in this section — nothing else:
//
//   1. qore_httpclient_priv::send_internal_conn_mgr → transformConnMgrResponse
//        feeds the sync HTTPClient::send()/get()/post()/… API, which
//        predates nested headers and expects them flattened to the top
//        level alongside status_code and body.  Matches the legacy
//        readHTTPHeader() response format.
//
//   2. HttpClientConnMgrPollOp::getOutput → toLegacyPollApiOutputShape
//        feeds HTTPClient::startPollSendRecv(...).getOutput(), which
//        predates the nested-headers design and expects the shape
//          { code,
//            info: { response-headers, response-headers-raw, response-body },
//            response-body }
//        where response-body is intentionally present at BOTH positions
//        for backward compatibility with existing poll-API consumers
//        that read either location.  This matches what the pre-conn_mgr
//        HttpClientConnectSendRecvPollOperation::getOutput() produced.
//
// NEW CODE MUST NOT ADD TO THIS SECTION.  If you need a different shape
// for a new async API, read HttpClientResponseInfo directly and do the
// transformation at the call site you introduce.  Any future legacy
// adapter (if one is ever needed again) goes in its own named function
// right here, in this section, with this same guardrail.
// ============================================================================

// Transform a clean HttpClientResponseInfo-shaped response hash into the
// legacy sync HTTPClient shape that send_internal's redirect/auth/encoding
// logic expects.  Only called from @c send_internal_conn_mgr.
//
// See LEGACY RESPONSE SHAPE ADAPTERS block above for the rationale and
// the full list of callers — do not reuse this function for anything else.
static QoreHashNode* transformConnMgrResponse(QoreHashNode* src, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);

    // Copy top-level scalar fields
    QoreValue v = src->getKeyValue("status_code");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("status_code", v.refSelf(), xsink);
    }
    v = src->getKeyValue("status_message");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("status_message", v.refSelf(), xsink);
    }
    v = src->getKeyValue("http_version");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("http_version", v.refSelf(), xsink);
    }

    // Flatten nested headers to top level (matching readHTTPHeader format)
    v = src->getKeyValue("headers");
    if (v.getType() == NT_HASH) {
        const QoreHashNode* hdrs = v.get<const QoreHashNode>();
        ConstHashIterator hi(hdrs);
        while (hi.next()) {
            result->setKeyValue(hi.getKey(), hi.get().refSelf(), xsink);
        }
    }

    // Copy body if present
    v = src->getKeyValue("body");
    if (!v.isNullOrNothing()) {
        result->setKeyValue("body", v.refSelf(), xsink);
    }

    return result.release();
}

// Transform a clean HttpClientResponseInfo-shaped response hash into the
// legacy HTTPClient poll API shape consumed by
//   hc.startPollSendRecv(...).getOutput()
// which returns:
//
//   { code, info: { response-headers, response-headers-raw,
//                   response-body },
//     response-body }
//
// — response-body lives in BOTH positions (inside info and at the top
// level), matching the pre-conn_mgr @c HttpClientConnectSendRecvPollOperation
// output format: @c processReceivedBody sets @c info->response-body and
// @c getOutput sets @c rv->response-body from @c recv_data_holder.  Existing
// poll-API consumers read either location, so we populate both.
//
// Also handles Content-Encoding decompression on the body, preserving
// the contract that poll-API callers don't see the compressed bytes.
//
// Only called from @c HttpClientConnMgrPollOp::getOutput.
//
// See LEGACY RESPONSE SHAPE ADAPTERS block above for the rationale and
// the full list of callers — do not reuse this function for anything else.
static QoreHashNode* toLegacyPollApiOutputShape(const QoreHashNode* src,
        ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    QoreValue sc = src->getKeyValue("status_code");
    if (!sc.isNullOrNothing()) {
        result->setKeyValue("code", sc.getAsBigInt(), xsink);
    }

    // Decode the body once (decompressing if needed) and reuse the same
    // ref at both the top-level and the info sub-hash positions.
    ReferenceHolder<AbstractQoreNode> body_ref(nullptr);
    QoreValue body = src->getKeyValue("body");
    QoreValue hdrs_v = src->getKeyValue("headers");
    if (!body.isNullOrNothing()) {
        if (body.getType() == NT_BINARY) {
            const BinaryNode* bin = body.get<const BinaryNode>();
            const char* content_encoding = nullptr;
            if (hdrs_v.getType() == NT_HASH) {
                QoreValue cev = hdrs_v.get<const QoreHashNode>()->getKeyValue(
                    "content-encoding");
                if (cev.getType() == NT_STRING) {
                    const char* ce = cev.get<const QoreStringNode>()->c_str();
                    if (ce && *ce && strcasecmp(ce, "identity")) {
                        content_encoding = ce;
                    }
                }
            }
            if (content_encoding && bin && bin->size()) {
                bool ignore_encoding = false;
                qore_uncompress_to_string_t dec =
                    get_decoder_for_content_encoding(content_encoding,
                        ignore_encoding);
                if (dec && !ignore_encoding) {
                    QoreStringNode* decoded = dec(bin, QCS_UTF8, xsink);
                    if (!*xsink && decoded) {
                        // Convert decompressed string back to binary for
                        // the poll API's response-body format
                        SimpleRefHolder<BinaryNode> decompressed(
                            new BinaryNode());
                        decompressed->append(decoded->c_str(), decoded->size());
                        decoded->deref(xsink);
                        body_ref = decompressed.release();
                    } else {
                        if (*xsink) {
                            xsink->clear();
                        }
                        if (decoded) {
                            decoded->deref(xsink);
                        }
                    }
                }
            }
        }
        // If no decompression happened (not binary, no encoding, or
        // decoder missing), pass the source body through.
        if (!body_ref) {
            body_ref = body.getInternalNode()
                ? body.getInternalNode()->refSelf() : nullptr;
        }
    }

    // Build the info sub-hash (response-headers, response-headers-raw,
    // response-body).
    ReferenceHolder<QoreHashNode> info_hash(new QoreHashNode(autoTypeInfo), xsink);
    // Build response-headers by merging top-level response metadata
    // (status_code, status_message, http_version) into the headers hash,
    // matching what set_http2_response_info does in the legacy sync path.
    if (hdrs_v.getType() == NT_HASH) {
        ReferenceHolder<QoreHashNode> rh(hdrs_v.get<QoreHashNode>()->copy(), xsink);
        if (!sc.isNullOrNothing()) {
            rh->setKeyValue("status_code", sc.getAsBigInt(), xsink);
        }
        QoreValue sm = src->getKeyValue("status_message");
        if (sm.getType() == NT_STRING) {
            rh->setKeyValue("status_message", sm.refSelf(), xsink);
        } else if (!sc.isNullOrNothing()) {
            rh->setKeyValue("status_message",
                new QoreStringNode(QoreHttpClientObject::getHttpStatusMessage(
                    (int)sc.getAsBigInt())), xsink);
        }
        QoreValue hv = src->getKeyValue("http_version");
        if (hv.getType() == NT_STRING) {
            rh->setKeyValue("http_version", hv.refSelf(), xsink);
        } else {
            // Infer http_version from protocol or stream_id presence
            QoreValue proto = src->getKeyValue("protocol");
            if (proto.getType() == NT_STRING) {
                const char* p = proto.get<const QoreStringNode>()->c_str();
                if (!strcmp(p, "h2")) {
                    rh->setKeyValue("http_version",
                        new QoreStringNode("2"), xsink);
                } else if (!strcmp(p, "h3")) {
                    rh->setKeyValue("http_version",
                        new QoreStringNode("3"), xsink);
                }
            } else if (!src->getKeyValue("stream_id").isNullOrNothing()) {
                rh->setKeyValue("http_version",
                    new QoreStringNode("2"), xsink);
            }
        }
        info_hash->setKeyValue("response-headers", rh.release(), xsink);
    }
    QoreValue hdrs_raw_v = src->getKeyValue("headers_raw");
    if (hdrs_raw_v.getType() == NT_HASH) {
        info_hash->setKeyValue("response-headers-raw",
            hdrs_raw_v.refSelf(), xsink);
    } else if (hdrs_v.getType() == NT_HASH) {
        // H2/H3 responses have no headers_raw (headers are already
        // lowercase per spec); use the same headers hash.
        info_hash->setKeyValue("response-headers-raw",
            hdrs_v.refSelf(), xsink);
    }
    if (body_ref) {
        info_hash->setKeyValue("response-body", body_ref->refSelf(), xsink);
    }
    // Populate body-content-type from the Content-Type header (matching
    // set_body_content_type_info in the legacy sync path).
    if (hdrs_v.getType() == NT_HASH) {
        const QoreHashNode* hdrs = hdrs_v.get<const QoreHashNode>();
        QoreValue ct = hdrs->getKeyValue("content-type");
        if (ct.getType() == NT_STRING) {
            const QoreStringNode* cts = ct.get<const QoreStringNode>();
            if (!cts->empty()) {
                const char* s = cts->c_str();
                while (*s == ' ') { ++s; }
                const char* e = s;
                while (*e && *e != ';' && *e != ',') { ++e; }
                if (e > s) {
                    SimpleRefHolder<QoreStringNode> base(new QoreStringNode());
                    base->concat(s, e - s);
                    base->trim();
                    if (!base->empty()) {
                        info_hash->setKeyValue("body-content-type",
                            base.release(), xsink);
                    }
                }
            }
        }
    }
    // Populate response-uri (matching setConnMgrResponseUri in the sync
    // path).  The poll path's raw response may not have protocol/
    // http_version, so derive the version from stream_id presence.
    if (!sc.isNullOrNothing()) {
        QoreValue sm_val = src->getKeyValue("status_message");
        const char* sm_str = sm_val.getType() == NT_STRING
            ? sm_val.get<const QoreStringNode>()->c_str() : "";
        if (!sm_str[0]) {
            sm_str = QoreHttpClientObject::getHttpStatusMessage(
                (int)sc.getAsBigInt());
        }
        const char* ver = "HTTP/1.1";
        QoreValue proto = src->getKeyValue("protocol");
        QoreValue hv_src = src->getKeyValue("http_version");
        if (proto.getType() == NT_STRING) {
            const char* p = proto.get<const QoreStringNode>()->c_str();
            if (!strcmp(p, "h2")) {
                ver = "HTTP/2";
            } else if (!strcmp(p, "h3")) {
                ver = "HTTP/3";
            } else if (hv_src.getType() == NT_STRING) {
                // H1 with explicit version — build URI inline to avoid
                // a static buffer (thread safety)
                QoreStringNode* uri = new QoreStringNodeMaker("HTTP/%s %d %s",
                    hv_src.get<const QoreStringNode>()->c_str(),
                    (int)sc.getAsBigInt(), sm_str);
                info_hash->setKeyValue("response-uri", uri, xsink);
                ver = nullptr;  // skip default URI below
            }
        } else if (!src->getKeyValue("stream_id").isNullOrNothing()) {
            ver = "HTTP/2";
        }
        if (ver) {
            QoreStringNode* uri = new QoreStringNodeMaker("%s %d %s", ver,
                (int)sc.getAsBigInt(), sm_str);
            info_hash->setKeyValue("response-uri", uri, xsink);
        }
    }
    if (!info_hash->empty()) {
        result->setKeyValue("info", info_hash.release(), xsink);
    }
    // Top-level response-body (legacy consumers read here).
    if (body_ref) {
        result->setKeyValue("response-body", body_ref.release(), xsink);
    }
    return result.release();
}

// Set info["response-uri"] from the conn_mgr response hash (which has keys
// status_code, status_message, protocol).  Matches legacy HTTP/1.x/2/3
// behavior: `HTTP/<version> <code> <message>`.
static void setConnMgrResponseUri(QoreHashNode* info, const QoreHashNode* src,
        ExceptionSink* xsink) {
    if (!info || !src) {
        return;
    }
    QoreValue sc = src->getKeyValue("status_code");
    if (sc.isNullOrNothing()) {
        return;
    }
    QoreValue msg = src->getKeyValue("status_message");
    const char* msg_str = msg.getType() == NT_STRING
        ? msg.get<const QoreStringNode>()->c_str() : "";
    QoreValue proto = src->getKeyValue("protocol");
    const char* proto_str = proto.getType() == NT_STRING
        ? proto.get<const QoreStringNode>()->c_str() : "h1";
    // Map protocol token (h1/h2/h3) to HTTP version label
    const char* ver;
    if (!strcmp(proto_str, "h2")) {
        ver = "HTTP/2";
    } else if (!strcmp(proto_str, "h3")) {
        ver = "HTTP/3";
    } else {
        // For h1, prefer the http_version field if present
        QoreValue hv = src->getKeyValue("http_version");
        if (hv.getType() == NT_STRING) {
            QoreStringNode* rv = new QoreStringNodeMaker("HTTP/%s %d %s",
                hv.get<const QoreStringNode>()->c_str(), (int)sc.getAsBigInt(),
                msg_str);
            info->setKeyValue("response-uri", rv, xsink);
            return;
        }
        ver = "HTTP/1.1";
    }
    QoreStringNode* rv = new QoreStringNodeMaker("%s %d %s", ver,
        (int)sc.getAsBigInt(), msg_str);
    info->setKeyValue("response-uri", rv, xsink);
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
    // Atomic: may be read from send_internal without priv->m while set
    // by setHTTPVersion / setHttp2Mode under priv->m
    std::atomic<int> http2_mode{HTTP2_MODE_AUTO};

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

    //! C++ connection manager for async-driven dispatch (Phase P7+).
    /** Lazily created on first use via getConnMgr().  When present, sync
        HTTPClient methods (P8-P11) delegate H1/H2/H3 work through it
        instead of using the legacy sync socket dispatch.  The Qore-level
        HttpClientIo module has its own manager with richer features
        (retry, Alt-Svc, cookies); this one is the minimal C++ base for
        the sync HTTPClient conversion.
    */
    std::unique_ptr<HttpClientConnectionManagerBase> conn_mgr;

    //! When true, send_internal delegates to the C++ conn_mgr for
    //! non-streaming request/response flows (all protocols, including proxy).
    //! Disabled when poll APIs are used (poll_apis_used flag) to preserve
    //! legacy msock state for startPollConnect/startPollSendRecv.
    bool use_conn_mgr = true;

    //! Set when poll APIs (startPollConnect, startPollSendRecv) are used on
    //! this HTTPClient.  Once set, send_internal falls back to the legacy
    //! msock dispatch so that poll APIs (which use msock) find it connected.
    bool poll_apis_used = false;

    //! Set during user-initiated disconnect() to map HTTP1-ABORT errors
    //! on pending poll ops to SOCKET-NOT-OPEN (matching legacy semantics).
    std::atomic<bool> user_disconnect_in_progress{false};

    //! Channel for conn_mgr streaming receive (sendAndStream mode).
    /** When non-null, readHTTPChunk/readServerSentEvent read from this
        channel instead of the raw socket.  Set by send_internal_conn_mgr
        when streaming=true; cleared by clearStreamingChannel().
    */
    QoreChannel* streaming_recv_channel = nullptr;

    //! Buffer for accumulating partial SSE event text across channel messages
    std::string sse_recv_buffer;

    //! Returns the connection manager, creating it lazily if needed.
    DLLLOCAL HttpClientConnectionManagerBase& getConnMgr(ExceptionSink* xsink) {
        // Check if SSL settings or the effective protocol changed since
        // the conn_mgr was created.  If so, reset so a fresh connection
        // is established with the new settings.  The global H2 mode can
        // change at runtime via set_global_http2_mode(); re-evaluate it.
        if (conn_mgr) {
            const auto& opts = conn_mgr->getOptions();
            // Recompute the effective protocol
            int gm = qore_global_http2_mode.load(std::memory_order_relaxed);
            bool ld = qore_check_option(QLO_DISABLE_HTTP2);
            // H2C_DIRECT is also an H2 protocol (over plain TCP, client
            // sends the HTTP/2 preface on connect).  REQUIRED with SSL
            // negotiates h2 via ALPN; REQUIRED without SSL is equivalent
            // to H2C_DIRECT.  AUTO over SSL uses NEGOTIATE (per-connect
            // ALPN via NegotiatingHttpClientConnection).
            bool h2_hard = (http2_mode == HTTP2_MODE_REQUIRED
                    || http2_mode == HTTP2_MODE_H2C_DIRECT)
                && gm != HTTP2_MODE_DISABLED && !ld;
            bool h2_auto_ssl = http2_mode == HTTP2_MODE_AUTO && connection.ssl
                && gm != HTTP2_MODE_DISABLED && !ld;
            HttpClientProtocol want_proto;
            if (http3_mode.load(std::memory_order_relaxed)
                    == HTTP3_MODE_REQUIRED) {
                want_proto = HttpClientProtocol::H3;
            } else if (h2_hard) {
                want_proto = HttpClientProtocol::H2;
            } else if (h2_auto_ssl) {
                want_proto = HttpClientProtocol::NEGOTIATE;
            } else {
                want_proto = HttpClientProtocol::H1;
            }
            if (opts.protocol != want_proto
                    || opts.ssl_verify_mode != msock->socket->priv->ssl_verify_mode
                    || opts.accept_all_certs != msock->socket->priv->ssl_accept_all_certs
                    || opts.client_cert != msock->cert
                    || opts.client_key != msock->pk) {
                conn_mgr.reset();
            }
        }
        if (!conn_mgr) {
            HttpClientConnectionManagerBase::Options opts;
            // Derive protocol from the HTTPClient's mode settings.
            // HTTP2_MODE_AUTO over SSL maps to HttpClientProtocol::
            // NEGOTIATE — the conn_mgr's NegotiatingHttpClientConnection
            // path does per-connect ALPN over TLS and adopts the result
            // into a concrete H1/H2 connection (see
            // design/conn-mgr-alpn-negotiation.md).  REQUIRED and
            // H2C_DIRECT map to H2, REQUIRED H3 maps to H3, and
            // everything else maps to H1.  The global mode override
            // must be checked here so that set_global_http2_mode("disabled")
            // prevents H2 connections even for REQUIRED-mode clients
            // (matches legacy connect).
            {
                int global_mode = qore_global_http2_mode.load(
                    std::memory_order_relaxed);
                bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
                bool h2_hard = (http2_mode == HTTP2_MODE_REQUIRED
                        || http2_mode == HTTP2_MODE_H2C_DIRECT)
                    && global_mode != HTTP2_MODE_DISABLED && !lib_disabled;
                bool h2_auto_ssl = http2_mode == HTTP2_MODE_AUTO
                    && connection.ssl
                    && global_mode != HTTP2_MODE_DISABLED && !lib_disabled;

                if (http3_mode.load(std::memory_order_relaxed)
                        == HTTP3_MODE_REQUIRED) {
                    opts.protocol = HttpClientProtocol::H3;
                } else if (h2_hard) {
                    opts.protocol = HttpClientProtocol::H2;
                } else if (h2_auto_ssl) {
                    opts.protocol = HttpClientProtocol::NEGOTIATE;
                } else {
                    opts.protocol = HttpClientProtocol::H1;
                }
            }
            opts.connect_timeout_ms = connect_timeout_ms;
            opts.request_timeout_ms = timeout;
            opts.idle_timeout_ms = 60000;
            // SSL settings from the HTTPClient
            opts.ssl_verify_mode = msock->socket->priv->ssl_verify_mode;
            opts.accept_all_certs = msock->socket->priv->ssl_accept_all_certs;
            opts.client_cert = msock->cert;
            opts.client_key = msock->pk;
            // Proxy URL from the existing connection info
            if (proxy_connection.has_url()) {
                char buf[512];
                snprintf(buf, sizeof(buf), "%s://%s:%d",
                    proxy_connection.ssl ? "https" : "http",
                    proxy_connection.host.c_str(),
                    proxy_connection.port);
                opts.proxy_url = buf;
            }
            conn_mgr.reset(new HttpClientConnectionManagerBase(opts, xsink));
            // New manager: clear user-disconnect flag (which may have been
            // left set by a prior resetConnMgr — see resetConnMgr comments)
            user_disconnect_in_progress.store(false, std::memory_order_release);
            // Mirror the manager's effective protocol to the
            // legacy-style @c http2_active / @c http3_active flags so
            // @c isHttp2Active() / @c isHttp3Active() report correctly
            // for conn_mgr-routed clients (issue 1.5a).  The manager's
            // fixed-protocol model guarantees every connection it creates
            // uses @c opts.protocol — no per-connection variance on a
            // given HTTPClient instance.
            http2_active = (opts.protocol == HttpClientProtocol::H2);
            http3_active = (opts.protocol == HttpClientProtocol::H3);
        }
        return *conn_mgr;
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

    //! Creates a conn_mgr-backed poll operation for startPollSendRecv
    DLLLOCAL QoreObject* startPollSendRecvConnMgr(ExceptionSink* xsink, QoreObject* self,
            QoreHttpClientObject* client, const char* method, const char* path,
            const void* data, size_t size, const QoreHashNode* headers);

    //! Creates a conn_mgr-backed poll operation for startPollConnect
    DLLLOCAL QoreObject* startPollConnectConnMgr(ExceptionSink* xsink, QoreObject* self,
            QoreHttpClientObject* client);

    DLLLOCAL QoreObject* startPollSendRecv(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client,
            const QoreString* method, const QoreString* path, const AbstractQoreNode* data_save, const void* data,
            size_t size, const QoreHashNode* headers, const QoreEncoding* enc = nullptr) {
        int global_mode = qore_global_http2_mode.load(std::memory_order_relaxed);
        bool lib_disabled = qore_check_option(QLO_DISABLE_HTTP2);
        bool h2_enabled = global_mode != HTTP2_MODE_DISABLED && !lib_disabled;
        // H2C_UPGRADE is deprecated by RFC 9113 §3.2 and never implemented;
        // raise the error here so the rejection is consistent across the
        // sync and async entry points regardless of routing.
        if (h2_enabled && http2_mode == HTTP2_MODE_H2C_UPGRADE) {
            xsink->raiseException("HTTP2-ERROR", "h2c-upgrade mode is not supported "
                "(deprecated by RFC 9113); use h2c (h2c-direct) mode for HTTP/2 cleartext "
                "with prior knowledge");
            return nullptr;
        }
        // Delegate to conn_mgr when enabled.  All protocol modes route
        // through conn_mgr now — AUTO+SSL picks HttpClientProtocol::
        // NEGOTIATE in getConnMgr, which drives per-connect ALPN via
        // NegotiatingHttpClientConnection and adopts the result into a
        // concrete H1/H2 connection.  The only legitimate bypass left
        // is the WebSocket upgrade path handled in send_internal.
        // All traffic routes through conn_mgr — no legacy fallback.
        return startPollSendRecvConnMgr(xsink, self, client,
            method->c_str(), path ? path->c_str() : nullptr,
            data, size, headers);
    }

    DLLLOCAL QoreObject* startPollConnect(ExceptionSink* xsink, QoreObject* self, QoreHttpClientObject* client) {
        return startPollConnectConnMgr(xsink, self, client);
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
                    ? http2_mode.load(std::memory_order_relaxed) : HTTP2_MODE_REQUIRED;
            } else {
                // Global auto: use object's setting
                effective_http2_mode = http2_mode.load(std::memory_order_relaxed);
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
        clearStreamingChannel();
    }

    //! User-initiated conn_mgr reset.  Drains the pool so pending poll ops
    //! get errors, and destroys the manager so a fresh one is created on the
    //! next request.  Sets user_disconnect_in_progress which poll ops use
    //! to distinguish user disconnect from other errors.  The flag stays
    //! set until the next new conn_mgr is created (see getConnMgr).
    DLLLOCAL void resetConnMgr() {
        if (conn_mgr) {
            user_disconnect_in_progress.store(true, std::memory_order_release);
            ExceptionSink xsink;
            conn_mgr->closeAll(&xsink);
            conn_mgr.reset();
            // Flag stays set — cleared on next getConnMgr() that creates
            // a new manager.  This ensures poll ops whose futures are
            // rejected asynchronously (after resetConnMgr returns) still
            // see the flag and map HTTP1-ABORT → SOCKET-NOT-OPEN.
            // Clear the protocol-active flags — the next getConnMgr will
            // set them again for the new manager's protocol.
            http2_active = false;
            http3_active = false;
        }
    }

    //! Pre-populate the conn_mgr pool for HTTPClient::connect().
    /** Returns 0 on success, -1 on error (with xsink set).  Called from
        QoreHttpClientObject::connect() when use_conn_mgr is active so
        the legacy HTTPClient::connect() API still does what callers
        expect (fail fast on unreachable server, warm up the socket) but
        uses the conn_mgr pool instead of the legacy msock — so the
        first subsequent send() reuses the same TCP connection.

        Acquires a connection with the configured connect timeout,
        then releases its stream reservation so the next request can
        take it.  @a proxy_connection is checked first so the correct
        scheme/host/port is used for proxied setups.
    */
    DLLLOCAL int connectViaConnMgr(ExceptionSink* xsink) {
        if (!connection.has_url()) {
            xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                "no URL set — cannot connect");
            return -1;
        }
        // Scheme follows the origin: it decides TLS-vs-plain for the
        // conn_mgr pool, and HTTPS-through-proxy is a CONNECT tunnel to
        // the origin that conn_mgr handles internally from the
        // proxy_info it was configured with in getConnMgr().
        const char* scheme = connection.ssl ? "https" : "http";
        HttpClientConnectionManagerBase& mgr = getConnMgr(xsink);
        if (*xsink) {
            return -1;
        }
        HttpClientConnectionBase* conn = mgr.acquireConnection(scheme,
            connection.host.c_str(), connection.port, xsink);
        if (!conn || *xsink) {
            return -1;
        }
        // Release the stream reservation taken by acquireConnection.  The
        // connection stays in the pool; the next send() finds it ready and
        // reserves a stream of its own.
        mgr.releaseConnection(conn);
        return 0;
    }

    //! Clears the streaming receive channel, closing it if open
    DLLLOCAL void clearStreamingChannel() {
        if (streaming_recv_channel) {
            streaming_recv_channel->close();
            ExceptionSink xsink;
            streaming_recv_channel->deref(&xsink);
            streaming_recv_channel = nullptr;
        }
        sse_recv_buffer.clear();
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
            // setPersistent() is a legacy feature that sticks a single
            // msock connection across repeated send() calls.  It has no
            // meaning for the conn_mgr pool (which already keeps
            // connections alive across requests).  If the caller asks
            // for persistence on a client whose connect() went through
            // conn_mgr, drop back to the legacy path: disable conn_mgr
            // for this instance and open an msock connection now.
            // Subsequent send()s route through send_internal's legacy
            // branch, which is what setPersistent() callers expect.
            if (use_conn_mgr) {
                use_conn_mgr = false;
                // Tear down any conn_mgr state so stale pooled
                // connections don't linger for this client.
                resetConnMgr();
            }
            if (connect_unlocked(xsink, connection)) {
                return;
            }
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

    //! Conn_mgr dispatch path for simple H1 request/response (no streaming, no proxy)
    DLLLOCAL QoreHashNode* send_internal_conn_mgr(ExceptionSink* xsink, const char* mname, const char* meth,
        const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body, const void* data,
        unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info,
        int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream* os,
        InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback,
        bool streaming = false);

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
        } else {
            // Cache entry may have been erased by disconnectQuic() during
            // the failed connect attempt.  Insert a backoff-only placeholder
            // with port=0 so parseAltSvc() preserves the backoff (it keeps
            // retry_after_epoch when updating existing entries) even if the
            // next response re-advertises Alt-Svc.  Without this, every
            // H2/HTTPS request to a server advertising a broken h3 endpoint
            // would retry QUIC with the 3s timeout — see:
            // * disconnectQuic() erases cache on any failed connect
            // * parseAltSvc() re-adds from next response header
            // * markAltSvcFailed() previously only set backoff if entry present
            // Result: infinite 3s-per-request loop on H2 keep-alive.
            alt_svc_cache[origin] = AltSvcEntry{0, q_epoch() + 3600,
                q_epoch() + 300};
        }
    }

    // Establish a QUIC/HTTP3 connection to the given server
    // timeout_override_ms: -1 = use connect_timeout_ms; > 0 = use this timeout
    DLLLOCAL int connectQuic(ExceptionSink* xsink, con_info& connection, int timeout_override_ms = -1);

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

    //! Parsed auth challenge from WWW-Authenticate / Proxy-Authenticate header
    struct AuthChallenge {
        std::string scheme;  //!< "Basic" or "Digest"
        std::unordered_map<std::string, std::string> params;  //!< realm, nonce, qop, algorithm, etc.
    };

    //! Parse a WWW-Authenticate or Proxy-Authenticate header value
    /** Supports "Basic realm=..." and "Digest realm=..., nonce=..., ..." formats.
        @param header_value the raw header value
        @return parsed challenge with scheme and parameters
    */
    static AuthChallenge parseAuthChallenge(const char* header_value) {
        AuthChallenge result;
        if (!header_value || !*header_value) {
            return result;
        }

        // Extract scheme (first token before space)
        const char* p = header_value;
        while (*p && *p != ' ') {
            ++p;
        }
        result.scheme.assign(header_value, p - header_value);

        // Normalize scheme to title case
        if (!result.scheme.empty()) {
            result.scheme[0] = toupper(result.scheme[0]);
            for (size_t i = 1; i < result.scheme.size(); ++i) {
                result.scheme[i] = tolower(result.scheme[i]);
            }
        }

        // Skip whitespace after scheme
        while (*p && *p == ' ') {
            ++p;
        }

        // Parse key=value pairs (comma-separated)
        while (*p) {
            // Skip whitespace and commas
            while (*p && (*p == ' ' || *p == ',')) {
                ++p;
            }
            if (!*p) {
                break;
            }

            // Extract key
            const char* key_start = p;
            while (*p && *p != '=' && *p != ' ' && *p != ',') {
                ++p;
            }
            std::string key(key_start, p - key_start);

            // Lowercase the key
            for (auto& c : key) {
                c = tolower(c);
            }

            if (*p != '=') {
                // No value — store as empty
                result.params[key] = "";
                continue;
            }
            ++p;  // skip '='

            // Extract value (possibly quoted)
            std::string value;
            if (*p == '"') {
                ++p;  // skip opening quote
                while (*p && *p != '"') {
                    if (*p == '\\' && *(p + 1)) {
                        ++p;  // skip escape
                    }
                    value += *p++;
                }
                if (*p == '"') {
                    ++p;  // skip closing quote
                }
            } else {
                const char* val_start = p;
                while (*p && *p != ',' && *p != ' ') {
                    ++p;
                }
                value.assign(val_start, p - val_start);
            }
            result.params[key] = value;
        }
        return result;
    }

    //! Compute a hex MD5 or SHA-256 digest of the input string
    /** @param input the string to hash
        @param use_sha256 if true, use SHA-256; otherwise MD5
        @return hex-encoded digest string, or empty on error
    */
    static std::string hexDigest(const std::string& input, bool use_sha256 = false) {
        DigestHelper dh(input.c_str(), input.size());
        const EVP_MD* md = use_sha256 ? EVP_sha256() : EVP_md5();
        if (dh.doDigest("DIGEST-ERROR", md)) {
            return {};
        }
        QoreString hex;
        dh.getString(hex);
        return hex.c_str();
    }

    //! Compute Digest auth response per RFC 7616
    /** @param method HTTP method
        @param uri request URI
        @param username
        @param password
        @param ac parsed auth challenge parameters
        @return the complete "Digest ..." header value, or nullptr on error
    */
    static QoreStringNode* computeDigestAuth(const char* method, const char* uri,
            const char* username, const char* password,
            const AuthChallenge& ac) {
        auto realm_it = ac.params.find("realm");
        auto nonce_it = ac.params.find("nonce");
        if (realm_it == ac.params.end() || nonce_it == ac.params.end()) {
            return nullptr;
        }
        const std::string& realm = realm_it->second;
        const std::string& nonce = nonce_it->second;

        // Check algorithm — default MD5
        bool use_sha256 = false;
        auto algo_it = ac.params.find("algorithm");
        if (algo_it != ac.params.end()) {
            if (strcasecmp(algo_it->second.c_str(), "SHA-256") == 0) {
                use_sha256 = true;
            }
        }

        // Check qop
        auto qop_it = ac.params.find("qop");
        bool has_qop = (qop_it != ac.params.end() && !qop_it->second.empty());

        // Generate cnonce (16 random bytes → hex)
        unsigned char cnonce_bytes[16];
        RAND_bytes(cnonce_bytes, sizeof(cnonce_bytes));
        char cnonce_hex[33];
        for (int i = 0; i < 16; ++i) {
            snprintf(cnonce_hex + i * 2, 3, "%02x", cnonce_bytes[i]);
        }
        std::string cnonce(cnonce_hex, 32);

        // nonce count (always "00000001" — we only retry once)
        const char* nc = "00000001";

        // HA1 = H(username:realm:password)
        std::string a1 = std::string(username) + ":" + realm + ":" + password;
        std::string ha1 = hexDigest(a1, use_sha256);
        if (ha1.empty()) {
            return nullptr;
        }

        // HA2 = H(method:uri)
        std::string a2 = std::string(method) + ":" + uri;
        std::string ha2 = hexDigest(a2, use_sha256);
        if (ha2.empty()) {
            return nullptr;
        }

        // response = H(HA1:nonce:nc:cnonce:qop:HA2) if qop
        //          = H(HA1:nonce:HA2) if no qop
        std::string resp_input;
        if (has_qop) {
            resp_input = ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":auth:" + ha2;
        } else {
            resp_input = ha1 + ":" + nonce + ":" + ha2;
        }
        std::string response = hexDigest(resp_input, use_sha256);
        if (response.empty()) {
            return nullptr;
        }

        // Build the header value
        QoreStringNode* hdr = new QoreStringNode;
        hdr->sprintf("Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"",
            username, realm.c_str(), nonce.c_str(), uri, response.c_str());
        if (has_qop) {
            hdr->sprintf(", qop=auth, nc=%s, cnonce=\"%s\"", nc, cnonce.c_str());
        }
        if (use_sha256) {
            hdr->concat(", algorithm=SHA-256");
        }
        auto opaque_it = ac.params.find("opaque");
        if (opaque_it != ac.params.end()) {
            hdr->sprintf(", opaque=\"%s\"", opaque_it->second.c_str());
        }
        return hdr;
    }

    //! Attempt to compute an auth response for a 401/407 challenge
    /** @param code HTTP status code (401 or 407)
        @param ans response hash with headers
        @param meth HTTP method
        @param mpath request path
        @param nh request headers (will be modified to add auth header)
        @param xsink exception sink
        @return true if an auth header was computed and the request should be retried
    */
    bool tryAuthChallenge(int code, QoreHashNode& ans, const char* meth,
            const char* mpath, QoreHashNode* nh, ExceptionSink* xsink) {
        const char* challenge_hdr = (code == 401) ? "www-authenticate" : "proxy-authenticate";
        const char* auth_hdr = (code == 401) ? "Authorization" : "Proxy-Authorization";
        const con_info& creds = (code == 401) ? connection : proxy_connection;

        if (creds.username.empty()) {
            return false;
        }

        const QoreStringNode* challenge = get_string_header_node(xsink, ans, challenge_hdr);
        if (*xsink || !challenge || challenge->empty()) {
            return false;
        }

        AuthChallenge ac = parseAuthChallenge(challenge->c_str());
        QoreStringNode* auth_value = nullptr;

        if (ac.scheme == "Digest") {
            auth_value = computeDigestAuth(meth, mpath,
                creds.username.c_str(), creds.password.c_str(), ac);
        } else if (ac.scheme == "Basic") {
            QoreString tmp;
            tmp.sprintf("%s:%s", creds.username.c_str(), creds.password.c_str());
            auth_value = new QoreStringNode("Basic ");
            auth_value->concatBase64(&tmp);
        }

        if (!auth_value) {
            return false;
        }

        nh->setKeyValue(auth_hdr, auth_value, xsink);
        return !*xsink;
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

void QoreHttpClientObject::setUseConnectionManager(bool enable) {
    http_priv->use_conn_mgr = enable;
}

bool QoreHttpClientObject::getUseConnectionManager() const {
    return http_priv->use_conn_mgr;
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
        http_priv->http3_mode = HTTP3_MODE_DISABLED;
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
    if (http_priv->http2_active) {
        return true;
    }
    // For NEGOTIATE clients, http2_active may not have been refreshed yet
    // (the poll API's goalReached() can return true before continuePoll
    // runs the refresh).  Check the conn_mgr's pool for H2 connections.
    if (http_priv->conn_mgr) {
        const auto& opts = http_priv->conn_mgr->getOptions();
        if (opts.protocol == HttpClientProtocol::NEGOTIATE) {
            return http_priv->conn_mgr->hasProtocolInPool(HttpClientProtocol::H2);
        }
    }
    return false;
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
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "connect", xsink);
    SafeLocker sl(priv->m);

    if (priv->checkNonBlock(xsink)) {
        return -1;
    }

    // When the conn_mgr is the active dispatch path (use_conn_mgr &&
    // !poll_apis_used), connect() pre-populates the conn_mgr pool instead
    // of opening the legacy msock.  Every subsequent non-WS send() goes
    // through the same pool entry, so there is a single TCP connection
    // per logical session — no more "warm-up msock + real conn_mgr
    // connection" double-connect that hangs single-accept test servers
    // and wastes a socket slot on real peers.  WS upgrade and poll-API
    // paths still use msock, so they stay on the legacy connect_unlocked
    // path below.
    if (http_priv->use_conn_mgr && !http_priv->poll_apis_used
            && http_priv->connection.has_url()) {
        // Drop any stale msock state (prior disconnect/reconnect cycle).
        http_priv->disconnect_unlocked();
        return http_priv->connectViaConnMgr(xsink);
    }

    http_priv->disconnect_unlocked();
    return http_priv->connect_unlocked(xsink, http_priv->connection);
}

void QoreHttpClientObject::disconnect() {
    SafeLocker sl(priv->m);
    http_priv->disconnect_unlocked();
    // User-initiated disconnect: reset conn_mgr so pending poll ops fail
    // with SOCKET-NOT-OPEN and new connections are created on next request
    http_priv->resetConnMgr();
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

    // H2/H3 dispatch was here — removed in Phase 5.  This method is now
    // only reachable from the WebSocket upgrade path (is_ws_upgrade in
    // send_internal), which is always HTTP/1.1.  H2/H3 traffic routes
    // through the conn_mgr (send_internal_conn_mgr).

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
int qore_httpclient_priv::connectQuic(ExceptionSink* xsink, con_info& connection, int timeout_override_ms) {
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
    int effective_timeout = timeout_override_ms > 0 ? timeout_override_ms : connect_timeout_ms;
    int64 deadline_ms = effective_timeout < 0 ? -1 : q_clock_getmillis() + effective_timeout;

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

QoreHashNode* qore_httpclient_priv::send_internal_conn_mgr(ExceptionSink* xsink, const char* mname,
        const char* meth, const char* mpath, const QoreHashNode* headers, const QoreStringNode* msg_body,
        const void* data, unsigned size, const ResolvedCallReferenceNode* send_callback, bool getbody,
        QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback,
        QoreObject* obj, OutputStream* os, InputStream* is, size_t max_chunk_size,
        const ResolvedCallReferenceNode* trailer_callback, bool streaming) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", mname, xsink);

    con_info this_connection = connection;

    bool bodyp = false;
    meth = checkMethod(xsink, meth, bodyp);
    if (*xsink) {
        return nullptr;
    }

    if (!timeout_ms) {
        timeout_ms = timeout;
    }

    // Build request headers
    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        msg_body ? msg_body->getEncoding() : nullptr, (data && size), false,
        keep_alive, host_override), xsink);
    if (*xsink) {
        return nullptr;
    }

    // Prepare body pointer
    const void* body_ptr = nullptr;
    size_t body_len = 0;
    if (msg_body && msg_body->size()) {
        body_ptr = msg_body->c_str();
        body_len = msg_body->size();
    } else if (data && size) {
        body_ptr = data;
        body_len = size;
    }

    // Build path
    QoreString pathstr(enc ? enc : QCS_UTF8);
    bool path_already_encoded = false;

    // Redirect + auth retry loop
    int redirect_count = 0;
    bool auth_retried = false;
    const char* location = nullptr;
    ReferenceHolder<QoreHashNode> ans(xsink);
    int code = 0;

    while (true) {
        const char* msgpath = getMsgPath(xsink, this_connection, mpath, pathstr, path_already_encoded);
        if (*xsink) {
            return nullptr;
        }

        // Determine scheme
        const char* scheme = this_connection.ssl ? "https" : "http";

        // Populate request-uri and request headers in info hash
        if (info) {
            info->setKeyValue("request-uri", new QoreStringNodeMaker("%s %s HTTP/%s", meth,
                msgpath && msgpath[0] ? msgpath : "/", http11 ? "1.1" : "1.0"), xsink);
            if (*xsink) {
                return nullptr;
            }
            // Store a copy of the request headers with Host header added
            // (matching legacy send_internal behavior).  Host is added only
            // to the info copy — the poll op's submitRequest builds its own
            // Host header on the wire from the target host:port.
            ReferenceHolder<QoreHashNode> info_headers(nh->copy(), xsink);
            if (!host_override) {
                info_headers->setKeyValue("Host",
                    getHostHeaderValueUnlocked(this_connection), xsink);
            }
            info->setKeyValue("headers", info_headers.release(), xsink);
            if (*xsink) {
                return nullptr;
            }
        }

        // Submit request via conn_mgr
        HttpClientConnectionManagerBase& mgr = getConnMgr(xsink);
        if (*xsink) {
            return nullptr;
        }

        if (send_callback || is) {
            // Streaming send path: use chunked TE with incremental body push
            // streaming_recv also set when streaming=true (sendAndStream) to
            // use Channel-based delivery for the response
            bool streaming_recv = (recv_callback || os || streaming) ? true : false;

            // Acquire connection manually (can't use mgr.request() because
            // body must be pushed between submit and future-get)
            HttpClientConnectionBase* conn = mgr.acquireConnection(scheme,
                this_connection.host.c_str(), this_connection.port, xsink);
            if (!conn || *xsink) {
                return nullptr;
            }
            // Hold a strong ref — the I/O thread may fire onConnectionClosed
            // (which derefs the pool's ref) while we're blocking on pushSendData
            // or the Future/Channel wait.
            conn->ref();
            ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);

            // Ensure the connection is ready
            conn->waitForReadyOrError(timeout_ms, xsink);
            if (*xsink || conn->isClosed()) {
                if (!*xsink) {
                    xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
                        "connection closed before streaming send request");
                }
                mgr.closeAndEvict(conn, xsink);
                return nullptr;
            }

            // Submit streaming send request via virtual dispatch (H1/H2/H3)
            QoreChannel* channel_raw = nullptr;
            ReferenceHolder<QoreHashNode> submit_result(
                conn->submitRequestStreamingSend(meth, msgpath, *nh,
                    streaming_recv, channel_raw, xsink), xsink);
            if (*xsink || !submit_result) {
                mgr.releaseConnection(conn);
                return nullptr;
            }

            // Push body chunks from send_callback or InputStream
            if (send_callback) {
                while (true) {
                    ValueHolder res(send_callback->execValue(nullptr, xsink), xsink);
                    if (*xsink) {
                        conn->pushSendData(nullptr, 0, xsink);
                        if (channel_raw) {
                            channel_raw->close();
                            channel_raw->deref(xsink);
                        }
                        return nullptr;
                    }

                    bool done = false;
                    switch (res->getType()) {
                        case NT_STRING: {
                            const QoreStringNode* str = res->get<const QoreStringNode>();
                            if (str->empty()) {
                                done = true;
                            } else {
                                conn->pushSendData(str->c_str(), str->size(), xsink);
                                if (*xsink) {
                                    conn->pushSendData(nullptr, 0, xsink);
                                    if (channel_raw) {
                                        channel_raw->close();
                                        channel_raw->deref(xsink);
                                    }
                                    return nullptr;
                                }
                            }
                            break;
                        }
                        case NT_BINARY: {
                            const BinaryNode* b = res->get<const BinaryNode>();
                            if (b->empty()) {
                                done = true;
                            } else {
                                conn->pushSendData(b->getPtr(), b->size(), xsink);
                                if (*xsink) {
                                    conn->pushSendData(nullptr, 0, xsink);
                                    if (channel_raw) {
                                        channel_raw->close();
                                        channel_raw->deref(xsink);
                                    }
                                    return nullptr;
                                }
                            }
                            break;
                        }
                        case NT_NOTHING:
                        case NT_NULL:
                            done = true;
                            break;
                        default:
                            xsink->raiseException("HTTP-CLIENT-CALLBACK-ERROR",
                                "send_callback returned type '%s'; expected "
                                "'string', 'binary', or NOTHING",
                                res->getTypeName());
                            conn->pushSendData(nullptr, 0, xsink);
                            if (channel_raw) {
                                channel_raw->close();
                                channel_raw->deref(xsink);
                            }
                            return nullptr;
                    }

                    if (done) {
                        break;
                    }
                }
            } else if (is) {
                // InputStream path
                size_t chunk_size = max_chunk_size > 0 ? max_chunk_size : 65536;
                while (true) {
                    SimpleRefHolder<BinaryNode> buf(is->readHelper(chunk_size, xsink));
                    if (*xsink) {
                        conn->pushSendData(nullptr, 0, xsink);
                        if (channel_raw) {
                            channel_raw->close();
                            channel_raw->deref(xsink);
                        }
                        return nullptr;
                    }
                    if (!buf || buf->empty()) {
                        break;
                    }
                    conn->pushSendData(buf->getPtr(), buf->size(), xsink);
                    if (*xsink) {
                        conn->pushSendData(nullptr, 0, xsink);
                        if (channel_raw) {
                            channel_raw->close();
                            channel_raw->deref(xsink);
                        }
                        return nullptr;
                    }
                }
            }

            // Set trailers if trailer_callback is provided
            if (trailer_callback) {
                ValueHolder trailer_result(trailer_callback->execValue(nullptr, xsink), xsink);
                if (*xsink) {
                    conn->pushSendData(nullptr, 0, xsink);
                    if (channel_raw) {
                        channel_raw->close();
                        channel_raw->deref(xsink);
                    }
                    return nullptr;
                }
                if (trailer_result->getType() == NT_HASH) {
                    conn->setTrailers(trailer_result->get<const QoreHashNode>(), xsink);
                }
            }

            // Push end sentinel
            conn->pushSendData(nullptr, 0, xsink);

            if (streaming_recv) {
                // Streaming receive: drain channel (same logic as the
                // recv_callback/os path below)
                ReferenceHolder<QoreChannel> channel(channel_raw, xsink);
                bool channel_done = false;
                bool got_headers = false;

                while (true) {
                    bool timed_out = false;
                    bool has_value = false;
                    ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
                    if (*xsink) {
                        channel->close();
                        return nullptr;
                    }
                    if (timed_out) {
                        channel->close();
                        xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                            "timed out after %dms waiting for streaming response", timeout_ms);
                        return nullptr;
                    }
                    if (!has_value) {
                        if (!got_headers) {
                            xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                                "connection closed before response received");
                        }
                        break;
                    }

                    if (rv->getType() != NT_HASH) {
                        continue;
                    }
                    QoreHashNode* h = rv->get<QoreHashNode>();

                    // Check for error
                    QoreValue err_val = h->getKeyValue("err");
                    if (!err_val.isNullOrNothing()) {
                        channel->close();
                        const char* err_str = err_val.getType() == NT_STRING
                            ? err_val.get<const QoreStringNode>()->c_str()
                            : "HTTP-CLIENT-RECEIVE-ERROR";
                        QoreValue desc_val = h->getKeyValue("desc");
                        const char* desc_str = desc_val.getType() == NT_STRING
                            ? desc_val.get<const QoreStringNode>()->c_str()
                            : "streaming request failed";
                        xsink->raiseException(err_str, desc_str);
                        return nullptr;
                    }

                    // Check for response headers
                    QoreValue sc_val = h->getKeyValue("status_code");
                    if (!sc_val.isNullOrNothing() && !got_headers) {
                        got_headers = true;
                        ans = transformConnMgrResponse(h, xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                        code = (int)sc_val.getAsBigInt();

                        if (processContentType(xsink, **ans)) {
                            channel->close();
                            return nullptr;
                        }

                        if (info) {
                            info->setKeyValue("response-headers", ans->refSelf(), xsink);
                            QoreValue raw_hdrs = h->getKeyValue("headers_raw");
                            if (raw_hdrs.getType() == NT_HASH) {
                                info->setKeyValue("response-headers-raw",
                                    raw_hdrs.refSelf(), xsink);
                            }
                            setConnMgrResponseUri(info, h, xsink);
                        }

                        if (recv_callback) {
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("hdr", ans->refSelf(), xsink);
                            cb_arg->setKeyValue("info",
                                info ? info->copy() : nullptr, xsink);
                            cb_arg->setKeyValue("send_aborted", false, xsink);
                            if (obj) {
                                cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                            }
                            args->push(cb_arg, xsink);
                            rv = recv_callback->execValue(*args, xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }

                        // Handle 401/407 auth challenges in streaming path
                        if (!auth_retried && !error_passthru
                                && (code == 401 || code == 407)) {
                            if (tryAuthChallenge(code, **ans, meth, msgpath,
                                    *nh, xsink)) {
                                if (*xsink) {
                                    channel->close();
                                    return nullptr;
                                }
                                auth_retried = true;
                                channel->close();
                                channel_done = true;
                                break;
                            }
                        }

                        if (!redirect_passthru && code >= 300 && code < 400
                                && code != 304) {
                            channel->close();
                            channel_done = true;
                            break;
                        }

                        // sendAndStream mode: store channel for later reads,
                        // don't drain the body
                        if (streaming) {
                            clearStreamingChannel();
                            channel->ref();
                            streaming_recv_channel = *channel;
                            break;
                        }
                        // Fall through to check for body data in the same
                        // hash — H2/H3 streaming send can dispatch headers +
                        // body in a single channel event (when the response
                        // fits in one frame).
                    }

                    // Check for body data
                    QoreValue body_val = h->getKeyValue("body");
                    if (!body_val.isNullOrNothing()) {
                        if (os) {
                            if (body_val.getType() == NT_BINARY) {
                                const BinaryNode* bin = body_val.get<const BinaryNode>();
                                os->write(bin->getPtr(), bin->size(), xsink);
                            } else if (body_val.getType() == NT_STRING) {
                                const QoreStringNode* str =
                                    body_val.get<const QoreStringNode>();
                                os->write(str->c_str(), str->size(), xsink);
                            }
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        } else if (recv_callback) {
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("data", body_val.refSelf(), xsink);
                            cb_arg->setKeyValue("chunked", true, xsink);
                            args->push(cb_arg, xsink);
                            rv = recv_callback->execValue(*args, xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }
                        continue;
                    }

                    // Check for end_stream
                    QoreValue end_val = h->getKeyValue("end_stream");
                    if (!end_val.isNullOrNothing()) {
                        bool is_chunked = false;
                        if (ans) {
                            const char* te = get_string_header(xsink, **ans,
                                "transfer-encoding");
                            if (te && strcasestr(te, "chunked")) {
                                is_chunked = true;
                            }
                            if (*xsink) {
                                xsink->clear();
                            }
                        }
                        if (recv_callback && is_chunked) {
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("send_aborted", false, xsink);
                            if (obj) {
                                cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                            }
                            args->push(cb_arg, xsink);
                            rv = recv_callback->execValue(*args, xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }
                        break;
                    }
                }

                channel->close();

                if (channel_done && !redirect_passthru && code >= 300
                        && code < 400 && code != 304) {
                    // redirect — falls through to redirect block below
                } else {
                    if (!ans) {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "no response received from streaming request");
                        return nullptr;
                    }
                    break;
                }
            } else {
                // Non-streaming receive: block on future
                QoreValue future_v = submit_result->getKeyValue("future");
                if (future_v.getType() != NT_OBJECT) {
                    xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                        "submitRequestStreamingSend result missing 'future' key");
                    return nullptr;
                }
                QoreObject* future_obj = const_cast<QoreObject*>(
                    future_v.get<const QoreObject>());
                future_obj->ref();

                int effective_timeout = timeout_ms > 0 ? timeout_ms : timeout;
                QoreValue result = q_future_get_blocking(future_obj,
                    effective_timeout, xsink);
                future_obj->deref(xsink);

                if (*xsink) {
                    result.discard(xsink);
                    return nullptr;
                }
                if (result.getType() != NT_HASH) {
                    result.discard(xsink);
                    xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                        "Future returned non-hash result type %d",
                        (int)result.getType());
                    return nullptr;
                }

                // Transform to legacy flat format
                ReferenceHolder<QoreHashNode> raw_resp(
                    result.get<QoreHashNode>(), xsink);
                ans = transformConnMgrResponse(*raw_resp, xsink);
                if (*xsink) {
                    return nullptr;
                }

                code = (int)ans->getKeyValue("status_code").getAsBigInt();

                if (info) {
                    info->setKeyValue("response-headers", ans->refSelf(), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                    QoreValue raw_hdrs = raw_resp->getKeyValue("headers_raw");
                    if (raw_hdrs.getType() == NT_HASH) {
                        info->setKeyValue("response-headers-raw",
                            raw_hdrs.refSelf(), xsink);
                        if (*xsink) {
                            return nullptr;
                        }
                    }
                    setConnMgrResponseUri(info, *raw_resp, xsink);
                }

                if (!ans->is_unique()) {
                    ans = ans->copy();
                }
            }
        } else if (recv_callback || os) {
            // Streaming receive path: use Channel-based delivery
            QoreChannel* channel_raw = nullptr;
            int64_t stream_id = mgr.requestStreaming(meth, scheme,
                this_connection.host.c_str(), this_connection.port,
                msgpath, *nh, body_ptr, body_len, channel_raw, xsink);
            if (*xsink || stream_id < 0 || !channel_raw) {
                return nullptr;
            }
            // ReferenceHolder ensures deref on all exit paths
            ReferenceHolder<QoreChannel> channel(channel_raw, xsink);
            bool channel_done = false;

            // Channel read loop: process streaming response
            bool got_headers = false;
            // Body accumulation for non-chunked + content-encoding case
            // (matches legacy: read full body, decompress, single callback)
            bool is_chunked_response = false;
            std::string resp_content_encoding;
            SimpleRefHolder<BinaryNode> accumulated_body;
            while (true) {
                bool timed_out = false;
                bool has_value = false;
                ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
                if (*xsink) {
                    channel->close();
                    return nullptr;
                }
                if (timed_out) {
                    channel->close();
                    xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                        "timed out after %dms waiting for streaming response", timeout_ms);
                    return nullptr;
                }
                if (!has_value) {
                    // Channel closed with no more data
                    if (!got_headers) {
                        xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                            "connection closed before response received");
                    }
                    break;
                }

                if (rv->getType() != NT_HASH) {
                    continue;
                }
                QoreHashNode* h = rv->get<QoreHashNode>();

                // Check for error
                QoreValue err_val = h->getKeyValue("err");
                if (!err_val.isNullOrNothing()) {
                    channel->close();
                    const char* err_str = err_val.getType() == NT_STRING
                        ? err_val.get<const QoreStringNode>()->c_str() : "HTTP-CLIENT-RECEIVE-ERROR";
                    QoreValue desc_val = h->getKeyValue("desc");
                    const char* desc_str = desc_val.getType() == NT_STRING
                        ? desc_val.get<const QoreStringNode>()->c_str() : "streaming request failed";
                    xsink->raiseException(err_str, desc_str);
                    return nullptr;
                }

                // Check for response headers
                QoreValue sc_val = h->getKeyValue("status_code");
                if (!sc_val.isNullOrNothing() && !got_headers) {
                    got_headers = true;
                    // Transform to flat format for redirect/error processing
                    ans = transformConnMgrResponse(h, xsink);
                    if (*xsink) {
                        channel->close();
                        return nullptr;
                    }
                    code = (int)sc_val.getAsBigInt();

                    // Process content-type (sets _qore_orig_content_type, charset)
                    if (processContentType(xsink, **ans)) {
                        channel->close();
                        return nullptr;
                    }

                    if (info) {
                        info->setKeyValue("response-headers", ans->refSelf(), xsink);
                        QoreValue raw_hdrs = h->getKeyValue("headers_raw");
                        if (raw_hdrs.getType() == NT_HASH) {
                            info->setKeyValue("response-headers-raw", raw_hdrs.refSelf(), xsink);
                        }
                        setConnMgrResponseUri(info, h, xsink);
                    }

                    // Determine response transfer-encoding and content-encoding
                    // for body delivery decisions
                    {
                        const char* te = get_string_header(xsink, **ans, "transfer-encoding");
                        if (te && strcasestr(te, "chunked")) {
                            is_chunked_response = true;
                        }
                        if (*xsink) {
                            xsink->clear();
                        }
                        const char* ce = get_string_header(xsink, **ans, "content-encoding");
                        if (ce && *ce && strcasecmp(ce, "identity")) {
                            resp_content_encoding = ce;
                        }
                        if (*xsink) {
                            xsink->clear();
                        }
                        // For non-chunked + content-encoding + recv_callback,
                        // accumulate body to decompress at end (matches
                        // legacy send_internal behavior — see line ~9070)
                        if (recv_callback && !is_chunked_response
                                && !resp_content_encoding.empty()) {
                            accumulated_body = new BinaryNode();
                        }
                    }

                    // Invoke recv_callback with header hash (matching
                    // runHeaderCallback format: single hash arg with keys
                    // hdr, info, send_aborted, obj)
                    if (recv_callback) {
                        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                        QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                        cb_arg->setKeyValue("hdr", ans->refSelf(), xsink);
                        cb_arg->setKeyValue("info", info ? info->copy() : nullptr, xsink);
                        cb_arg->setKeyValue("send_aborted", false, xsink);
                        if (obj) {
                            cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                        }
                        args->push(cb_arg, xsink);
                        rv = recv_callback->execValue(*args, xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    }

                    // Handle 401/407 auth challenges
                    if (!auth_retried && !error_passthru
                            && (code == 401 || code == 407)) {
                        if (tryAuthChallenge(code, **ans, meth, msgpath,
                                *nh, xsink)) {
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                            auth_retried = true;
                            channel->close();
                            channel_done = true;
                            break;
                        }
                    }

                    // Check for redirect
                    if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
                        // Drain and close channel, then reloop
                        channel->close();
                        channel_done = true;
                        break;  // break out of channel loop; redirect handling below
                    }
                    continue;
                }

                // Check for body data
                QoreValue body_val = h->getKeyValue("body");
                if (!body_val.isNullOrNothing()) {
                    if (os) {
                        // Write to OutputStream
                        if (body_val.getType() == NT_BINARY) {
                            const BinaryNode* bin = body_val.get<const BinaryNode>();
                            os->write(bin->getPtr(), bin->size(), xsink);
                        } else if (body_val.getType() == NT_STRING) {
                            const QoreStringNode* str = body_val.get<const QoreStringNode>();
                            os->write(str->c_str(), str->size(), xsink);
                        }
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    } else if (recv_callback) {
                        if (accumulated_body) {
                            // Buffer for decompression at end_stream
                            if (body_val.getType() == NT_BINARY) {
                                const BinaryNode* bin = body_val.get<const BinaryNode>();
                                accumulated_body->append(bin->getPtr(), bin->size());
                            } else if (body_val.getType() == NT_STRING) {
                                const QoreStringNode* str = body_val.get<const QoreStringNode>();
                                accumulated_body->append(str->c_str(), str->size());
                            }
                        } else {
                            // Per-chunk callback.  Convert binary→string
                            // when no content-encoding (matches legacy
                            // readHttpChunkedBody behavior).
                            QoreValue cb_data;
                            if (resp_content_encoding.empty()
                                    && body_val.getType() == NT_BINARY) {
                                const BinaryNode* bin = body_val.get<const BinaryNode>();
                                cb_data = new QoreStringNode(
                                    (const char*)bin->getPtr(), bin->size(),
                                    QCS_UTF8);
                            } else {
                                cb_data = body_val.refSelf();
                            }
                            ReferenceHolder<QoreListNode> args(
                                new QoreListNode(autoTypeInfo), xsink);
                            QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                            cb_arg->setKeyValue("data", cb_data, xsink);
                            cb_arg->setKeyValue("chunked", true, xsink);
                            args->push(cb_arg, xsink);
                            rv = recv_callback->execValue(*args, xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                        }
                    }
                    continue;
                }

                // Check for end_stream
                QoreValue end_val = h->getKeyValue("end_stream");
                if (!end_val.isNullOrNothing()) {
                    // For non-chunked + content-encoding case, decompress
                    // accumulated body and deliver as single data callback
                    // (matches legacy send_internal behavior at line ~9070)
                    if (recv_callback && accumulated_body) {
                        bool ignore_encoding = false;
                        qore_uncompress_to_string_t dec =
                            get_decoder_for_content_encoding(
                                resp_content_encoding.c_str(), ignore_encoding);
                        QoreValue cb_data;
                        if (dec && !ignore_encoding && accumulated_body->size()) {
                            QoreStringNode* decoded = dec(*accumulated_body,
                                QCS_UTF8, xsink);
                            if (*xsink) {
                                channel->close();
                                return nullptr;
                            }
                            cb_data = decoded;
                        } else {
                            // Unknown encoding or empty body — pass raw binary
                            cb_data = accumulated_body.release();
                        }
                        ReferenceHolder<QoreListNode> args(
                            new QoreListNode(autoTypeInfo), xsink);
                        QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                        cb_arg->setKeyValue("data", cb_data, xsink);
                        cb_arg->setKeyValue("chunked", false, xsink);
                        args->push(cb_arg, xsink);
                        rv = recv_callback->execValue(*args, xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    }
                    // Final callback only for chunked responses (matching
                    // legacy readHttpChunkedBody[Binary] which calls
                    // runHeaderCallback at end).  Content-length responses
                    // only get the initial header callback.
                    bool is_chunked = false;
                    if (ans) {
                        const char* te = get_string_header(xsink, **ans, "transfer-encoding");
                        if (te && strcasestr(te, "chunked")) {
                            is_chunked = true;
                        }
                        if (*xsink) {
                            xsink->clear();
                        }
                    }
                    if (recv_callback && is_chunked) {
                        ReferenceHolder<QoreListNode> args(new QoreListNode(autoTypeInfo), xsink);
                        QoreHashNode* cb_arg = new QoreHashNode(autoTypeInfo);
                        // hdr = NOTHING (no trailers)
                        cb_arg->setKeyValue("send_aborted", false, xsink);
                        if (obj) {
                            cb_arg->setKeyValue("obj", obj->refSelf(), xsink);
                        }
                        args->push(cb_arg, xsink);
                        rv = recv_callback->execValue(*args, xsink);
                        if (*xsink) {
                            channel->close();
                            return nullptr;
                        }
                    }
                    break;
                }
            }

            // Close the channel now that we're done reading
            channel->close();

            // If we broke out for a redirect, handle it
            if (channel_done && !redirect_passthru && code >= 300 && code < 400 && code != 304) {
                // redirect handling falls through to the redirect block below
            } else {
                // Non-redirect: streaming is complete, return the headers
                if (!ans) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                        "no response received from streaming request");
                    return nullptr;
                }
                break;
            }
        } else if (streaming) {
            // sendAndStream path: submit as streaming, read only headers,
            // store the channel for later readHTTPChunk/readServerSentEvent
            QoreChannel* channel_raw = nullptr;
            int64_t stream_id = mgr.requestStreaming(meth, scheme,
                this_connection.host.c_str(), this_connection.port,
                msgpath, *nh, body_ptr, body_len, channel_raw, xsink);
            if (*xsink || stream_id < 0 || !channel_raw) {
                return nullptr;
            }
            ReferenceHolder<QoreChannel> channel(channel_raw, xsink);
            bool channel_done = false;

            // Read only headers from the channel — body stays for later reads
            while (true) {
                bool timed_out = false;
                bool has_value = false;
                ValueHolder rv(channel->recv(timeout_ms, xsink, timed_out, has_value), xsink);
                if (*xsink) {
                    channel->close();
                    return nullptr;
                }
                if (timed_out) {
                    channel->close();
                    xsink->raiseException("HTTP-CLIENT-TIMEOUT",
                        "timed out after %dms waiting for streaming response headers",
                        timeout_ms);
                    return nullptr;
                }
                if (!has_value) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                        "connection closed before response headers received");
                    return nullptr;
                }

                if (rv->getType() != NT_HASH) {
                    continue;
                }
                QoreHashNode* h = rv->get<QoreHashNode>();

                // Check for error
                QoreValue err_val = h->getKeyValue("err");
                if (!err_val.isNullOrNothing()) {
                    channel->close();
                    const char* err_str = err_val.getType() == NT_STRING
                        ? err_val.get<const QoreStringNode>()->c_str()
                        : "HTTP-CLIENT-RECEIVE-ERROR";
                    QoreValue desc_val = h->getKeyValue("desc");
                    const char* desc_str = desc_val.getType() == NT_STRING
                        ? desc_val.get<const QoreStringNode>()->c_str()
                        : "streaming request failed";
                    xsink->raiseException(err_str, desc_str);
                    return nullptr;
                }

                // Check for response headers
                QoreValue sc_val = h->getKeyValue("status_code");
                if (!sc_val.isNullOrNothing()) {
                    ans = transformConnMgrResponse(h, xsink);
                    if (*xsink) {
                        channel->close();
                        return nullptr;
                    }
                    code = (int)sc_val.getAsBigInt();

                    if (processContentType(xsink, **ans)) {
                        channel->close();
                        return nullptr;
                    }

                    if (info) {
                        info->setKeyValue("response-headers", ans->refSelf(), xsink);
                        QoreValue raw_hdrs = h->getKeyValue("headers_raw");
                        if (raw_hdrs.getType() == NT_HASH) {
                            info->setKeyValue("response-headers-raw",
                                raw_hdrs.refSelf(), xsink);
                        }
                        setConnMgrResponseUri(info, h, xsink);
                    }

                    // Check for redirect
                    if (!redirect_passthru && code >= 300 && code < 400
                            && code != 304) {
                        channel->close();
                        channel_done = true;
                    } else {
                        // Store the channel for later reads via
                        // readHTTPChunk / readServerSentEvent
                        clearStreamingChannel();
                        channel->ref();
                        streaming_recv_channel = *channel;
                    }
                    break;
                }
            }

            if (channel_done && !redirect_passthru && code >= 300
                    && code < 400 && code != 304) {
                // redirect — falls through to redirect block below
            } else {
                if (!ans) {
                    xsink->raiseException("HTTP-CLIENT-RECEIVE-ERROR",
                        "no response received from streaming request");
                    return nullptr;
                }
                break;
            }
        } else {
            // Non-streaming path: submit and await complete response
            ReferenceHolder<QoreHashNode> raw_resp(
                mgr.request(meth, scheme, this_connection.host.c_str(), this_connection.port,
                    msgpath, *nh, body_ptr, body_len, timeout_ms, xsink),
                xsink);
            if (!raw_resp || *xsink) {
                return nullptr;
            }

            // Transform to legacy flat format
            ans = transformConnMgrResponse(*raw_resp, xsink);
            if (*xsink) {
                return nullptr;
            }

            code = (int)ans->getKeyValue("status_code").getAsBigInt();

            if (info) {
                info->setKeyValue("response-headers", ans->refSelf(), xsink);
                if (*xsink) {
                    return nullptr;
                }
                // Populate response-headers-raw with original-case keys
                QoreValue raw_hdrs = raw_resp->getKeyValue("headers_raw");
                if (raw_hdrs.getType() == NT_HASH) {
                    info->setKeyValue("response-headers-raw", raw_hdrs.refSelf(), xsink);
                    if (*xsink) {
                        return nullptr;
                    }
                }
                setConnMgrResponseUri(info, *raw_resp, xsink);
            }

            if (!ans->is_unique()) {
                ans = ans->copy();
            }
        }

        // Fire HTTP events on the HTTPClient's event queue (matching legacy
        // send_internal behavior).  The conn_mgr path uses its own sockets
        // for I/O, but events are fired on msock's queue for user visibility.
        {
            Queue* event_queue = msock->socket->getQueue();
            if (event_queue) {
                const char* cl = get_string_header(xsink, **ans, "content-length");
                if (!*xsink && cl) {
                    ssize_t len = strtoll(cl, nullptr, 10);
                    do_content_length_event(event_queue, msock->socket->priv, len);
                }
                if (*xsink) {
                    return nullptr;
                }
            }
        }

        // Handle 401/407 auth challenges — retry once with computed credentials
        if (!auth_retried && !error_passthru && (code == 401 || code == 407)) {
            if (tryAuthChallenge(code, **ans, meth, msgpath, *nh, xsink)) {
                if (*xsink) {
                    return nullptr;
                }
                auth_retried = true;
                continue;
            }
        }

        // Handle 3xx redirects (304 Not Modified passes through)
        if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
            host_override = false;
            const QoreStringNode* mess = ans->getKeyValue("status_message").get<QoreStringNode>();

            const QoreStringNode* loc = get_string_header_node(xsink, **ans, "location");
            if (*xsink) {
                return nullptr;
            }
            location = loc && !loc->empty() ? loc->c_str() : nullptr;
            if (!location) {
                const char* msg = mess ? mess->c_str() : "<no message>";
                xsink->raiseException("HTTP-CLIENT-REDIRECT-ERROR",
                    "no redirect location given for status code %d: message: '%s'", code, msg);
                return nullptr;
            }

            if (++redirect_count > max_redirects) {
                break;
            }

            // Fire redirect event on the HTTPClient's event queue
            {
                Queue* event_queue = msock->socket->getQueue();
                if (event_queue) {
                    do_redirect_event(event_queue, msock->socket->priv,
                        loc, mess);
                }
            }

            if (redirectUrlUnlocked(location, this_connection, xsink)) {
                const char* msg = mess ? mess->c_str() : "<no message>";
                xsink->appendLastDescription(": while setting URL for redirect location '%s' (code %d: "
                    "message: '%s')", location, code, msg);
                return nullptr;
            }
            if (!path_already_encoded) {
                path_already_encoded = true;
            }

            // Set redirect info in info hash if present
            if (info) {
                QoreString tmp;
                tmp.sprintf("redirect-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), loc->refSelf(), xsink);
                if (*xsink) {
                    return nullptr;
                }

                tmp.clear();
                tmp.sprintf("redirect-message-%d", redirect_count);
                info->setKeyValue(tmp.c_str(), mess ? mess->refSelf() : QoreValue(), xsink);
            }

            // Use updated connection path for the next iteration
            mpath = nullptr;
            continue;
        }

        break;
    }

    // Check for max redirects exceeded
    if (!redirect_passthru && code >= 300 && code < 400 && code != 304) {
        const char* mess = get_string_header(xsink, **ans, "status_message");
        if (!mess) {
            mess = "<no message>";
        }
        if (!location) {
            location = "<no location>";
        }
        xsink->raiseException("HTTP-CLIENT-MAXIMUM-REDIRECTS-EXCEEDED",
            "maximum redirections (%d) exceeded; redirect code %d to '%s' ignored (message: '%s')",
            max_redirects, code, location, mess);
        return nullptr;
    }

    // Refresh http2_active for NEGOTIATE clients: the first successful
    // response reveals the actual protocol the conn_mgr selected.  Phase 6
    // of design/conn-mgr-alpn-negotiation.md (Option β).
    {
        QoreValue proto = ans->getKeyValue("protocol");
        if (proto.getType() == NT_STRING) {
            const char* p = proto.get<const QoreStringNode>()->c_str();
            if (!strcmp(p, "h2")) {
                http2_active = true;
            } else if (!strcmp(p, "h3")) {
                http3_active = true;
            }
        }
    }

    // Process content-type (skip for streaming path — already done in channel bridge)
    if (!recv_callback && !os) {
        if (processContentType(xsink, **ans)) {
            return nullptr;
        }
    }

    // Handle body content-encoding (skip for streaming — body already delivered)
    QoreValue body_val = (!recv_callback && !os) ? ans->getKeyValue("body") : QoreValue();
    if (!body_val.isNullOrNothing() && body_val.getType() == NT_BINARY) {
        const BinaryNode* bin = body_val.get<const BinaryNode>();
        if (bin && bin->size()) {
            qore_uncompress_to_string_t dec = nullptr;
            const char* content_encoding = normalizeContentEncoding(xsink, **ans, false, dec);
            if (*xsink) {
                return nullptr;
            }

            // Use default encoding from the HTTPClient
            const QoreEncoding* body_enc = enc ? enc : QCS_UTF8;
            QoreValue processed = process_binary_body(bin, body_enc, content_encoding, dec,
                encoding_passthru, xsink);
            if (*xsink) {
                return nullptr;
            }
            if (processed) {
                ans->setKeyValue("body", processed.getInternalNode(), xsink);
            }
        }
    }

    // Populate info hash with response body
    if (info) {
        QoreValue bv = ans->getKeyValue("body");
        if (!bv.isNullOrNothing()) {
            info->setKeyValue("response-body", bv.refSelf(), xsink);
        }
    }

    // Check error status codes
    if (!error_passthru && !*xsink && (code < 100 || code >= 300)) {
        const char* mess = get_string_header(xsink, **ans, "status_message");
        if (!mess) {
            mess = "<no message>";
        }
        assert(!*xsink);
        xsink->raiseExceptionArg("HTTP-CLIENT-RECEIVE-ERROR", ans.release(),
            "HTTP status code %d received: message: %s", code, mess);
        return nullptr;
    }

    return *xsink ? nullptr : ans.release();
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
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", mname, xsink);

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

    // H2C_UPGRADE is deprecated by RFC 9113 §3.2 and was never implemented;
    // raise the deprecated-mode error up front so the rejection does not
    // depend on which code path would have handled the request.
    {
        int gm = qore_global_http2_mode.load(std::memory_order_relaxed);
        bool ld = qore_check_option(QLO_DISABLE_HTTP2);
        if (gm != HTTP2_MODE_DISABLED && !ld
                && http2_mode == HTTP2_MODE_H2C_UPGRADE) {
            xsink->raiseException("HTTP2-ERROR", "h2c-upgrade mode is not supported "
                "(deprecated by RFC 9113); use h2c (h2c-direct) mode for HTTP/2 cleartext "
                "with prior knowledge");
            return nullptr;
        }
    }
    // Delegate to conn_mgr when enabled.  The only remaining bypass is
    // WebSocket upgrade requests (Connection: Upgrade + Upgrade:
    // websocket header): after the 101 Switching Protocols response,
    // the same TCP socket carries the upgraded WebSocket protocol, and
    // conn_mgr returns pooled connections to the pool after each
    // response — breaking the WS upgrade contract.  The legacy path
    // leaves msock connected so the caller can drive the WS frames
    // over it.  AUTO+SSL used to bypass to the legacy path as well;
    // the conn_mgr now handles it via NEGOTIATE (see getConnMgr and
    // design/conn-mgr-alpn-negotiation.md).
    {
        bool is_ws_upgrade = false;
        if (headers) {
            // HTTP headers are case-insensitive — scan the hash looking for
            // "Connection: upgrade" + "Upgrade: websocket" irrespective of
            // the caller's header-key case.
            bool has_conn_upgrade = false;
            bool has_upgrade_ws = false;
            ConstHashIterator hi(headers);
            while (hi.next()) {
                const char* key = hi.getKey();
                QoreValue v = hi.get();
                if (v.getType() != NT_STRING) {
                    continue;
                }
                const char* val = v.get<const QoreStringNode>()->c_str();
                if (!strcasecmp(key, "Connection") && val
                        && strcasestr(val, "upgrade")) {
                    has_conn_upgrade = true;
                } else if (!strcasecmp(key, "Upgrade") && val
                        && !strcasecmp(val, "websocket")) {
                    has_upgrade_ws = true;
                }
            }
            is_ws_upgrade = has_conn_upgrade && has_upgrade_ws;
        }
        if (use_conn_mgr && !is_ws_upgrade) {
            return send_internal_conn_mgr(xsink, mname, meth, mpath, headers,
                msg_body, data, size, send_callback, getbody, info, timeout_ms,
                recv_callback, obj, os, is, max_chunk_size, trailer_callback, streaming);
        }
    }

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

// --- conn_mgr-backed poll operations ---

extern QoreClass* QC_EVENTNOTIFIER;

//! Poll operation that delegates to the conn_mgr for startPollSendRecv/Connect
class HttpClientConnMgrPollOp : public SocketPollOperationBase {
public:
    //! Two-phase async state for sendRecv mode.
    /** A fresh (CONNECTING) connection starts in WAITING_CONNECT; when the
        notifier signals READY, the request is submitted and the op moves
        to WAITING_RESPONSE.  A reused pool connection (already READY) skips
        WAITING_CONNECT and starts in WAITING_RESPONSE.  Connect-only mode
        (@ref connect_mode) does not use this enum — it uses the legacy
        done/notifier path.
    */
    enum class Phase { WAITING_CONNECT, WAITING_RESPONSE, DONE };

    //! Constructor for sendRecv mode — fast path (connection already READY)
    HttpClientConnMgrPollOp(QoreFuture* future, QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref = nullptr,
            QoreObject* client_obj = nullptr)
        : future(future), notifier(notifier), priv_ref(priv_ref),
          client_obj(client_obj),
          phase(Phase::WAITING_RESPONSE) {
        assert(notifier);
        if (future) {
            future->ref();
        }
        notifier->ref();
        if (client_obj) {
            client_obj->ref();
        }
    }

    //! Constructor for sendRecv mode — async path (connection still CONNECTING)
    /** All pending_* refs are CONSUMED (the constructor takes ownership):
        the caller passes already-ref'd pointers and does NOT deref on
        failure-after-this-call.  If construction itself fails (xsink only),
        the destructor cleans up everything.

        @param pending_conn connection in CONNECTING state — borrowed from
            @a pending_mgr's pool; the poll op releases its stream reservation
            if the op is destroyed before the request is submitted
        @param pending_mgr back-ref for stream-reservation release
        @param method request method (copied)
        @param path request path (copied)
        @param pending_headers request headers — ref CONSUMED (may be nullptr)
        @param pending_body request body as BinaryNode — ref CONSUMED (may be
            nullptr if no body); must own its backing buffer
        @param pending_promise promise paired with @a future — ref CONSUMED
        @param future future that resolves on response — poll op takes its
            own ref
        @param notifier event notifier used both for connection-ready wake
            and for response completion — poll op takes its own ref
        @param priv_ref HTTPClient priv back-reference
        @param client_obj HTTPClient QoreObject — poll op takes its own ref
    */
    HttpClientConnMgrPollOp(HttpClientConnectionBase* pending_conn,
            HttpClientConnectionManagerBase* pending_mgr,
            std::string method, std::string path,
            QoreHashNode* pending_headers,
            BinaryNode* pending_body,
            QorePromise* pending_promise,
            QoreFuture* future, QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref,
            QoreObject* client_obj)
        : future(future), notifier(notifier), priv_ref(priv_ref),
          client_obj(client_obj),
          phase(Phase::WAITING_CONNECT),
          pending_conn(pending_conn),
          pending_mgr(pending_mgr),
          pending_method(std::move(method)),
          pending_path(std::move(path)),
          pending_headers(pending_headers),
          pending_body(pending_body),
          pending_promise(pending_promise) {
        assert(notifier);
        assert(pending_conn);
        assert(pending_mgr);
        assert(pending_promise);
        assert(future);
        future->ref();
        notifier->ref();
        // Ref the pending connection to keep its priv data (including the
        // AbstractHttpPollConnectionPriv state) alive until continuePoll
        // inspects it.  Without this, a fast connect failure can evict the
        // connection from the pool and free it before our continuePoll runs,
        // leaving pending_conn pointing at freed memory.  Released in
        // clearPending() via pending_mgr->releaseConnection (stream slot)
        // and explicit pending_conn->deref().
        pending_conn->ref();
        if (client_obj) {
            client_obj->ref();
        }
        // pending_headers, pending_body, pending_promise refs were already
        // consumed by the caller — no ref() here.
    }

    //! Constructor for connect mode (already ready — signals immediately)
    HttpClientConnMgrPollOp(QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref = nullptr)
        : notifier(notifier), connect_mode(true), priv_ref(priv_ref) {
        if (notifier) {
            notifier->ref();
            notifier->notify();  // signal immediately so first continuePoll completes
        }
    }

    //! Constructor for deferred error mode
    HttpClientConnMgrPollOp(const char* err, const char* desc,
            QoreEventNotifier* notifier,
            qore_httpclient_priv* priv_ref = nullptr)
        : notifier(notifier), connect_mode(true), priv_ref(priv_ref),
          deferred_err(err), deferred_desc(desc) {
        if (notifier) {
            notifier->ref();
        }
    }

    ~HttpClientConnMgrPollOp() override {
        // Refcount is 0 at dtor — no other thread can hold a reference, so
        // we skip op_lock here.  Acquiring it would be wasted work and also
        // risk asserting under a debug mutex if the lock is ever promoted.
        ExceptionSink xsink;
        clearPendingUnlocked(&xsink);
        if (future) {
            future->deref(&xsink);
            future = nullptr;
        }
        if (notifier) {
            notifier->deref(&xsink);
            notifier = nullptr;
        }
        if (client_obj) {
            client_obj->deref(&xsink);
            client_obj = nullptr;
        }
    }

    //! Releases pending-state refs and the stream reservation.
    /** Called when the poll op is destroyed, aborted, or has successfully
        transitioned out of WAITING_CONNECT.  After a successful submit in
        WAITING_CONNECT → WAITING_RESPONSE, the caller nulls @ref pending_conn
        and @ref pending_mgr because @c submitRequestWithAction releases the
        stream reservation internally; if those pointers are still set here
        the reservation is released.

        @par Ownership contract
        pending_headers, pending_body, pending_promise refs were consumed by
        the WAITING_CONNECT constructor; they are owned by the poll op until
        cleared here.

        @par Lock contract
        Must be called with @ref op_lock held.  See op_lock comment for the
        race this closes.
    */
    void clearPendingUnlocked(ExceptionSink* xsink) {
        if (pending_conn && pending_mgr) {
            pending_mgr->releaseConnection(pending_conn);
        }
        if (pending_conn) {
            // Release the ref acquired in the WAITING_CONNECT constructor
            pending_conn->deref(xsink);
            pending_conn = nullptr;
        }
        pending_mgr = nullptr;
        if (pending_headers) {
            pending_headers->deref(xsink);
            pending_headers = nullptr;
        }
        if (pending_body) {
            pending_body->deref();
            pending_body = nullptr;
        }
        if (pending_promise) {
            pending_promise->deref(xsink);
            pending_promise = nullptr;
        }
    }

    void setConnectMode() { connect_mode = true; }

    bool goalReached() const override {
        return done || (future && future->isDone());
    }

    QoreHashNode* continuePoll(ExceptionSink* xsink) override {
        // Serialize against concurrent abort().  The I/O controller's
        // Phase 2 uses a raw @c spop_base pointer captured during Phase 1,
        // so @c this stays alive until Phase 3 — but another thread can
        // still invoke abort() on us (e.g., via DelegatingPollOperation
        // dispatched abort from the controller's shutdown or an application
        // cancel path that routes evalMethod through a worker).  Without
        // this lock a racing abort nulls @ref pending_conn between
        // continueWaitingConnect's isReady() check and its pending_conn
        // deref — ASAN-confirmed SEGV at pending_conn=NULL in
        // continueWaitingConnect under QORE_IO_THREADS>=2.
        AutoLocker al(op_lock);
        // Check if HTTPClient has been deleted (matches legacy semantics
        // where continuePoll on a poll op with a deleted client throws).
        if (client_obj && !client_obj->isValid()) {
            xsink->raiseException("OBJECT-ALREADY-DELETED",
                "HTTPClient has been deleted; poll operation aborted");
            done = true;
            phase = Phase::DONE;
            // The manager is owned by the HTTPClient that was destroyed —
            // don't call releaseConnection on it.  But our own ref on
            // pending_conn (acquired in the WAITING_CONNECT ctor) is still
            // valid via the connection's own refcount, so deref it here.
            if (pending_conn) {
                pending_conn->deref(xsink);
                pending_conn = nullptr;
            }
            pending_mgr = nullptr;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // Check for deferred connection error
        if (!deferred_err.empty()) {
            xsink->raiseException(deferred_err.c_str(), deferred_desc.c_str());
            done = true;
            return nullptr;
        }
        if (done) {
            return nullptr;
        }

        // Phase 1: WAITING_CONNECT — wait for pending_conn to transition
        // READY (then submit the request) or CLOSED (surface error).
        if (phase == Phase::WAITING_CONNECT) {
            return continueWaitingConnect(xsink);
        }

        if (future && future->isDone()) {
            done = true;
            phase = Phase::DONE;
            if (notifier) {
                notifier->acknowledge(xsink);
            }
            // Refresh http2_active for NEGOTIATE clients (Phase 6):
            // peek the future's value for the "protocol" field without
            // consuming it (the caller reads via getOutput later).
            if (priv_ref && !future->isError()) {
                ExceptionSink peek_xsink;
                QoreValue pv = future->get(0, &peek_xsink);
                if (!peek_xsink && pv.getType() == NT_HASH) {
                    QoreValue proto = pv.get<QoreHashNode>()->getKeyValue("protocol");
                    if (proto.getType() == NT_STRING) {
                        const char* p = proto.get<const QoreStringNode>()->c_str();
                        if (!strcmp(p, "h2")) {
                            priv_ref->http2_active = true;
                        }
                    }
                }
                pv.discard(&peek_xsink);
                peek_xsink.clear();
            }
            // If the future was rejected (error), surface the error.
            // Map HTTP1-ABORT to SOCKET-NOT-OPEN only when a user-initiated
            // disconnect is in progress (priv_ref->user_disconnect_in_progress).
            // Other HTTP1-ABORT errors (e.g., from internal cleanup) pass
            // through with their original error code.
            if (future->isError()) {
                ExceptionSink err_xsink;
                future->get(0, &err_xsink);
                if (err_xsink.isException()) {
                    QoreValue err_val = err_xsink.getExceptionErr();
                    bool mapped = false;
                    if (err_val.getType() == NT_STRING && priv_ref
                            && priv_ref->user_disconnect_in_progress.load(
                                std::memory_order_acquire)) {
                        const char* err = err_val.get<const QoreStringNode>()->c_str();
                        if (!strcmp(err, "HTTP1-ABORT")) {
                            xsink->raiseException("SOCKET-NOT-OPEN",
                                "socket disconnected during poll operation");
                            mapped = true;
                        }
                    }
                    if (!mapped) {
                        xsink->assimilate(err_xsink);
                    }
                    err_xsink.clear();
                }
            }
            return nullptr;
        }
        // Check if notifier has been signaled (for connect mode where
        // there's no future to check)
        if (!future && notifier) {
            // Non-blocking check: try to peek the notifier fd
            int fd = notifier->fd();
            if (fd >= 0) {
                struct pollfd pfd = {fd, POLLIN, 0};
                int rc = ::poll(&pfd, 1, 0);
                if (rc > 0 && (pfd.revents & POLLIN)) {
                    notifier->acknowledge(xsink);
                    done = true;
                    return nullptr;
                }
            }
        }

        // Return poll info pointing to the notifier fd (retrieved from
        // the "sock" member on self — avoids DGC cycle from storing a
        // QoreObject ref in C++ private data)
        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    //! Handles WAITING_CONNECT phase.  Peeks the notifier fd; on signal,
    //! submits the request and transitions to WAITING_RESPONSE.
    DLLLOCAL QoreHashNode* continueWaitingConnect(ExceptionSink* xsink) {
        // Non-blocking peek on the notifier fd.  If not yet signaled, stay
        // in WAITING_CONNECT.
        int fd = notifier->fd();
        if (fd >= 0) {
            struct pollfd pfd = {fd, POLLIN, 0};
            int rc = ::poll(&pfd, 1, 0);
            if (!(rc > 0 && (pfd.revents & POLLIN))) {
                return getSocketPollInfoHash(xsink, SOCK_POLLIN);
            }
        }
        notifier->acknowledge(xsink);

        // Connection reached a decided state (READY or CLOSED).
        if (pending_conn->isClosed()) {
            // Surface the connection's error, if any.
            ReferenceHolder<QoreHashNode> err_info(
                pending_conn->getReferencedErrorInfo(), xsink);
            const char* err_str = "HTTP-CLIENT-CONNECT-ERROR";
            const char* desc_str = "connection closed before READY during poll";
            if (err_info) {
                QoreValue ev = err_info->getKeyValue("err");
                QoreValue dv = err_info->getKeyValue("desc");
                if (ev.getType() == NT_STRING) {
                    err_str = ev.get<const QoreStringNode>()->c_str();
                }
                if (dv.getType() == NT_STRING) {
                    desc_str = dv.get<const QoreStringNode>()->c_str();
                }
            }
            xsink->raiseException(err_str, "%s", desc_str);
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        if (!pending_conn->isReady()) {
            // Spurious wake (state still CONNECTING) — extremely unlikely
            // because onConnectionReady signals only on transition.  Treat
            // as an internal error rather than spinning.
            xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
                "connection-ready notifier signaled but connection is not "
                "in a decided state");
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // READY: submit the request now.  Dispatch via the virtual
        // submitRequestWithAction — H1 and H2 both implement it, H3
        // raises HTTPCLIENT-NOT-IMPLEMENTED.
        // PromiseNotifierAction refs both promise and notifier; we still
        // hold our own refs, which are deref'd via clearPending / ~dtor.
        PromiseNotifierAction* action =
            new PromiseNotifierAction(pending_promise, notifier);
        const void* body_ptr = nullptr;
        size_t body_len = 0;
        if (pending_body) {
            body_ptr = pending_body->getPtr();
            body_len = pending_body->size();
        }
        int64_t stream_id = pending_conn->submitRequestWithAction(
            pending_method.c_str(), pending_path.c_str(), pending_headers,
            body_ptr, body_len, action, xsink);
        if (*xsink || stream_id < 0) {
            // submitRequestWithAction deref'd the action on failure but did
            // NOT release the stream reservation — clearPending does that.
            done = true;
            phase = Phase::DONE;
            clearPendingUnlocked(xsink);
            return nullptr;
        }

        // Successful submit: the connection has consumed its reservation
        // (submitRequestWithAction called releaseStreamReservation
        // internally).  Release our own connection ref explicitly, then
        // null pending_conn/mgr so clearPending does NOT double-release
        // the stream reservation.  Drop the other pending refs — the
        // future + notifier remain and will be used by WAITING_RESPONSE.
        pending_conn->deref(xsink);
        pending_conn = nullptr;
        pending_mgr = nullptr;
        clearPendingUnlocked(xsink);
        phase = Phase::WAITING_RESPONSE;
        return getSocketPollInfoHash(xsink, SOCK_POLLIN);
    }

    void abort(ExceptionSink* xsink) override {
        AutoLocker al(op_lock);
        done = true;
        phase = Phase::DONE;
        clearPendingUnlocked(xsink);
        cleanup(xsink);
    }

    QoreValue getOutput() const override {
        if (!future || !future->isDone()) {
            return QoreValue();
        }
        // Get the result from the future (non-blocking since isDone())
        ExceptionSink xsink;
        QoreValue rv = future->get(0, &xsink);
        if (xsink) {
            xsink.clear();
            return QoreValue();
        }
        // The conn_mgr produces one canonical response shape
        // (HttpClientResponseInfo — see the hashdecl in
        // qlib/HttpClientIo/HttpClientIo.qm).  This poll op is the
        // LEGACY ADAPTER for HTTPClient::startPollSendRecv(...).getOutput()
        // and translates to the historical dual-location shape via
        // toLegacyPollApiOutputShape — see the LEGACY RESPONSE SHAPE
        // ADAPTERS block at the top of this file for the rationale.
        if (rv.getType() == NT_HASH) {
            QoreHashNode* src = rv.get<QoreHashNode>();
            QoreHashNode* out = toLegacyPollApiOutputShape(src, &xsink);
            rv.discard(&xsink);
            if (xsink) {
                xsink.clear();
                if (out) {
                    out->deref(&xsink);
                }
                return QoreValue();
            }
            return out;
        }
        return rv;
    }

    const char* getStateImpl() const override {
        if (!deferred_err.empty()) {
            return "connecting";
        }
        if (done) {
            return connect_mode ? "connected" : "received";
        }
        if (connect_mode) {
            return "connecting";
        }
        switch (phase) {
            case Phase::WAITING_CONNECT:  return "connecting";
            case Phase::WAITING_RESPONSE: return "sending";
            case Phase::DONE:             return "received";
        }
        return "sending";
    }

    void cleanup(ExceptionSink* xsink) {
        if (future) {
            future->deref(xsink);
            future = nullptr;
        }
        if (notifier) {
            notifier->deref(xsink);
            notifier = nullptr;
        }
    }

private:
    QoreFuture* future = nullptr;
    QoreEventNotifier* notifier = nullptr;
    qore_httpclient_priv* priv_ref = nullptr;  // back-ref for user-disconnect detection
    mutable bool done = false;
    bool connect_mode = false;
    //! HTTPClient QoreObject (ref'd) — used to detect OBJECT-ALREADY-DELETED
    //! when the client is destroyed while the poll op is still active.
    QoreObject* client_obj = nullptr;
    std::string deferred_err;
    std::string deferred_desc;

    //! Phase tracks sendRecv mode progress (WAITING_CONNECT →
    //! WAITING_RESPONSE → DONE).  Unused for @ref connect_mode.
    Phase phase = Phase::WAITING_RESPONSE;

    //! Fields valid only while @ref phase == WAITING_CONNECT.  Cleared by
    //! @ref clearPending when the request is submitted (or the op aborts).
    //! pending_conn is a borrowed pool pointer (not ref'd); pending_mgr is
    //! the back-reference used to release the stream reservation if the
    //! request is never submitted.  headers/body/promise hold their own refs.
    HttpClientConnectionBase* pending_conn = nullptr;
    HttpClientConnectionManagerBase* pending_mgr = nullptr;
    std::string pending_method;
    std::string pending_path;
    QoreHashNode* pending_headers = nullptr;
    BinaryNode* pending_body = nullptr;
    QorePromise* pending_promise = nullptr;

    //! Serializes continuePoll() vs abort()/clearPendingUnlocked() so a
    //! concurrent cancel can't null pending_* fields mid-continuePoll.  The
    //! public abort() method is reachable on any thread via
    //! evalMethod("abort") dispatch (e.g., from the I/O controller's
    //! shutdown/cancel worker path); continuePoll() runs on the owning I/O
    //! thread.  Uncontended in the common case — the lock serialises only
    //! cross-thread cancels against in-flight polls on the same op.
    mutable QoreThreadLock op_lock;
};

QoreObject* qore_httpclient_priv::startPollSendRecvConnMgr(ExceptionSink* xsink, QoreObject* self,
        QoreHttpClientObject* client, const char* method, const char* path,
        const void* data, size_t size, const QoreHashNode* headers) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "startPollSendRecv", xsink);

    con_info this_connection = connection;

    // Build request headers — pass nullptr for encoding to avoid adding
    // charset to Content-Type (the poll op's submitRequest handles Content-Length)
    bool keep_alive = true;
    bool host_override = false;
    ReferenceHolder<QoreHashNode> nh(getRequestHeaders(xsink, headers,
        nullptr, (data && size), false, keep_alive, host_override), xsink);
    if (*xsink) {
        return nullptr;
    }

    // Determine scheme and path
    const char* scheme = this_connection.ssl ? "https" : "http";
    QoreString pathstr(enc ? enc : QCS_UTF8);
    bool path_already_encoded = false;
    const char* msgpath = getMsgPath(xsink, this_connection, path, pathstr, path_already_encoded);
    if (*xsink) {
        return nullptr;
    }

    // Acquire connection without waiting — a fresh connection is returned
    // in CONNECTING state so the poll op can report "connecting" until the
    // I/O thread finishes TCP/SSL setup.
    HttpClientConnectionManagerBase& mgr = getConnMgr(xsink);
    if (*xsink) {
        return nullptr;
    }
    HttpClientConnectionBase* conn = mgr.acquireConnectionAsync(scheme,
        this_connection.host.c_str(), this_connection.port, xsink);
    if (!conn || *xsink) {
        return nullptr;
    }
    // Hold a strong ref for the duration of this function.  The pool holds
    // its own ref, but the I/O thread may fire onConnectionClosed → deref
    // at any time — even between isClosed()/isReady() calls and
    // registerReadyNotifier — so without this extra ref the connection can
    // be freed under us.  Mirrors the comment block at
    // HttpClientConnectionManagerBase::request() for the sync path.
    //
    // The ref is EXPLICITLY RELEASED (not deref'd) into the poll op via
    // conn_local_ref_held==false once ownership transfers on each branch:
    // fast-path synchronous submit drops the ref at the end; slow-path
    // WAITING_CONNECT transfers the ref to the poll op's pending_conn slot.
    bool conn_local_ref_held = true;
    conn->ref();
    auto release_local_conn_ref = [&]() {
        if (conn_local_ref_held) {
            ExceptionSink rel_xsink;
            conn->deref(&rel_xsink);
            rel_xsink.clear();
            conn_local_ref_held = false;
        }
    };

    // Create EventNotifier + Promise + Future.  The notifier is used for
    // BOTH connection-ready signaling (WAITING_CONNECT phase) and response
    // completion (WAITING_RESPONSE phase, via PromiseNotifierAction).
    ReferenceHolder<QoreEventNotifier> notifier_holder(
        new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        mgr.releaseConnection(conn);
        release_local_conn_ref();
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;

    ReferenceHolder<QorePromise> promise_holder(new QorePromise(), xsink);
    QorePromise* promise_raw = *promise_holder;
    ReferenceHolder<QoreFuture> future_holder(promise_holder->getFuture(xsink), xsink);
    if (*xsink) {
        mgr.releaseConnection(conn);
        release_local_conn_ref();
        return nullptr;
    }

    // Wrap EventNotifier in a QoreObject for the "sock" member
    notifier_raw->ref();
    ReferenceHolder<QoreObject> notifier_obj(
        new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

    ReferenceHolder<HttpClientConnMgrPollOp> poller(xsink);
    QoreFuture* future_raw = *future_holder;
    QoreEventNotifier* notifier_for_op = *notifier_holder;

    if (conn->isClosed()) {
        // Rare: the connection transitioned to CLOSED between async acquire
        // and here.  Surface the protocol error if available.
        ReferenceHolder<QoreHashNode> err_info(
            conn->getReferencedErrorInfo(), xsink);
        const char* err_str = "HTTP-CLIENT-CONNECT-ERROR";
        const char* desc_str = "connection closed before poll send/recv request";
        if (err_info) {
            QoreValue ev = err_info->getKeyValue("err");
            QoreValue dv = err_info->getKeyValue("desc");
            if (ev.getType() == NT_STRING) {
                err_str = ev.get<const QoreStringNode>()->c_str();
            }
            if (dv.getType() == NT_STRING) {
                desc_str = dv.get<const QoreStringNode>()->c_str();
            }
        }
        xsink->raiseException(err_str, "%s", desc_str);
        mgr.closeAndEvict(conn, xsink);
        release_local_conn_ref();
        return nullptr;
    }

    if (conn->isReady()) {
        // Fast path: pool hit (or the controller finished the handshake
        // between async acquire and here).  Submit synchronously, just
        // like the pre-async implementation.  Dispatches via the virtual
        // HttpClientConnectionBase::submitRequestWithAction, so H1 and H2
        // are both supported.
        PromiseNotifierAction* action = new PromiseNotifierAction(
            promise_raw, notifier_raw);
        int64_t stream_id = conn->submitRequestWithAction(method, msgpath,
            *nh, data, size, action, xsink);
        if (*xsink || stream_id < 0) {
            mgr.releaseConnection(conn);
            release_local_conn_ref();
            return nullptr;
        }
        poller = new HttpClientConnMgrPollOp(future_raw, notifier_for_op,
            this, self);

        // Drop our local promise ref — the action owns one now.
        promise_holder.release()->deref(xsink);
    } else {
        // Slow path: the connection is still CONNECTING.  Register the
        // notifier for the READY/CLOSED transition and store the request
        // parameters in the poll op for deferred submission.
        notifier_raw->ref();
        notifier_obj->ref();
        bool queued = conn->registerReadyNotifier(notifier_raw, *notifier_obj);
        if (!queued) {
            // Race: the connection decided state between our check and the
            // register call.  registerReadyNotifier() consumes the refs we
            // passed in regardless of outcome (on failure the priv derefs
            // them internally before returning false — see
            // AbstractHttpPollConnectionPriv::registerReadyNotifier).
            // Do NOT deref again here: that was a double-deref, observed as
            // a PromiseNotifierAction ctor SIGABRT in concurrent-submit
            // under QORE_IO_THREADS>=2 when this slow path raced with
            // onConnectionReady.
            if (conn->isClosed()) {
                ReferenceHolder<QoreHashNode> err_info(
                    conn->getReferencedErrorInfo(), xsink);
                const char* err_str = "HTTP-CLIENT-CONNECT-ERROR";
                const char* desc_str = "connection closed before poll send/recv request";
                if (err_info) {
                    QoreValue ev = err_info->getKeyValue("err");
                    QoreValue dv = err_info->getKeyValue("desc");
                    if (ev.getType() == NT_STRING) {
                        err_str = ev.get<const QoreStringNode>()->c_str();
                    }
                    if (dv.getType() == NT_STRING) {
                        desc_str = dv.get<const QoreStringNode>()->c_str();
                    }
                }
                xsink->raiseException(err_str, "%s", desc_str);
                mgr.closeAndEvict(conn, xsink);
                release_local_conn_ref();
                return nullptr;
            }
            // READY now — fall into the fast-path synchronous submit.
            PromiseNotifierAction* action = new PromiseNotifierAction(
                promise_raw, notifier_raw);
            int64_t stream_id = conn->submitRequestWithAction(method,
                msgpath, *nh, data, size, action, xsink);
            if (*xsink || stream_id < 0) {
                mgr.releaseConnection(conn);
                release_local_conn_ref();
                return nullptr;
            }
            poller = new HttpClientConnMgrPollOp(future_raw, notifier_for_op,
                this, self);
            promise_holder.release()->deref(xsink);
        } else {
            // Queued: build a WAITING_CONNECT poll op that will submit
            // the request once the notifier fires.  All pending_* refs
            // are CONSUMED by the constructor.
            SimpleRefHolder<BinaryNode> body_holder;
            if (data && size) {
                body_holder = new BinaryNode();
                body_holder->append(data, size);
            }
            QoreHashNode* headers_raw = nh.release();  // ref consumed
            BinaryNode* body_raw = body_holder.release();  // nullable, ref consumed
            QorePromise* promise_raw_consumed = promise_holder.release();
            poller = new HttpClientConnMgrPollOp(conn, &mgr,
                std::string(method), std::string(msgpath),
                headers_raw, body_raw, promise_raw_consumed,
                future_raw, notifier_for_op, this, self);
        }
    }

    // Release our local ref on the connection now that ownership has
    // transferred to the poll op (WAITING_CONNECT ctor did pending_conn->ref())
    // or the request has been submitted (fast paths above).  Must run before
    // returning — doing it after rv.release() is also fine, but we drop it
    // here to minimise the window where we still hold it unnecessarily.
    release_local_conn_ref();

    // Wrap in SocketPollOperation QoreObject
    SocketPollOperationBase* p = *poller;
    ReferenceHolder<QoreObject> rv(
        new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
    if (!*xsink) {
        p->setSelf(*rv);
        rv->setValue("sock", notifier_obj.release(), xsink);
        rv->setValue("goal", new QoreStringNode("received"), xsink);
    }
    return rv.release();
}

QoreObject* qore_httpclient_priv::startPollConnectConnMgr(ExceptionSink* xsink, QoreObject* self,
        QoreHttpClientObject* client) {
    SocketSyncPoll::assertNotOnIoThread("HTTPClient", "startPollConnect", xsink);

    con_info this_connection = connection;
    const char* scheme = this_connection.ssl ? "https" : "http";

    // Acquire connection (may still be CONNECTING)
    HttpClientConnectionManagerBase& mgr = getConnMgr(xsink);
    if (*xsink) {
        return nullptr;
    }
    // acquireConnection may fail synchronously if TCP connect is refused
    // immediately; capture error and return a poll op that defers it to
    // the continuePoll loop (matching legacy startPollConnect behavior)
    ExceptionSink connect_xsink;
    HttpClientConnectionBase* conn = mgr.acquireConnection(scheme,
        this_connection.host.c_str(), this_connection.port, &connect_xsink);
    if (!conn || connect_xsink) {
        // Extract error info for deferred raising
        std::string err_str = "SOCKET-CONNECT-ERROR";
        std::string desc_str = "connection failed";
        if (connect_xsink.isException()) {
            QoreValue err_val = connect_xsink.getExceptionErr();
            QoreValue desc_val = connect_xsink.getExceptionDesc();
            if (err_val.getType() == NT_STRING) {
                err_str = err_val.get<const QoreStringNode>()->c_str();
            }
            if (desc_val.getType() == NT_STRING) {
                desc_str = desc_val.get<const QoreStringNode>()->c_str();
            }
        }
        connect_xsink.clear();

        ReferenceHolder<QoreEventNotifier> notifier_holder(
            new QoreEventNotifier(xsink), xsink);
        if (*xsink || !notifier_holder->isValid()) {
            return nullptr;
        }
        notifier_holder->notify();

        QoreEventNotifier* notifier_raw = *notifier_holder;
        notifier_raw->ref();
        ReferenceHolder<QoreObject> notifier_obj(
            new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

        // Create poll op with deferred error — constructor refs notifier,
        // holder will deref its own ref on scope exit
        QoreEventNotifier* notifier_for_op = *notifier_holder;
        ReferenceHolder<HttpClientConnMgrPollOp> poller(
            new HttpClientConnMgrPollOp(err_str.c_str(), desc_str.c_str(),
                notifier_for_op, this), xsink);

        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(
            new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", notifier_obj.release(), xsink);
            rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return rv.release();
    }

    // If already ready, return a poll op that completes on first continuePoll
    if (conn->isReady()) {
        mgr.releaseConnection(conn);
        ReferenceHolder<QoreEventNotifier> notifier_holder(
            new QoreEventNotifier(xsink), xsink);
        if (*xsink || !notifier_holder->isValid()) {
            return nullptr;
        }
        QoreEventNotifier* notifier_raw = *notifier_holder;
        notifier_raw->ref();
        ReferenceHolder<QoreObject> notifier_obj(
            new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

        QoreEventNotifier* notifier_for_op = *notifier_holder;
        ReferenceHolder<HttpClientConnMgrPollOp> poller(
            new HttpClientConnMgrPollOp(notifier_for_op, this), xsink);
        SocketPollOperationBase* p = *poller;
        ReferenceHolder<QoreObject> rv(
            new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
        if (!*xsink) {
            p->setSelf(*rv);
            rv->setValue("sock", notifier_obj.release(), xsink);
            rv->setValue("goal", new QoreStringNode("connect"), xsink);
        }
        return rv.release();
    }

    // Not ready yet — create EventNotifier and register for readiness
    ReferenceHolder<QoreEventNotifier> notifier_holder(
        new QoreEventNotifier(xsink), xsink);
    if (*xsink || !notifier_holder->isValid()) {
        mgr.releaseConnection(conn);
        return nullptr;
    }
    QoreEventNotifier* notifier_raw = *notifier_holder;

    // Wrap in QoreObject
    notifier_raw->ref();
    ReferenceHolder<QoreObject> notifier_obj(
        new QoreObject(QC_EVENTNOTIFIER, getProgram(), notifier_raw), xsink);

    // Register notifier on the connection's ready_notifiers
    notifier_raw->ref();
    notifier_obj->ref();
    bool queued = conn->registerReadyNotifier(notifier_raw, *notifier_obj);
    if (!queued) {
        // Already decided (ready or failed) between our check and registration.
        // registerReadyNotifier() consumes the refs we passed in regardless of
        // outcome — see AbstractHttpPollConnectionPriv::registerReadyNotifier.
        // Do NOT deref again here.
        mgr.releaseConnection(conn);
        if (conn->isReady()) {
            // Ready now — create a poll op that completes on first continuePoll
            ReferenceHolder<QoreEventNotifier> n2_holder(
                new QoreEventNotifier(xsink), xsink);
            if (*xsink || !n2_holder->isValid()) {
                return nullptr;
            }
            QoreEventNotifier* n2_raw = *n2_holder;
            n2_raw->ref();
            ReferenceHolder<QoreObject> n2_obj(
                new QoreObject(QC_EVENTNOTIFIER, getProgram(), n2_raw), xsink);

            QoreEventNotifier* n2_for_op = *n2_holder;
            ReferenceHolder<HttpClientConnMgrPollOp> poller(
                new HttpClientConnMgrPollOp(n2_for_op, this), xsink);
            SocketPollOperationBase* p = *poller;
            ReferenceHolder<QoreObject> rv(
                new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
            if (!*xsink) {
                p->setSelf(*rv);
                rv->setValue("sock", n2_obj.release(), xsink);
                rv->setValue("goal", new QoreStringNode("connect"), xsink);
            }
            return rv.release();
        }
        xsink->raiseException("HTTP-CLIENT-CONNECT-ERROR",
            "connection failed during poll connect");
        return nullptr;
    }

    mgr.releaseConnection(conn);

    // Create poll op — no future for connect, just notifier signaling
    QoreEventNotifier* notifier_for_op = *notifier_holder;
    ReferenceHolder<HttpClientConnMgrPollOp> poller(
        new HttpClientConnMgrPollOp(nullptr, notifier_for_op, this), xsink);
    (*poller)->setConnectMode();

    SocketPollOperationBase* p = *poller;
    ReferenceHolder<QoreObject> rv(
        new QoreObject(QC_SOCKETPOLLOPERATION, getProgram(), poller.release()), xsink);
    if (!*xsink) {
        p->setSelf(*rv);
        rv->setValue("sock", notifier_obj.release(), xsink);
        rv->setValue("goal", new QoreStringNode("connect"), xsink);
    }
    return rv.release();
}

QoreHashNode* QoreHttpClientObject::readHTTPChunkConnMgr(int timeout_ms, ExceptionSink* xsink) {
    if (!http_priv->streaming_recv_channel) {
        // No channel — not in conn_mgr streaming mode
        return nullptr;
    }

    QoreChannel* ch = http_priv->streaming_recv_channel;

    bool timed_out = false;
    bool has_value = false;
    ValueHolder rv(ch->recv(timeout_ms <= 0 ? 0 : timeout_ms, xsink, timed_out, has_value), xsink);
    if (*xsink) {
        http_priv->clearStreamingChannel();
        return nullptr;
    }
    if (timed_out) {
        xsink->raiseException("SOCKET-TIMEOUT",
            "timed out after %dms waiting for HTTP chunk data", timeout_ms);
        return nullptr;
    }
    if (!has_value) {
        // Channel closed = EOF
        http_priv->clearStreamingChannel();
        return new QoreHashNode(autoTypeInfo);
    }

    if (rv->getType() != NT_HASH) {
        // Skip non-hash values (recurse)
        return readHTTPChunkConnMgr(timeout_ms, xsink);
    }
    QoreHashNode* h = rv->get<QoreHashNode>();

    // Check for error
    QoreValue err_val = h->getKeyValue("err");
    if (!err_val.isNullOrNothing()) {
        http_priv->clearStreamingChannel();
        const char* err_str = err_val.getType() == NT_STRING
            ? err_val.get<const QoreStringNode>()->c_str()
            : "HTTP-CLIENT-RECEIVE-ERROR";
        QoreValue desc_val = h->getKeyValue("desc");
        const char* desc_str = desc_val.getType() == NT_STRING
            ? desc_val.get<const QoreStringNode>()->c_str()
            : "streaming request failed";
        xsink->raiseException(err_str, desc_str);
        return nullptr;
    }

    // Check for body data
    QoreValue body_val = h->getKeyValue("body");
    if (!body_val.isNullOrNothing()) {
        ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
        result->setKeyValue("body", body_val.refSelf(), xsink);
        return result.release();
    }

    // Check for end_stream
    QoreValue end_val = h->getKeyValue("end_stream");
    if (!end_val.isNullOrNothing()) {
        http_priv->clearStreamingChannel();
        return new QoreHashNode(autoTypeInfo);  // empty = EOF
    }

    // Unknown message type — skip (recurse)
    return readHTTPChunkConnMgr(timeout_ms, xsink);
}

QoreHashNode* QoreHttpClientObject::readServerSentEventConnMgr(const QoreStringNode* content_encoding,
        int timeout_ms, ExceptionSink* xsink) {
    if (!http_priv->streaming_recv_channel) {
        return nullptr;
    }

    // Check buffer for complete SSE event (double newline delimiter)
    while (true) {
        size_t sep = http_priv->sse_recv_buffer.find("\n\n");
        if (sep != std::string::npos) {
            std::string event_text = http_priv->sse_recv_buffer.substr(0, sep + 2);
            http_priv->sse_recv_buffer.erase(0, sep + 2);
            // Parse SSE event using the static Socket method
            SimpleRefHolder<QoreStringNode> event_str(
                new QoreStringNode(event_text.c_str(), event_text.size(), QCS_UTF8));
            return parseSseEvent(xsink, **event_str);
        }

        // Read more data from channel
        ReferenceHolder<QoreHashNode> chunk(readHTTPChunkConnMgr(timeout_ms, xsink), xsink);
        if (*xsink) {
            return nullptr;
        }
        if (!chunk) {
            return nullptr;
        }

        QoreValue body_val = chunk->getKeyValue("body");
        if (body_val.isNullOrNothing()) {
            // EOF — parse any remaining buffer
            if (!http_priv->sse_recv_buffer.empty()) {
                std::string remaining = http_priv->sse_recv_buffer;
                http_priv->sse_recv_buffer.clear();
                SimpleRefHolder<QoreStringNode> event_str(
                    new QoreStringNode(remaining.c_str(), remaining.size(), QCS_UTF8));
                return parseSseEvent(xsink, **event_str);
            }
            return nullptr;
        }

        // Append body to buffer
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* bin = body_val.get<const BinaryNode>();
            http_priv->sse_recv_buffer.append(
                reinterpret_cast<const char*>(bin->getPtr()), bin->size());
        } else if (body_val.getType() == NT_STRING) {
            const QoreStringNode* str = body_val.get<const QoreStringNode>();
            http_priv->sse_recv_buffer.append(str->c_str(), str->size());
        }
    }
}

QoreHashNode* QoreHttpClientObject::readHTTPChunkedBodyConnMgr(int timeout_ms, ExceptionSink* xsink) {
    if (!http_priv->streaming_recv_channel) {
        return nullptr;
    }

    // Drain all chunks into a string
    QoreStringNode* body = new QoreStringNode();
    while (true) {
        ReferenceHolder<QoreHashNode> chunk(readHTTPChunkConnMgr(timeout_ms, xsink), xsink);
        if (*xsink) {
            body->deref(xsink);
            return nullptr;
        }
        if (!chunk) {
            break;
        }
        QoreValue body_val = chunk->getKeyValue("body");
        if (body_val.isNullOrNothing()) {
            break;  // EOF
        }
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* bin = body_val.get<const BinaryNode>();
            body->concat(reinterpret_cast<const char*>(bin->getPtr()), bin->size());
        } else if (body_val.getType() == NT_STRING) {
            const QoreStringNode* str = body_val.get<const QoreStringNode>();
            body->concat(str->c_str(), str->size());
        }
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("body", body, xsink);
    return result.release();
}

QoreHashNode* QoreHttpClientObject::readHTTPChunkedBodyBinaryConnMgr(int timeout_ms, ExceptionSink* xsink) {
    if (!http_priv->streaming_recv_channel) {
        return nullptr;
    }

    // Drain all chunks into a binary node
    BinaryNode* body = new BinaryNode();
    while (true) {
        ReferenceHolder<QoreHashNode> chunk(readHTTPChunkConnMgr(timeout_ms, xsink), xsink);
        if (*xsink) {
            body->deref(xsink);
            return nullptr;
        }
        if (!chunk) {
            break;
        }
        QoreValue body_val = chunk->getKeyValue("body");
        if (body_val.isNullOrNothing()) {
            break;  // EOF
        }
        if (body_val.getType() == NT_BINARY) {
            const BinaryNode* bin = body_val.get<const BinaryNode>();
            body->append(bin->getPtr(), bin->size());
        } else if (body_val.getType() == NT_STRING) {
            const QoreStringNode* str = body_val.get<const QoreStringNode>();
            body->append(str->c_str(), str->size());
        }
    }

    ReferenceHolder<QoreHashNode> result(new QoreHashNode(autoTypeInfo), xsink);
    result->setKeyValue("body", body, xsink);
    return result.release();
}

QoreHashNode* QoreHttpClientObject::sendAndStream(const char* meth, const char* new_path, const QoreHashNode* headers,
        const void* data, unsigned size, QoreHashNode* info, ExceptionSink* xsink) {
    // streaming=true means don't read the body, leave it on the socket for streaming
    return http_priv->send_internal(xsink, "sendAndStream", meth, new_path, headers, nullptr, data, size, nullptr,
        false, info, http_priv->timeout, nullptr, nullptr, nullptr, nullptr, 0, nullptr, true);
}

bool QoreHttpClientObject::hasStreamingChannel() const {
    return http_priv->streaming_recv_channel != nullptr
        || !http_priv->sse_recv_buffer.empty();
}

bool QoreHttpClientObject::isDataAvailable(int timeout_ms, ExceptionSink* xsink) const {
    if (http_priv->streaming_recv_channel) {
        // Consider buffered SSE text as immediately-pending — matches
        // readServerSentEventConnMgr which drains this buffer first.
        if (!http_priv->sse_recv_buffer.empty()) {
            return true;
        }
        // Non-destructive wait on the channel: blocks up to timeout_ms
        // for the channel to become non-empty or closed.  Returning true
        // on "closed" is correct — the caller's next readHTTPChunk /
        // readServerSentEvent will see EOF and unwind its loop.  Without
        // the wait, a busy caller (e.g. ServerSentEventClient::eventLoop)
        // would burn CPU polling size() instead of blocking like the
        // legacy msock path did.
        return http_priv->streaming_recv_channel->waitReadable(timeout_ms, xsink);
    }
    // No streaming channel — legacy msock path
    return http_priv->msock->socket->isDataAvailable(xsink, timeout_ms);
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
    // send_internal returns a QoreHashNode* that the recv-callback path
    // discards — the callback already consumed the response.  Wrap in
    // ReferenceHolder so the hash (and its transitively-owned header /
    // body values) is deref'd on return instead of leaking.
    ReferenceHolder<QoreHashNode> rv(
        http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers,
            nullptr, data, size, nullptr,
            getbody, info, timeout_ms, recv_callback, obj),
        xsink);
}

void QoreHttpClientObject::sendWithRecvCallback(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    // See comment in the other overload — wrap the unused return to avoid
    // leaking the response hash.
    ReferenceHolder<QoreHashNode> rv(
        http_priv->send_internal(xsink, "sendWithRecvCallback", meth, mpath, headers,
            *tstr, tstr->c_str(), tstr->size(),
            nullptr, getbody, info, timeout_ms, recv_callback, obj),
        xsink);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const void* data, unsigned size, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    // send_internal may return a response hash via conn_mgr; deref it since
    // this method returns void and the caller doesn't need the hash.
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers,
        nullptr, data, size, nullptr, getbody, info, timeout_ms, recv_callback, obj, os), xsink);
}

void QoreHttpClientObject::sendWithOutputStream(const char* meth, const char* mpath, const QoreHashNode* headers,
        const QoreStringNode& body, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, OutputStream *os, ExceptionSink* xsink) {
    const QoreEncoding* enc = http_priv->getEncoding();
    QoreStringNodeValueHelper tstr(&body, enc, xsink);
    if (*xsink) {
        return;
    }
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers,
        *tstr, tstr->c_str(), tstr->size(), nullptr, getbody, info, timeout_ms, recv_callback, obj, os), xsink);
}

void QoreHttpClientObject::sendChunked(const char* meth, const char* mpath, const QoreHashNode* headers, bool getbody,
        QoreHashNode* info, int timeout_ms, const ResolvedCallReferenceNode* recv_callback, QoreObject* obj,
        OutputStream *os, InputStream* is, size_t max_chunk_size, const ResolvedCallReferenceNode* trailer_callback, ExceptionSink* xsink) {
    assert(max_chunk_size);
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithOutputStream", meth, mpath, headers,
        nullptr, nullptr, 0, nullptr, getbody, info, timeout_ms, recv_callback, obj, os, is, max_chunk_size,
        trailer_callback), xsink);
}

void QoreHttpClientObject::sendWithCallbacks(const char* meth, const char* mpath, const QoreHashNode* headers,
        const ResolvedCallReferenceNode* send_callback, bool getbody, QoreHashNode* info, int timeout_ms,
        const ResolvedCallReferenceNode* recv_callback, QoreObject* obj, ExceptionSink* xsink) {
    ReferenceHolder<QoreHashNode> rv(http_priv->send_internal(xsink, "sendWithCallbacks", meth, mpath, headers,
        nullptr, nullptr, 0, send_callback, getbody, info, timeout_ms, recv_callback, obj), xsink);
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
    if (http_priv->streaming_recv_channel) {
        return !http_priv->streaming_recv_channel->isClosed();
    }
    return http_priv->msock->socket->isOpen();
}

bool QoreHttpClientObject::isOpen() const {
    return isConnected();
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
