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
#include <condition_variable>
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
    //! true if stream received data in 0-RTT (for server-side logging/auditing)
    bool received_0rtt_data = false;
};

//! Per-stream body data for sending via nghttp3 data reader callback
struct QuicBodyData {
    std::vector<uint8_t> data;   //!< owned copy of body data
    size_t offset = 0;
};

//! Per-stream streaming body data for incremental sending via nghttp3 data reader callback
/** Used by submitResponseStreaming()/sendStreamData() for InputStream-based responses.
    The nghttp3 data reader callback returns NGHTTP3_ERR_WOULDBLOCK when no data is
    available, and nghttp3_conn_resume_stream() is called when new data arrives.
*/
struct QuicStreamingBodyData {
    std::vector<uint8_t> data;   //!< buffered chunk data
    size_t offset = 0;           //!< read offset into data
    bool eof = false;            //!< InputStream signaled EOF
    bool deferred = false;       //!< WOULDBLOCK returned, waiting for resume
};

class qore_socket_private;

//! Result from processTimerAndWrite() — coalesces timer check + packet generation
struct QuicTimerWriteResult {
    ngtcp2_tstamp next_expiry = UINT64_MAX;  //!< next timer expiry after processing
    bool error = false;                       //!< true if an error occurred
};

//! A received QUIC packet ready for batch processing
struct QuicReceivedPacket {
    const uint8_t* data;    //!< packet data (not owned)
    size_t len;             //!< packet length
    ngtcp2_path path;       //!< network path (local + remote address)
};

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

    //! Maximum active connection IDs (CIDs) advertised to the peer
    /** Each migration consumes one CID; 8 allows 7 migrations before CID
        exhaustion (default ngtcp2 value is 2, which is too low for mobile
        clients that may experience multiple network transitions).
    */
    static constexpr uint64_t QUIC_ACTIVE_CONNECTION_ID_LIMIT = 8;

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
        int ssl_verify_mode = SSL_VERIFY_NONE,
        bool enable_0rtt = false);

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
        @param shared_ssl_ctx optional shared SSL_CTX for session ticket key continuity (0-RTT)
    */
    DLLLOCAL static std::shared_ptr<QuicSession> createServer(
        qore_socket_private* sock, ExceptionSink* xsink,
        const ngtcp2_pkt_hd* initial_hdr,
        QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
        const struct sockaddr* local_addr, socklen_t local_addrlen,
        const struct sockaddr* remote_addr, socklen_t remote_addrlen,
        QoreDatagramDispatcher* dispatcher = nullptr,
        SSL_CTX* shared_ssl_ctx = nullptr);

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

    //! Coalesced timer check + packet generation under a single lock acquisition
    /** Replaces the separate getExpiry() → handleExpiry() → writePackets() → getExpiry()
        sequence with a single lock acquisition.
        @param packets output: generated QUIC packets
        @param xsink exception sink
        @return result containing next_expiry and error flag
    */
    DLLLOCAL QuicTimerWriteResult processTimerAndWrite(QuicPacketBatch& packets, ExceptionSink* xsink);

    //! Process a batch of received packets under a single lock acquisition
    /** Replaces N separate readPacket() calls with a single lock acquisition.
        On per-packet error: logs and continues. On DRAINING/CLOSING: breaks early.
        @param packets array of received packets
        @param count number of packets in the array
        @param xsink exception sink
        @return 0 on success, -1 on fatal error
    */
    DLLLOCAL int readPacketBatch(const QuicReceivedPacket* packets, int count, ExceptionSink* xsink);

    //! Submit an HTTP/3 request (client side)
    /** @return stream ID on success, -1 on error */
    DLLLOCAL int64_t submitRequest(const char* method, const char* path,
                          const strcase_str_map_t& headers,
                          const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit an HTTP/3 response (server side)
    DLLLOCAL int submitResponse(int64_t stream_id, int status_code,
                       const strcase_str_map_t& headers,
                       const void* body, size_t body_len, ExceptionSink* xsink);

    //! Submit an HTTP/3 streaming response (server side, headers only)
    /** Sets up a deferred data reader callback that will return NGHTTP3_ERR_WOULDBLOCK
        until data is provided via sendStreamData().

        @param stream_id the HTTP/3 stream ID to respond on
        @param status_code the HTTP status code
        @param headers response headers
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int submitResponseStreaming(int64_t stream_id, int status_code,
                       const strcase_str_map_t& headers, ExceptionSink* xsink);

    //! Send body data on a streaming HTTP/3 response
    /** Appends data to the stream's buffer and resumes the nghttp3 deferred data
        reader if it was waiting.

        @param stream_id the HTTP/3 stream ID
        @param data body data to send (nullptr with end_stream=true to signal EOF)
        @param len length of data
        @param end_stream true if this is the last chunk
        @param xsink exception sink
        @return 0 on success, 1 if buffer full (backpressure), -1 on error
    */
    DLLLOCAL int sendStreamData(int64_t stream_id, const void* data, size_t len,
                       bool end_stream, ExceptionSink* xsink);

    //! Wait for a streaming body buffer to drain below the backpressure threshold
    /** Blocks the calling thread until the stream's buffered data drops below
        QUIC_MAX_STREAM_BODY, the stream is closed, or the timeout expires.
        Only acquires mtx_ (NOT the socket lock priv->m), so this is safe to call
        from a handler thread that does not hold the socket lock.

        @param stream_id the HTTP/3 stream ID
        @param timeout_ms maximum wait time in milliseconds (0 = no wait, -1 = infinite)
        @return 0 if buffer drained, 1 if timed out, -1 if stream not found or closed
    */
    DLLLOCAL int waitForStreamDrain(int64_t stream_id, int timeout_ms);

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

    //! Tear down and re-create the HTTP/3 layer (used after 0-RTT rejection)
    /** When 0-RTT is rejected, all 0-RTT streams are discarded by ngtcp2, including
        the HTTP/3 control and QPACK streams opened during early data. This method
        tears down the old h3 connection and creates a fresh one.
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int resetHttp3(ExceptionSink* xsink);

    //! Check if HTTP/3 layer is initialized
    DLLLOCAL bool isHttp3Ready() const {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        return h3_conn_ != nullptr;
    }

    //! Check if this session is attempting 0-RTT
    DLLLOCAL bool isAttempting0Rtt() const { return attempting_0rtt_.load(std::memory_order_acquire); }

    //! Check if 0-RTT early data was rejected by the server
    DLLLOCAL bool isEarlyDataRejected() const { return early_data_rejected_.load(std::memory_order_acquire); }

    //! Check if 0-RTT TX key is installed (can send early data)
    DLLLOCAL bool isEarlyDataReady() const { return early_data_ready_.load(std::memory_order_acquire); }

    //! Check if there is pending data to write (set after submitRequest/submitResponse)
    DLLLOCAL bool hasPendingWrite() const { return pending_write_.load(std::memory_order_acquire); }

    //! Clear the pending write flag (called after writePackets flushes all data)
    DLLLOCAL void clearPendingWrite() { pending_write_.store(false, std::memory_order_release); }

    //! Get current timestamp in nanoseconds (monotonic clock)
    DLLLOCAL static ngtcp2_tstamp timestamp();

    //! Copy the stored remote peer address into caller-provided buffers
    /** Thread-safe: acquires mtx_ to ensure the address and length are read
        atomically.  Copies only the actual address length, not the full
        sockaddr_storage.
        @param out destination buffer (must be at least sizeof(sockaddr_storage))
        @param out_len set to the actual address length
    */
    DLLLOCAL void getRemoteAddrCopy(struct sockaddr_storage& out, socklen_t& out_len) const {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        memcpy(&out, &remote_addr_, remote_addrlen_);
        out_len = remote_addrlen_;
    }

    //! Copy the stored local address into caller-provided buffers
    /** Thread-safe: acquires mtx_ to ensure the address and length are read
        atomically.  Copies only the actual address length, not the full
        sockaddr_storage.
        @param out destination buffer (must be at least sizeof(sockaddr_storage))
        @param out_len set to the actual address length
    */
    DLLLOCAL void getLocalAddrCopy(struct sockaddr_storage& out, socklen_t& out_len) const {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        memcpy(&out, &local_addr_, local_addrlen_);
        out_len = local_addrlen_;
    }

    //! Update the stored remote peer address (called after path validation succeeds)
    /** Thread-safe: acquires mtx_ internally.
        @param addr new remote address
        @param len address length
    */
    DLLLOCAL void updateRemoteAddr(const struct sockaddr* addr, socklen_t len);

    //! Update the stored remote peer address — caller MUST hold mtx_
    /** Used by ngtcp2 callbacks (pathValidationCallback) and writePacketsLocked()
        which are already executing under mtx_.  Avoids recursive mutex acquisition.
        @param addr new remote address
        @param len address length
    */
    DLLLOCAL void updateRemoteAddrLocked(const struct sockaddr* addr, socklen_t len);

    //! Update the stored local address (called after path validation succeeds)
    /** Thread-safe: acquires mtx_ internally.
        @param addr new local address
        @param len address length
    */
    DLLLOCAL void updateLocalAddr(const struct sockaddr* addr, socklen_t len);

    //! Update the stored local address — caller MUST hold mtx_
    /** Used by ngtcp2 callbacks (pathValidationCallback) which are already executing
        under mtx_.  Avoids recursive mutex acquisition.
        @param addr new local address
        @param len address length
    */
    DLLLOCAL void updateLocalAddrLocked(const struct sockaddr* addr, socklen_t len);

    //! Get the migration generation counter (incremented on each path update)
    /** Memory ordering: acquire pairs with the release in updateRemoteAddr() /
        writePacketsLocked() to ensure the caller sees the updated address data
        (memcpy'd under mtx_) after observing the incremented counter. */
    DLLLOCAL uint64_t getMigrationGen() const { return migration_gen_.load(std::memory_order_acquire); }

    //! Check if a path migration has occurred since the last clearPathMigrated() call
    /** Memory ordering: acquire pairs with the release store in updateRemoteAddr()
        to ensure the new address data is visible when the flag reads true. */
    DLLLOCAL bool hasPathMigrated() const { return path_migrated_.load(std::memory_order_acquire); }

    //! Clear the path migration flag (called after cached addresses are refreshed)
    /** Memory ordering: relaxed would suffice (single-threaded server poll loop),
        but release is used for consistency with the store-true path and to remain
        correct if the threading model ever changes. */
    DLLLOCAL void clearPathMigrated() { path_migrated_.store(false, std::memory_order_release); }

    //! Initiate client-side connection migration to a new network path
    /** Calls ngtcp2_conn_initiate_immediate_migration() to start sending on
        the new path immediately while validating in the background. If validation
        fails, ngtcp2 falls back to the old path automatically.

        @param new_local new local address (from getsockname on new socket)
        @param new_local_len local address length
        @param new_remote new remote address
        @param new_remote_len remote address length
        @param xsink exception sink
        @return 0 on success, -1 on error
    */
    DLLLOCAL int initiateMigration(
        const struct sockaddr* new_local, socklen_t new_local_len,
        const struct sockaddr* new_remote, socklen_t new_remote_len,
        ExceptionSink* xsink);

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
                   int ssl_verify_mode = SSL_VERIFY_NONE,
                   bool enable_0rtt = false);

    //! Initialize server-side connection
    DLLLOCAL int initServer(qore_socket_private* sock, ExceptionSink* xsink,
                   const ngtcp2_pkt_hd* initial_hdr,
                   QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                   const struct sockaddr* local_addr, socklen_t local_addrlen,
                   const struct sockaddr* remote_addr, socklen_t remote_addrlen,
                   QoreDatagramDispatcher* dispatcher,
                   SSL_CTX* shared_ssl_ctx = nullptr);

    //! Set up SSL_CTX for client
    DLLLOCAL int setupClientSslCtx(const char* host, int ssl_verify_mode, ExceptionSink* xsink);

    //! Set up SSL_CTX for server with certificate and private key
    /** @param cert X.509 certificate (used only when shared_ssl_ctx is nullptr)
        @param pk private key (used only when shared_ssl_ctx is nullptr)
        @param xsink exception sink
        @param shared_ssl_ctx optional shared SSL_CTX for session ticket key continuity
    */
    DLLLOCAL int setupServerSslCtx(QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                          ExceptionSink* xsink, SSL_CTX* shared_ssl_ctx = nullptr);

    //! Get timer expiry without locking (caller must hold mtx_)
    DLLLOCAL ngtcp2_tstamp getExpiryLocked() const;

    //! Handle timer expiry without locking (caller must hold mtx_)
    DLLLOCAL int handleExpiryLocked(ExceptionSink* xsink);

    //! Generate outgoing packets without locking (caller must hold mtx_)
    DLLLOCAL int writePacketsLocked(QuicPacketBatch& batch, ExceptionSink* xsink);

    //! Process incoming packet without locking (caller must hold mtx_)
    DLLLOCAL int readPacketLocked(const uint8_t* data, size_t len,
                                  const ngtcp2_path& path, ExceptionSink* xsink);

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

    //! Receive TX key (for HTTP/3 uni stream binding and 0-RTT detection)
    DLLLOCAL static int recvTxKeyCallback(ngtcp2_conn* conn,
                                 ngtcp2_encryption_level level,
                                 void* user_data);

    //! TLS new session ticket callback (for caching 0-RTT session tickets)
    DLLLOCAL static int newSessionTicketCallback(SSL* ssl, SSL_SESSION* session);

    //! TLS early data rejected callback (for 0-RTT rejection handling)
    DLLLOCAL static int earlyDataRejectedCallback(ngtcp2_conn* conn, void* user_data);

    //! Path validation result callback (SUCCESS/FAILURE/ABORTED)
    /** On SUCCESS: updates stored addresses via updateRemoteAddr()/updateLocalAddr().
        On FAILURE/ABORTED: logs a diagnostic message.
    */
    DLLLOCAL static int pathValidationCallback(ngtcp2_conn* conn,
                                               uint32_t flags,
                                               const ngtcp2_path* path,
                                               const ngtcp2_path* old_path,
                                               ngtcp2_path_validation_result res,
                                               void* user_data);

    //! Path validation started callback (logging only)
    DLLLOCAL static int beginPathValidationCallback(ngtcp2_conn* conn,
                                                    uint32_t flags,
                                                    const ngtcp2_path* path,
                                                    const ngtcp2_path* fallback_path,
                                                    void* user_data);

    //! Connection ID removal callback (unregisters retired CID from dispatcher)
    DLLLOCAL static int removeConnectionIdCallback(ngtcp2_conn* conn,
                                                   const ngtcp2_cid* cid,
                                                   void* user_data);

    //! DCID status callback (logging only: activate/deactivate lifecycle)
    DLLLOCAL static int dcidStatusCallback(ngtcp2_conn* conn,
                                           ngtcp2_connection_id_status_type type,
                                           uint64_t seq,
                                           const ngtcp2_cid* cid,
                                           const uint8_t* token,
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
    SSL_CTX* ssl_ctx_ = nullptr;                    //!< OpenSSL context (always ref-counted; freed in destructor)
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
    std::atomic<bool> attempting_0rtt_{false};          //!< true when attempting 0-RTT connection
    std::atomic<bool> early_data_rejected_{false};    //!< true when 0-RTT early data was rejected by server
    std::atomic<bool> early_data_ready_{false};       //!< true when 0-RTT TX key installed (can send early data)
    std::atomic<bool> goaway_sent_{false};           //!< true when final GOAWAY (submitShutdown) has been queued; false during notice-only phase
    std::atomic<bool> goaway_received_{false};       //!< true when GOAWAY received from peer
    std::atomic<bool> path_migrated_{false};         //!< set by pathValidationCallback on SUCCESS; cleared by clearPathMigrated()
    //! Address change generation counter — incremented each time remote_addr_ is updated.
    /** NOTE: this is a "change generation", NOT a "migration count".  A single
        connection migration can trigger 2-3 increments: once from initiateMigration(),
        once from writePacketsLocked() (ngtcp2 output path), and/or once from
        pathValidationCallback().  This is harmless — the counter's purpose is stale
        address detection, not counting logical migration events.  Consumers must not
        assume a 1:1 mapping between increments and migration operations.
    */
    std::atomic<uint64_t> migration_gen_{0};
    int64_t goaway_max_stream_id_{-1};               //!< max stream ID from received GOAWAY; protected by mtx_
    std::string host_;                               //!< server hostname for :authority fallback
    uint16_t port_ = 0;                             //!< server port

    //! Active streams (unordered_map for O(1) lookup vs O(log n) with std::map)
    std::unordered_map<int64_t, std::unique_ptr<QuicStreamInfo>> streams_;

    //! Queue of completed stream IDs
    std::queue<int64_t> completed_streams_;

    //! Body data for streams being sent (used by data reader callback)
    std::unordered_map<int64_t, QuicBodyData> body_data_;

    //! Streaming body data for incremental sending (used by data reader callback)
    std::unordered_map<int64_t, QuicStreamingBodyData> streaming_body_data_;

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

    //! Condition variable for stream drain notifications (backpressure)
    /** Signaled by h3ReadDataCallback when it consumes data from a streaming
        buffer, and by streamCloseCallback when a stream is closed.

        Uses a separate non-recursive drain_mtx_ + drain_cv_ pair with an atomic
        generation counter to avoid both:
        - POSIX UB of pthread_cond_wait with PTHREAD_MUTEX_RECURSIVE
        - the extra internal mutex overhead of std::condition_variable_any

        The generation counter prevents lost wakeups: signal sites increment
        drain_gen_ and briefly lock drain_mtx_ before notify; the waiter checks
        drain_gen_ under drain_mtx_ then re-checks the actual predicate under mtx_.

        Lock ordering: signal sites hold mtx_ then briefly acquire drain_mtx_;
        the waiter holds drain_mtx_ (for CV wait) then separately acquires mtx_
        (never simultaneously), so no deadlock is possible.
    */
    std::mutex drain_mtx_;
    std::condition_variable drain_cv_;
    std::atomic<unsigned> drain_gen_{0};
};

#endif // _QORE_INTERN_QUICSESSION_H
