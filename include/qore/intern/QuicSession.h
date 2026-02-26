/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicSession.h

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

#ifndef _QORE_INTERN_QUICSESSION_H
#define _QORE_INTERN_QUICSESSION_H

#include "qore/common.h"
#include "qore/intern/QuicCommon.h"

#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#include <ngtcp2/ngtcp2_crypto_ossl.h>
#include <nghttp3/nghttp3.h>

#include <openssl/ssl.h>

#include <sys/socket.h>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

class QoreDatagramDispatcher;
class QoreSSLCertificate;
class QoreSSLPrivateKey;

//! QUIC stream state (parallel to Http2StreamState)
enum class QuicStreamState {
    Idle,
    Open,
    HalfClosedLocal,
    HalfClosedRemote,
    Closed,
};

//! Per-stream state (parallel to Http2StreamInfo)
struct QuicStreamInfo {
    int64_t stream_id = -1;
    QuicStreamState state = QuicStreamState::Idle;
    std::string method;
    std::string path;
    std::string authority;
    std::string scheme;
    int status_code = 0;
    std::map<std::string, std::vector<std::string>, ltstrcase> headers;
    std::vector<char> body;
    bool headers_complete = false;
    bool body_complete = false;
    std::string error_message;  //!< non-empty if stream terminated with error
};

//! Per-stream body data for sending via nghttp3 data reader callback
struct QuicBodyData {
    std::vector<uint8_t> data;   //!< owned copy of body data
    size_t offset = 0;
};

class qore_socket_private;

//! QUIC session wrapper around ngtcp2 + nghttp3 (parallel to Http2Session)
class QuicSession {
public:
    //! Named constants for QUIC transport parameters
    static constexpr int QUIC_INITIAL_MAX_STREAMS_UNI = 3;
    //! NOTE: with QUIC_MAX_STREAM_BODY = 1MB, the theoretical per-peer maximum is
    //! ~100MB.  This is acceptable because: (a) flow control limits actual inflight
    //! data via QUIC_INITIAL_MAX_DATA (1MB aggregate), and (b) real peers don't open
    //! 100 concurrent streams with max-size bodies.  A per-session aggregate limit
    //! can be added if abuse scenarios arise.
    static constexpr int QUIC_INITIAL_MAX_STREAMS_BIDI = 100;
    static constexpr size_t QUIC_INITIAL_MAX_STREAM_DATA = 256 * 1024;
    static constexpr size_t QUIC_INITIAL_MAX_DATA = 1024 * 1024;

    //! Idle timeout in nanoseconds (ngtcp2 uses ngtcp2_tstamp units)
    /** 30 seconds prevents resource exhaustion from lost or malicious connections.
        Both client and server use this value; the effective timeout is the minimum
        of the two endpoints' advertised values (RFC 9000 Section 10.1).
    */
    static constexpr uint64_t QUIC_IDLE_TIMEOUT_NS = 30ULL * NGTCP2_SECONDS;

    //! Maximum buffered data before HTTP/3 layer is initialized
    static constexpr size_t QUIC_MAX_PRE_H3_BUFFER = 65536;
    //! Maximum buffered entries before HTTP/3 layer is initialized
    /** Limits per-entry overhead amplification: each entry has ~48 bytes of
        struct/vector overhead beyond the payload bytes counted by
        QUIC_MAX_PRE_H3_BUFFER.
    */
    static constexpr size_t QUIC_MAX_PRE_H3_ENTRIES = 1024;
    //! Maximum stream body size (1 MB)
    static constexpr size_t QUIC_MAX_STREAM_BODY = 1048576;

    //! Client source connection ID length (8 bytes is common; server uses NGTCP2_MAX_CIDLEN)
    static constexpr size_t QUIC_CLIENT_SCID_LEN = 8;

    //! Factory: create a client QUIC session
    /** @param sock the underlying socket
        @param xsink exception sink
        @param host remote hostname for TLS SNI
        @param port remote port
        @param local_addr local socket address (from getsockname)
        @param local_addrlen length of local_addr
        @param remote_addr remote socket address (resolved)
        @param remote_addrlen length of remote_addr
    */
    DLLLOCAL static std::shared_ptr<QuicSession> createClient(
        qore_socket_private* sock, ExceptionSink* xsink,
        const char* host, uint16_t port,
        const struct sockaddr* local_addr, socklen_t local_addrlen,
        const struct sockaddr* remote_addr, socklen_t remote_addrlen,
        int ssl_verify_mode = SSL_VERIFY_NONE);

    //! Factory: create a server QUIC session
    /** @param sock the underlying socket
        @param xsink exception sink
        @param initial_hdr the parsed initial QUIC packet header
        @param cert the X.509 certificate for TLS 1.3 (required)
        @param pk the private key for TLS 1.3 (required)
        @param local_addr local socket address (from getsockname)
        @param local_addrlen length of local_addr
        @param remote_addr remote socket address (from recvfrom)
        @param remote_addrlen length of remote_addr
        @param dispatcher optional datagram dispatcher for CID-based routing (multi-connection)
    */
    DLLLOCAL static std::shared_ptr<QuicSession> createServer(
        qore_socket_private* sock, ExceptionSink* xsink,
        const ngtcp2_pkt_hd* initial_hdr,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
        const struct sockaddr* local_addr, socklen_t local_addrlen,
        const struct sockaddr* remote_addr, socklen_t remote_addrlen,
        QoreDatagramDispatcher* dispatcher = nullptr);

    ~QuicSession();

    // non-copyable, non-movable (shared_ptr manages lifetime)
    QuicSession(const QuicSession&) = delete;
    QuicSession& operator=(const QuicSession&) = delete;
    QuicSession(QuicSession&&) = delete;
    QuicSession& operator=(QuicSession&&) = delete;

    //! Process an incoming QUIC packet from the network
    /** @param data packet data
        @param len packet length
        @param path path (local + remote address)
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int readPacket(const uint8_t* data, size_t len,
                   const ngtcp2_path& path, ExceptionSink* xsink);

    //! Generate outgoing QUIC packets as individual datagrams
    /** Each entry in @a packets is a separate QUIC packet that must be sent
        as its own UDP datagram.  Short header (1-RTT) packets cannot be
        coalesced, so sending multiple concatenated packets in one datagram
        would cause the receiver to silently discard all but the first.
        @param packets output: one vector<uint8_t> per datagram
        @param xsink exception sink
        @return number of packets generated, 0 if nothing to send, -1 on error
    */
    DLLLOCAL int writePackets(QuicPacketBatch& batch, ExceptionSink* xsink);

    //! Get next timer expiry in nanoseconds (UINT64_MAX if none)
    DLLLOCAL ngtcp2_tstamp getExpiry() const;

    //! Handle timer expiry (retransmission, keep-alive, etc.)
    DLLLOCAL int handleExpiry(ExceptionSink* xsink);

    //! Submit an HTTP/3 request (client side)
    /** @return stream ID on success, -1 on error */
    DLLLOCAL int64_t submitRequest(const char* method, const char* path,
                          const strcase_str_map_t& headers,
                          const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit an HTTP/3 response (server side)
    DLLLOCAL int submitResponse(int64_t stream_id, int status_code,
                       const strcase_str_map_t& headers,
                       const void* body, size_t body_len, ExceptionSink* xsink);

    //! Take the next completed stream (transfers ownership)
    DLLLOCAL std::unique_ptr<QuicStreamInfo> takeCompletedStream();

    //! Check if there are completed streams ready
    /** @note This is a non-authoritative hint for polling loops.
        takeCompletedStream() may return nullptr even after this returns true
        due to TOCTOU (time-of-check-to-time-of-use) races.
    */
    DLLLOCAL bool hasCompletedStreams() const;

    //! Check if the QUIC handshake is complete
    DLLLOCAL bool isHandshakeComplete() const;

    //! Check if the connection is closed or closing
    DLLLOCAL bool isClosed() const;

    //! Get the connection close error (if any)
    DLLLOCAL ngtcp2_ccerr getCloseError() const;

    //! Get the ngtcp2 connection handle
    /** @note Caller must hold the socket lock (priv->m) or be on the exclusive
        I/O path (e.g., during handshake or inside continuePoll).
    */
    DLLLOCAL ngtcp2_conn* getConn() { return conn_; }

    //! Set up the HTTP/3 layer after handshake completes
    DLLLOCAL int setupHttp3(ExceptionSink* xsink);

    //! Check if HTTP/3 layer is initialized
    DLLLOCAL bool isHttp3Ready() const {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        return h3_conn_ != nullptr;
    }

    //! Check if there is pending data to write (set after submitRequest/submitResponse)
    DLLLOCAL bool hasPendingWrite() const { return pending_write_.load(std::memory_order_acquire); }

    //! Clear the pending write flag (called after writePackets flushes all data)
    DLLLOCAL void clearPendingWrite() { pending_write_.store(false, std::memory_order_release); }

    //! Get current timestamp in nanoseconds (monotonic clock)
    DLLLOCAL static ngtcp2_tstamp timestamp();

    //! Get the stored remote peer address
    DLLLOCAL const struct sockaddr_storage& getRemoteAddr() const { return remote_addr_; }

    //! Get the stored remote peer address length
    DLLLOCAL socklen_t getRemoteAddrLen() const { return remote_addrlen_; }

    //! Get unique session ID (assigned at creation)
    DLLLOCAL int64_t getSessionId() const { return session_id_; }

    //! Get the server CID (only valid for server sessions)
    DLLLOCAL const ngtcp2_cid& getScid() const { return scid_; }

    //! Cancel a QUIC stream (send RESET_STREAM + STOP_SENDING)
    /** @param stream_id the stream to cancel
        @param app_error_code application error code (e.g. NGHTTP3_H3_REQUEST_CANCELLED)
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int cancelStream(int64_t stream_id, uint64_t app_error_code, ExceptionSink* xsink);

    //! Write a CONNECTION_CLOSE packet for graceful shutdown
    /** @param buf output buffer (must be at least 1280 bytes for minimum QUIC packet)
        @param buflen size of the output buffer
        @return number of bytes written, or 0 if the connection cannot generate the frame
    */
    DLLLOCAL ssize_t writeConnectionClose(uint8_t* buf, size_t buflen);

    //! Clear the stored qore_socket_private pointer
    /** Called after session creation to avoid dangling pointer,
        since sock_ is only needed during initialization for TLS context.
    */
    DLLLOCAL void clearSockPtr() { sock_ = nullptr; }

    //! Clear dispatcher pointer (called before dispatcher destruction to prevent use-after-free)
    DLLLOCAL void clearDispatcher() {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        dispatcher_ = nullptr;
    }

    //! Submit an HTTP/3 GOAWAY shutdown notice (first phase: max stream ID)
    /** Sends a GOAWAY frame with the maximum stream ID, indicating that the
        server intends to shut down but hasn't decided on the final stream ID yet.
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitShutdownNotice(ExceptionSink* xsink);

    //! Submit an HTTP/3 GOAWAY shutdown (second phase: actual last stream ID)
    /** Sends a GOAWAY frame with the actual last stream ID that the server will
        process.  After this, no new streams will be accepted.
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitShutdown(ExceptionSink* xsink);

    //! Check if the final GOAWAY has been queued via submitShutdown()
    /** Returns false during the notice phase (submitShutdownNotice only).
        Set eagerly when queued; frame may not yet be on the wire.
    */
    DLLLOCAL bool isGoawaySent() const { return goaway_sent_.load(std::memory_order_acquire); }

    //! Check if a GOAWAY has been received from the peer
    DLLLOCAL bool isGoawayReceived() const { return goaway_received_.load(std::memory_order_acquire); }

    //! Get the max stream ID from received GOAWAY (-1 if none received)
    DLLLOCAL int64_t getGoawayMaxStreamId() const {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        return goaway_max_stream_id_;
    }

private:
    DLLLOCAL QuicSession();

    //! Initialize client-side connection
    DLLLOCAL int initClient(qore_socket_private* sock, ExceptionSink* xsink,
                   const char* host, uint16_t port,
                   const struct sockaddr* local_addr, socklen_t local_addrlen,
                   const struct sockaddr* remote_addr, socklen_t remote_addrlen,
                   int ssl_verify_mode = SSL_VERIFY_NONE);

    //! Initialize server-side connection
    DLLLOCAL int initServer(qore_socket_private* sock, ExceptionSink* xsink,
                   const ngtcp2_pkt_hd* initial_hdr,
                   QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                   const struct sockaddr* local_addr, socklen_t local_addrlen,
                   const struct sockaddr* remote_addr, socklen_t remote_addrlen,
                   QoreDatagramDispatcher* dispatcher);

    //! Set up SSL_CTX for client
    DLLLOCAL int setupClientSslCtx(const char* host, int ssl_verify_mode, ExceptionSink* xsink);

    //! Set up SSL_CTX for server with certificate and private key
    DLLLOCAL int setupServerSslCtx(QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                          ExceptionSink* xsink);

    //! Get or create a stream info entry
    DLLLOCAL QuicStreamInfo* getOrCreateStream(int64_t stream_id);

    //! Mark a stream as complete and move to the completed queue
    DLLLOCAL void markStreamComplete(int64_t stream_id);

    // --- ngtcp2 static callbacks ---

    //! Callback to get ngtcp2_conn from conn_ref (for TLS integration)
    DLLLOCAL static ngtcp2_conn* getConnFromRef(ngtcp2_crypto_conn_ref* conn_ref);

    //! Random number generation callback
    DLLLOCAL static void randCallback(uint8_t* dest, size_t destlen,
                             const ngtcp2_rand_ctx* rand_ctx);

    //! Generate new connection ID
    DLLLOCAL static int getNewConnectionIdCallback(ngtcp2_conn* conn, ngtcp2_cid* cid,
                                          uint8_t* token, size_t cidlen,
                                          void* user_data);

    //! Stream data received from peer
    DLLLOCAL static int recvStreamDataCallback(ngtcp2_conn* conn, uint32_t flags,
                                      int64_t stream_id, uint64_t offset,
                                      const uint8_t* data, size_t datalen,
                                      void* user_data, void* stream_user_data);

    //! Stream acknowledged data
    DLLLOCAL static int ackedStreamDataOffsetCallback(ngtcp2_conn* conn, int64_t stream_id,
                                             uint64_t offset, uint64_t datalen,
                                             void* user_data, void* stream_user_data);

    //! Stream closed
    DLLLOCAL static int streamCloseCallback(ngtcp2_conn* conn, uint32_t flags,
                                   int64_t stream_id, uint64_t app_error_code,
                                   void* user_data, void* stream_user_data);

    //! Handshake completed
    DLLLOCAL static int handshakeCompletedCallback(ngtcp2_conn* conn, void* user_data);

    //! Extend max local bidi streams (client)
    DLLLOCAL static int extendMaxLocalStreamsBidiCallback(ngtcp2_conn* conn,
                                                  uint64_t max_streams,
                                                  void* user_data);

    //! Extend max remote bidi streams (server)
    DLLLOCAL static int extendMaxRemoteStreamsBidiCallback(ngtcp2_conn* conn,
                                                   uint64_t max_streams,
                                                   void* user_data);

    //! Extend max stream data
    DLLLOCAL static int extendMaxStreamDataCallback(ngtcp2_conn* conn,
                                            int64_t stream_id,
                                            uint64_t max_data,
                                            void* user_data, void* stream_user_data);

    //! Receive TX key (for HTTP/3 uni stream binding)
    DLLLOCAL static int recvTxKeyCallback(ngtcp2_conn* conn,
                                 ngtcp2_encryption_level level,
                                 void* user_data);

    // --- nghttp3 static callbacks ---

    //! HTTP/3 begin headers
    DLLLOCAL static int h3BeginHeadersCallback(nghttp3_conn* conn, int64_t stream_id,
                                      void* conn_user_data, void* stream_user_data);

    //! HTTP/3 receive header
    DLLLOCAL static int h3RecvHeaderCallback(nghttp3_conn* conn, int64_t stream_id,
                                    int32_t token, nghttp3_rcbuf* name,
                                    nghttp3_rcbuf* value, uint8_t flags,
                                    void* conn_user_data, void* stream_user_data);

    //! HTTP/3 end headers
    DLLLOCAL static int h3EndHeadersCallback(nghttp3_conn* conn, int64_t stream_id,
                                    int fin, void* conn_user_data,
                                    void* stream_user_data);

    //! HTTP/3 receive data
    DLLLOCAL static int h3RecvDataCallback(nghttp3_conn* conn, int64_t stream_id,
                                  const uint8_t* data, size_t datalen,
                                  void* conn_user_data, void* stream_user_data);

    //! HTTP/3 end stream
    DLLLOCAL static int h3EndStreamCallback(nghttp3_conn* conn, int64_t stream_id,
                                   void* conn_user_data, void* stream_user_data);

    //! HTTP/3 deferred consume
    DLLLOCAL static int h3DeferredConsumeCallback(nghttp3_conn* conn, int64_t stream_id,
                                         size_t consumed, void* conn_user_data,
                                         void* stream_user_data);

    //! HTTP/3 stop sending
    DLLLOCAL static int h3StopSendingCallback(nghttp3_conn* conn, int64_t stream_id,
                                     uint64_t app_error_code,
                                     void* conn_user_data, void* stream_user_data);

    //! HTTP/3 reset stream
    DLLLOCAL static int h3ResetStreamCallback(nghttp3_conn* conn, int64_t stream_id,
                                     uint64_t app_error_code,
                                     void* conn_user_data, void* stream_user_data);

    //! HTTP/3 data reader callback for body data
    DLLLOCAL static nghttp3_ssize h3ReadDataCallback(nghttp3_conn* conn, int64_t stream_id,
                                            nghttp3_vec* vec, size_t veccnt,
                                            uint32_t* pflags,
                                            void* conn_user_data,
                                            void* stream_user_data);

    //! HTTP/3 acked stream data callback
    DLLLOCAL static int h3AckedStreamDataCallback(nghttp3_conn* conn, int64_t stream_id,
                                         uint64_t datalen, void* conn_user_data,
                                         void* stream_user_data);

    //! HTTP/3 shutdown (GOAWAY) callback — invoked when remote sends GOAWAY
    DLLLOCAL static int h3ShutdownCallback(nghttp3_conn* conn, int64_t id,
                                           void* conn_user_data);

    // --- Member data ---

    //! Unique session ID (monotonically increasing)
    static std::atomic<int64_t> next_session_id_;
    int64_t session_id_ = 0;                         //!< unique session ID

    ngtcp2_conn* conn_ = nullptr;                   //!< ngtcp2 connection handle
    nghttp3_conn* h3_conn_ = nullptr;               //!< nghttp3 HTTP/3 connection handle
    ngtcp2_crypto_ossl_ctx* ossl_ctx_ = nullptr;    //!< ngtcp2 OpenSSL context wrapper
    SSL_CTX* ssl_ctx_ = nullptr;                    //!< OpenSSL context
    SSL* ssl_ = nullptr;                            //!< OpenSSL connection
    ngtcp2_crypto_conn_ref conn_ref_{};              //!< TLS<->ngtcp2 connection reference
    qore_socket_private* sock_ = nullptr;           //!< associated socket
    bool is_server_ = false;                        //!< true if server-side session

    //! Server CID for this session (used for CID-based routing)
    ngtcp2_cid scid_{};

    //! Dispatcher for CID-based packet routing (server only, may be nullptr)
    QoreDatagramDispatcher* dispatcher_ = nullptr;
    std::atomic<bool> handshake_completed_{false};   //!< true when handshake completes
    std::atomic<bool> pending_write_{false};         //!< true when data queued for writing
    std::atomic<bool> has_completed_streams_{false};  //!< true when completed streams are queued (lock-free check)
    std::atomic<bool> goaway_sent_{false};           //!< true when final GOAWAY (submitShutdown) has been queued; false during notice-only phase
    std::atomic<bool> goaway_received_{false};       //!< true when GOAWAY received from peer
    int64_t goaway_max_stream_id_{-1};               //!< max stream ID from received GOAWAY; protected by mtx_
    std::string host_;                               //!< server hostname for :authority fallback
    uint16_t port_ = 0;                             //!< server port

    //! Active streams (unordered_map for O(1) lookup vs O(log n) with std::map)
    std::unordered_map<int64_t, std::unique_ptr<QuicStreamInfo>> streams_;

    //! Queue of completed stream IDs
    std::queue<int64_t> completed_streams_;

    //! Body data for streams being sent (used by data reader callback)
    std::unordered_map<int64_t, QuicBodyData> body_data_;

    //! Stream data received before HTTP/3 layer is initialized (buffered for replay)
    struct BufferedStreamData {
        int64_t stream_id;
        std::vector<uint8_t> data;
        bool fin;
    };
    std::vector<BufferedStreamData> pre_h3_buffer_;
    size_t pre_h3_buffer_size_{0};  //!< Running total of buffered bytes (avoids O(n) sum)

    //! Stored local address for path construction
    struct sockaddr_storage local_addr_{};
    socklen_t local_addrlen_ = 0;

    //! Stored remote address for path construction
    struct sockaddr_storage remote_addr_{};
    socklen_t remote_addrlen_ = 0;

    //! Packet buffer for writePackets()
    uint8_t pkt_buf_[QUIC_COMMON_MAX_PKTLEN]{};

    //! Recursive mutex required because ngtcp2/nghttp3 callbacks (e.g.,
    //! h3RecvDataCallback, streamCloseCallback) are invoked synchronously from
    //! readPacket() and writePackets() while mtx_ is already held. These
    //! callbacks call back into QuicSession methods (getOrCreateStream,
    //! markStreamComplete) that also need the lock. A non-recursive mutex
    //! would deadlock.
    mutable std::recursive_mutex mtx_;
};

#endif // _QORE_INTERN_QUICSESSION_H
