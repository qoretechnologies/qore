/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QuicSession.cpp

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

#include <qore/Qore.h>

#include "qore/intern/QuicSession.h"

#include <openssl/rand.h>
#include <openssl/err.h>

#include "qore/intern/QoreDatagramDispatcher.h"

#include <unordered_set>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <ctime>

// Static session ID counter
std::atomic<int64_t> QuicSession::next_session_id_{1};

// HTTP/3 forbids hop-by-hop headers from HTTP/1.x (RFC 9114 Section 4.2)
static const auto& h3_forbidden_headers = getForbiddenHopByHopHeaders();

// ALPN protocol ID for HTTP/3
static const uint8_t H3_ALPN[] = "\x02h3";
static const size_t H3_ALPN_LEN = sizeof(H3_ALPN) - 1;

// Thread-safe one-time initialization for ngtcp2 OpenSSL crypto backend
static std::once_flag ossl_init_flag;
static void ensureOsslInit() {
    std::call_once(ossl_init_flag, []() { ngtcp2_crypto_ossl_init(); });
}

// ===== Timestamp helper =====

ngtcp2_tstamp QuicSession::timestamp() {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    return static_cast<ngtcp2_tstamp>(tp.tv_sec) * NGTCP2_SECONDS
         + static_cast<ngtcp2_tstamp>(tp.tv_nsec);
}

// ===== Constructor / Destructor =====

QuicSession::QuicSession() : session_id_(next_session_id_.fetch_add(1, std::memory_order_relaxed)) {
    // conn_ref_ is value-initialized (zero) by the brace initializer in the header
}

QuicSession::~QuicSession() {
    // Unregister all CIDs from the dispatcher before destroying the connection.
    // Lifetime requirement: the dispatcher (owned by qore_socket_private) must outlive
    // all sessions; qore_socket_private destroys sessions (via shared_ptr) before itself.
    if (dispatcher_ && conn_) {
        try {
            size_t num_scids = ngtcp2_conn_get_scid(conn_, nullptr);
            if (num_scids > 0) {
                std::vector<ngtcp2_cid> cids(num_scids);
                ngtcp2_conn_get_scid(conn_, cids.data());
                for (const auto& cid : cids) {
                    std::string cid_str(reinterpret_cast<const char*>(cid.data), cid.datalen);
                    dispatcher_->unregisterConnectionId(cid_str);
                }
            }
        } catch (...) {
            // Guard against std::bad_alloc from vector allocation in destructor;
            // CIDs will be unreachable once the connection is destroyed anyway
        }
        dispatcher_ = nullptr;
    }

    if (h3_conn_) {
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
    }
    if (conn_) {
        // Clear app_data before freeing SSL to avoid dangling pointer
        if (ssl_) {
            SSL_set_app_data(ssl_, nullptr);
        }
        ngtcp2_conn_del(conn_);
        conn_ = nullptr;
    }
    // Destruction order: conn_ → ossl_ctx_ → ssl_ → ssl_ctx_
    // conn_ owns ngtcp2 state that references the TLS context.
    // ossl_ctx_ holds a non-owning reference to ssl_, so it must be freed
    // while ssl_ is still valid.  ssl_ must be freed before ssl_ctx_
    // (SSL_CTX) per OpenSSL convention.
    if (ossl_ctx_) {
        ngtcp2_crypto_ossl_ctx_del(ossl_ctx_);
        ossl_ctx_ = nullptr;
    }
    if (ssl_) {
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (ssl_ctx_) {
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
}

// ===== Factory Methods =====

std::shared_ptr<QuicSession> QuicSession::createClient(
    qore_socket_private* sock, ExceptionSink* xsink,
    const char* host, uint16_t port,
    const struct sockaddr* local_addr, socklen_t local_addrlen,
    const struct sockaddr* remote_addr, socklen_t remote_addrlen,
    int ssl_verify_mode) {
    assert(local_addr);
    assert(remote_addr);

    ensureOsslInit();

    auto session = std::shared_ptr<QuicSession>(new QuicSession());
    if (session->initClient(sock, xsink, host, port,
                            local_addr, local_addrlen,
                            remote_addr, remote_addrlen,
                            ssl_verify_mode) != 0) {
        return nullptr;
    }
    // Clear the temporary socket pointer; it was only needed during init
    session->clearSockPtr();
    return session;
}

std::shared_ptr<QuicSession> QuicSession::createServer(
    qore_socket_private* sock, ExceptionSink* xsink,
    const ngtcp2_pkt_hd* initial_hdr,
    QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
    const struct sockaddr* local_addr, socklen_t local_addrlen,
    const struct sockaddr* remote_addr, socklen_t remote_addrlen,
    QoreDatagramDispatcher* dispatcher) {
    assert(cert);
    assert(pk);
    assert(local_addr);
    assert(remote_addr);

    ensureOsslInit();

    auto session = std::shared_ptr<QuicSession>(new QuicSession());
    if (session->initServer(sock, xsink, initial_hdr, cert, pk,
                            local_addr, local_addrlen,
                            remote_addr, remote_addrlen,
                            dispatcher) != 0) {
        return nullptr;
    }
    // Clear the temporary socket pointer; it was only needed during init
    session->clearSockPtr();
    return session;
}

// ===== SSL Context Setup =====

int QuicSession::setupClientSslCtx(const char* host, int ssl_verify_mode, ExceptionSink* xsink) {
    ssl_ctx_ = SSL_CTX_new(TLS_client_method());
    if (!ssl_ctx_) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create SSL_CTX");
        return -1;
    }

    // Set minimum TLS version to 1.3 (required for QUIC)
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_3_VERSION);

    // Load system default CA certificates
    SSL_CTX_set_default_verify_paths(ssl_ctx_);

    // Enable certificate verification if requested
    SSL_CTX_set_verify(ssl_ctx_, ssl_verify_mode, nullptr);

    // Create SSL connection
    ssl_ = SSL_new(ssl_ctx_);
    if (!ssl_) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create SSL object");
        return -1;
    }

    // Configure for QUIC client
    if (ngtcp2_crypto_ossl_configure_client_session(ssl_) != 0) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to configure client SSL session for QUIC");
        return -1;
    }

    SSL_set_connect_state(ssl_);

    // Set ALPN to "h3"
    if (SSL_set_alpn_protos(ssl_, H3_ALPN, H3_ALPN_LEN) != 0) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to set ALPN protocol");
        return -1;
    }

    // Set SNI hostname for virtual hosting
    if (!SSL_set_tlsext_host_name(ssl_, host)) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to set SNI hostname '%s'", host);
        return -1;
    }

    // Create ngtcp2 ossl context
    if (ngtcp2_crypto_ossl_ctx_new(&ossl_ctx_, ssl_) != 0) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create ngtcp2 ossl context");
        return -1;
    }

    return 0;
}

int QuicSession::setupServerSslCtx(QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                                    ExceptionSink* xsink) {
    assert(cert);
    assert(pk);

    ssl_ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx_) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create server SSL_CTX");
        return -1;
    }

    // Set minimum TLS version to 1.3 (required for QUIC)
    SSL_CTX_set_min_proto_version(ssl_ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ssl_ctx_, TLS1_3_VERSION);

    // Load the X.509 certificate for the TLS 1.3 handshake
    if (SSL_CTX_use_certificate(ssl_ctx_, cert->getData()) != 1) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to load server certificate into SSL_CTX");
        return -1;
    }

    // Load the private key for the TLS 1.3 handshake
    EVP_PKEY* pk_data = pk->getData();
    if (!pk_data) {
        xsink->raiseException("QUIC-SSL-ERROR", "server private key getData() returned null");
        return -1;
    }
    if (SSL_CTX_use_PrivateKey(ssl_ctx_, pk_data) != 1) {
        unsigned long ssl_err = ERR_peek_last_error();
        char errbuf[256];
        ERR_error_string_n(ssl_err, errbuf, sizeof(errbuf));
        xsink->raiseException("QUIC-SSL-ERROR",
            "failed to load server private key into SSL_CTX: %s (EVP_PKEY type=%d)",
            errbuf, EVP_PKEY_base_id(pk_data));
        return -1;
    }

    // Verify cert/key match
    if (SSL_CTX_check_private_key(ssl_ctx_) != 1) {
        xsink->raiseException("QUIC-SSL-ERROR", "server certificate and private key do not match");
        return -1;
    }

    // Set ALPN selection callback for the server
    // The server must select "h3" from the client's ALPN list for HTTP/3
    SSL_CTX_set_alpn_select_cb(ssl_ctx_,
        [](SSL*, const unsigned char** out, unsigned char* outlen,
           const unsigned char* in, unsigned int inlen, void*) -> int {
            // Use OpenSSL helper to select "h3" from client's list
            if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                    H3_ALPN, H3_ALPN_LEN, in, inlen) != OPENSSL_NPN_NEGOTIATED) {
                return SSL_TLSEXT_ERR_NOACK;
            }
            return SSL_TLSEXT_ERR_OK;
        }, nullptr);

    // Create SSL connection
    ssl_ = SSL_new(ssl_ctx_);
    if (!ssl_) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create server SSL object");
        return -1;
    }

    // Configure for QUIC server
    if (ngtcp2_crypto_ossl_configure_server_session(ssl_) != 0) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to configure server SSL session for QUIC");
        return -1;
    }

    SSL_set_accept_state(ssl_);

    // Create ngtcp2 ossl context
    if (ngtcp2_crypto_ossl_ctx_new(&ossl_ctx_, ssl_) != 0) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create server ngtcp2 ossl context");
        return -1;
    }

    return 0;
}

// ===== Connection Initialization =====


int QuicSession::initClient(qore_socket_private* sock, ExceptionSink* xsink,
                            const char* host, uint16_t port,
                            const struct sockaddr* local_addr, socklen_t local_addrlen,
                            const struct sockaddr* remote_addr, socklen_t remote_addrlen,
                            int ssl_verify_mode) {
    sock_ = sock;
    is_server_ = false;
    host_ = host;
    port_ = port;

    // Store addresses for path construction
    if (local_addrlen > sizeof(local_addr_) || remote_addrlen > sizeof(remote_addr_)) {
        xsink->raiseException("QUIC-ERROR",
            "address length exceeds sockaddr_storage: local=%d (max %zu), remote=%d (max %zu)",
            (int)local_addrlen, sizeof(local_addr_), (int)remote_addrlen, sizeof(remote_addr_));
        return -1;
    }
    memcpy(&local_addr_, local_addr, local_addrlen);
    local_addrlen_ = local_addrlen;
    memcpy(&remote_addr_, remote_addr, remote_addrlen);
    remote_addrlen_ = remote_addrlen;

    // Set up SSL with certificate verification
    if (setupClientSslCtx(host, ssl_verify_mode, xsink) != 0) {
        return -1;
    }

    // Set up conn_ref for TLS integration
    conn_ref_.get_conn = getConnFromRef;
    conn_ref_.user_data = this;
    SSL_set_app_data(ssl_, &conn_ref_);

    // Generate random connection IDs
    ngtcp2_cid dcid, scid;
    dcid.datalen = NGTCP2_MIN_INITIAL_DCIDLEN;
    if (RAND_bytes(dcid.data, static_cast<int>(dcid.datalen)) != 1) {
        xsink->raiseException("QUIC-ERROR", "failed to generate random DCID");
        return -1;
    }
    scid.datalen = QUIC_CLIENT_SCID_LEN;
    if (RAND_bytes(scid.data, static_cast<int>(scid.datalen)) != 1) {
        xsink->raiseException("QUIC-ERROR", "failed to generate random SCID");
        return -1;
    }

    // Set up callbacks
    ngtcp2_callbacks callbacks{};
    callbacks.client_initial = ngtcp2_crypto_client_initial_cb;
    callbacks.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
    callbacks.encrypt = ngtcp2_crypto_encrypt_cb;
    callbacks.decrypt = ngtcp2_crypto_decrypt_cb;
    callbacks.hp_mask = ngtcp2_crypto_hp_mask_cb;
    callbacks.recv_retry = ngtcp2_crypto_recv_retry_cb;
    callbacks.update_key = ngtcp2_crypto_update_key_cb;
    callbacks.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
    callbacks.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
    callbacks.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
    callbacks.version_negotiation = ngtcp2_crypto_version_negotiation_cb;

    // Custom callbacks
    callbacks.rand = randCallback;
    callbacks.get_new_connection_id = getNewConnectionIdCallback;
    callbacks.recv_stream_data = recvStreamDataCallback;
    callbacks.acked_stream_data_offset = ackedStreamDataOffsetCallback;
    callbacks.stream_close = streamCloseCallback;
    callbacks.handshake_completed = handshakeCompletedCallback;
    callbacks.extend_max_local_streams_bidi = extendMaxLocalStreamsBidiCallback;
    callbacks.extend_max_stream_data = extendMaxStreamDataCallback;
    callbacks.recv_tx_key = recvTxKeyCallback;

    // Settings
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = timestamp();

    // Transport parameters
    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_streams_uni = QUIC_INITIAL_MAX_STREAMS_UNI;
    params.initial_max_streams_bidi = QUIC_INITIAL_MAX_STREAMS_BIDI;
    params.initial_max_stream_data_bidi_local = QUIC_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_bidi_remote = QUIC_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_uni = QUIC_INITIAL_MAX_STREAM_DATA;
    params.initial_max_data = QUIC_INITIAL_MAX_DATA;
    // Idle timeout: ngtcp2 default is 0 (no timeout), which prevents cleanup of
    // lost connections and enables resource exhaustion.  30s is standard for clients.
    params.max_idle_timeout = QUIC_IDLE_TIMEOUT_NS;

    // Build path from actual socket addresses
    ngtcp2_path path;
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr_);
    path.local.addrlen = local_addrlen_;
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&remote_addr_);
    path.remote.addrlen = remote_addrlen_;

    // Create client connection
    int rv = ngtcp2_conn_client_new(&conn_, &dcid, &scid, &path,
                                    NGTCP2_PROTO_VER_V1,
                                    &callbacks, &settings, &params,
                                    nullptr, this);
    if (rv != 0) {
        xsink->raiseException("QUIC-ERROR", "ngtcp2_conn_client_new failed: %s",
                              ngtcp2_strerror(rv));
        return -1;
    }

    // Set TLS native handle (for ossl backend, pass ossl_ctx_ not ssl_)
    ngtcp2_conn_set_tls_native_handle(conn_, ossl_ctx_);

    return 0;
}

int QuicSession::initServer(qore_socket_private* sock, ExceptionSink* xsink,
                            const ngtcp2_pkt_hd* initial_hdr,
                            QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                            const struct sockaddr* local_addr, socklen_t local_addrlen,
                            const struct sockaddr* remote_addr, socklen_t remote_addrlen,
                            QoreDatagramDispatcher* dispatcher) {
    sock_ = sock;
    is_server_ = true;
    dispatcher_ = dispatcher;

    // Store addresses for path construction
    if (local_addrlen > sizeof(local_addr_) || remote_addrlen > sizeof(remote_addr_)) {
        xsink->raiseException("QUIC-ERROR",
            "address length exceeds sockaddr_storage: local=%d (max %zu), remote=%d (max %zu)",
            (int)local_addrlen, sizeof(local_addr_), (int)remote_addrlen, sizeof(remote_addr_));
        return -1;
    }
    memcpy(&local_addr_, local_addr, local_addrlen);
    local_addrlen_ = local_addrlen;
    memcpy(&remote_addr_, remote_addr, remote_addrlen);
    remote_addrlen_ = remote_addrlen;

    // Set up SSL with cert/key for TLS 1.3
    if (setupServerSslCtx(cert, pk, xsink) != 0) {
        return -1;
    }

    // Set up conn_ref for TLS integration
    conn_ref_.get_conn = getConnFromRef;
    conn_ref_.user_data = this;
    SSL_set_app_data(ssl_, &conn_ref_);

    // Generate server CID and store it
    ngtcp2_cid scid;
    scid.datalen = NGTCP2_MAX_CIDLEN;
    if (RAND_bytes(scid.data, static_cast<int>(scid.datalen)) != 1) {
        xsink->raiseException("QUIC-ERROR", "failed to generate random server CID");
        return -1;
    }
    scid_ = scid;

    // Set up callbacks
    ngtcp2_callbacks callbacks{};
    callbacks.recv_client_initial = ngtcp2_crypto_recv_client_initial_cb;
    callbacks.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
    callbacks.encrypt = ngtcp2_crypto_encrypt_cb;
    callbacks.decrypt = ngtcp2_crypto_decrypt_cb;
    callbacks.hp_mask = ngtcp2_crypto_hp_mask_cb;
    callbacks.update_key = ngtcp2_crypto_update_key_cb;
    callbacks.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
    callbacks.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
    callbacks.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
    callbacks.version_negotiation = ngtcp2_crypto_version_negotiation_cb;

    // Custom callbacks
    callbacks.rand = randCallback;
    callbacks.get_new_connection_id = getNewConnectionIdCallback;
    callbacks.recv_stream_data = recvStreamDataCallback;
    callbacks.acked_stream_data_offset = ackedStreamDataOffsetCallback;
    callbacks.stream_close = streamCloseCallback;
    callbacks.handshake_completed = handshakeCompletedCallback;
    callbacks.extend_max_remote_streams_bidi = extendMaxRemoteStreamsBidiCallback;
    callbacks.extend_max_stream_data = extendMaxStreamDataCallback;
    callbacks.recv_tx_key = recvTxKeyCallback;

    // Settings
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = timestamp();

    // Transport parameters
    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_streams_uni = QUIC_INITIAL_MAX_STREAMS_UNI;
    params.initial_max_streams_bidi = QUIC_INITIAL_MAX_STREAMS_BIDI;
    params.initial_max_stream_data_bidi_local = QUIC_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_bidi_remote = QUIC_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_uni = QUIC_INITIAL_MAX_STREAM_DATA;
    params.initial_max_data = QUIC_INITIAL_MAX_DATA;
    // Idle timeout: ngtcp2 default is 0 (no timeout), which allows malicious clients
    // to hold server sessions open indefinitely.  30s matches the client setting.
    params.max_idle_timeout = QUIC_IDLE_TIMEOUT_NS;
    params.original_dcid = initial_hdr->dcid;
    params.original_dcid_present = 1;

    // Generate stateless reset token using a random secret
    uint8_t secret[32];
    if (RAND_bytes(secret, sizeof(secret)) != 1) {
        OPENSSL_cleanse(secret, sizeof(secret));
        xsink->raiseException("QUIC-ERROR", "failed to generate random secret");
        return -1;
    }
    int sr_rv = ngtcp2_crypto_generate_stateless_reset_token(
        params.stateless_reset_token, secret, sizeof(secret), &scid);
    OPENSSL_cleanse(secret, sizeof(secret));
    if (sr_rv != 0) {
        xsink->raiseException("QUIC-ERROR", "failed to generate stateless reset token");
        return -1;
    }
    params.stateless_reset_token_present = 1;

    // Build path from actual socket addresses
    ngtcp2_path path;
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_addr_);
    path.local.addrlen = local_addrlen_;
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&remote_addr_);
    path.remote.addrlen = remote_addrlen_;

    // Create server connection
    int rv = ngtcp2_conn_server_new(&conn_, &initial_hdr->scid, &scid, &path,
                                    initial_hdr->version,
                                    &callbacks, &settings, &params,
                                    nullptr, this);
    if (rv != 0) {
        xsink->raiseException("QUIC-ERROR", "ngtcp2_conn_server_new failed: %s",
                              ngtcp2_strerror(rv));
        return -1;
    }

    // Set TLS native handle (for ossl backend, pass ossl_ctx_ not ssl_)
    ngtcp2_conn_set_tls_native_handle(conn_, ossl_ctx_);

    // Register CIDs with dispatcher for CID-based packet routing
    if (dispatcher_) {
        // Register server's own SCID — this is what the client will use as DCID
        // after receiving the server's first response
        std::string scid_str(reinterpret_cast<const char*>(scid_.data), scid_.datalen);
        dispatcher_->registerConnectionId(scid_str, this);

        // Also register the client's original DCID — during the handshake,
        // the client may send additional packets (e.g., coalesced Handshake)
        // using the original DCID before it learns the server's SCID
        std::string odcid_str(reinterpret_cast<const char*>(initial_hdr->dcid.data),
                              initial_hdr->dcid.datalen);
        dispatcher_->registerConnectionId(odcid_str, this);
    }

    return 0;
}

// ===== HTTP/3 Setup =====

int QuicSession::setupHttp3(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (h3_conn_) {
        return 0;  // already set up
    }

    nghttp3_callbacks h3_cbs{};
    h3_cbs.begin_headers = h3BeginHeadersCallback;
    h3_cbs.recv_header = h3RecvHeaderCallback;
    h3_cbs.end_headers = h3EndHeadersCallback;
    h3_cbs.recv_data = h3RecvDataCallback;
    h3_cbs.end_stream = h3EndStreamCallback;
    // NOTE: deferred_consume is NOT registered because the QPACK dynamic
    // table is disabled below (capacity=0, blocked_streams=0).  Without
    // the dynamic table, nghttp3 never calls deferred_consume, so there
    // is no risk of ngtcp2 flow-control offset drift.  Static Huffman
    // encoding is sufficient; the dynamic table adds complexity for
    // marginal compression gain.
    h3_cbs.stop_sending = h3StopSendingCallback;
    h3_cbs.reset_stream = h3ResetStreamCallback;
    h3_cbs.acked_stream_data = h3AckedStreamDataCallback;
    h3_cbs.shutdown = h3ShutdownCallback;

    nghttp3_settings h3_settings;
    nghttp3_settings_default(&h3_settings);
    // Disable QPACK dynamic table — eliminates the need for deferred_consume
    h3_settings.qpack_max_dtable_capacity = 0;
    h3_settings.qpack_blocked_streams = 0;

    auto* mem = nghttp3_mem_default();

    int rv;
    if (is_server_) {
        rv = nghttp3_conn_server_new(&h3_conn_, &h3_cbs, &h3_settings, mem, this);
    } else {
        rv = nghttp3_conn_client_new(&h3_conn_, &h3_cbs, &h3_settings, mem, this);
    }

    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to create HTTP/3 connection: %s",
                              nghttp3_strerror(rv));
        return -1;
    }

    // Open and bind control stream
    int64_t ctrl_stream_id;
    rv = ngtcp2_conn_open_uni_stream(conn_, &ctrl_stream_id, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to open control stream: %s",
                              ngtcp2_strerror(rv));
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
        return -1;
    }
    rv = nghttp3_conn_bind_control_stream(h3_conn_, ctrl_stream_id);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to bind control stream: %s",
                              nghttp3_strerror(rv));
        // Shut down the already-opened control stream at the transport layer
        ngtcp2_conn_shutdown_stream_write(conn_, 0, ctrl_stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
        return -1;
    }

    // Open and bind QPACK encoder + decoder streams
    int64_t qpack_enc_stream_id, qpack_dec_stream_id;
    rv = ngtcp2_conn_open_uni_stream(conn_, &qpack_enc_stream_id, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to open QPACK encoder stream: %s",
                              ngtcp2_strerror(rv));
        ngtcp2_conn_shutdown_stream_write(conn_, 0, ctrl_stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
        return -1;
    }
    rv = ngtcp2_conn_open_uni_stream(conn_, &qpack_dec_stream_id, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to open QPACK decoder stream: %s",
                              ngtcp2_strerror(rv));
        ngtcp2_conn_shutdown_stream_write(conn_, 0, ctrl_stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        ngtcp2_conn_shutdown_stream_write(conn_, 0, qpack_enc_stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
        return -1;
    }
    rv = nghttp3_conn_bind_qpack_streams(h3_conn_, qpack_enc_stream_id, qpack_dec_stream_id);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to bind QPACK streams: %s",
                              nghttp3_strerror(rv));
        ngtcp2_conn_shutdown_stream_write(conn_, 0, qpack_enc_stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        ngtcp2_conn_shutdown_stream_write(conn_, 0, qpack_dec_stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        // NOTE: nghttp3_conn_del() cleans up all registered streams including the
        // control stream opened earlier; the underlying QUIC streams are cleaned up
        // when the QUIC connection is destroyed
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
        return -1;
    }

    // HTTP/3 control + QPACK streams have data to send
    pending_write_.store(true, std::memory_order_release);

    // Replay any stream data that arrived before HTTP/3 was initialized
    if (!pre_h3_buffer_.empty()) {
        ngtcp2_tstamp ts = ngtcp2_conn_get_timestamp(conn_);
        for (auto& buf : pre_h3_buffer_) {
            nghttp3_ssize nconsumed = nghttp3_conn_read_stream2(
                h3_conn_, buf.stream_id, buf.data.data(), buf.data.size(),
                buf.fin ? 1 : 0, ts);
            if (nconsumed < 0) {
                xsink->raiseException("QUIC-HTTP3-ERROR",
                    "failed to replay buffered stream data on stream %" PRId64 ": %s",
                    buf.stream_id, nghttp3_strerror(static_cast<int>(nconsumed)));
                // Shut down the uni-streams at the transport layer before
                // destroying the HTTP/3 connection.  nghttp3_conn_del() only
                // cleans up the HTTP/3 state; the underlying ngtcp2 streams
                // must be explicitly shut down to avoid leaking them.
                ngtcp2_conn_shutdown_stream_write(conn_, 0, ctrl_stream_id,
                    NGHTTP3_H3_INTERNAL_ERROR);
                ngtcp2_conn_shutdown_stream_write(conn_, 0, qpack_enc_stream_id,
                    NGHTTP3_H3_INTERNAL_ERROR);
                ngtcp2_conn_shutdown_stream_write(conn_, 0, qpack_dec_stream_id,
                    NGHTTP3_H3_INTERNAL_ERROR);
                nghttp3_conn_del(h3_conn_);
                h3_conn_ = nullptr;
                // Clear stale buffer to prevent double-replay on retry
                pre_h3_buffer_.clear();
                pre_h3_buffer_size_ = 0;
                return -1;
            }
            // Extend flow control for the replayed data
            ngtcp2_conn_extend_max_stream_offset(conn_, buf.stream_id,
                                                  static_cast<uint64_t>(nconsumed));
            ngtcp2_conn_extend_max_offset(conn_, static_cast<uint64_t>(nconsumed));
        }
        pre_h3_buffer_.clear();
        pre_h3_buffer_size_ = 0;
    }

    return 0;
}

// ===== Packet I/O =====

int QuicSession::readPacketLocked(const uint8_t* data, size_t len,
                                  const ngtcp2_path& path, ExceptionSink* xsink) {
    if (!conn_) {
        xsink->raiseException("QUIC-ERROR", "QUIC connection not initialized");
        return -1;
    }

    ngtcp2_pkt_info pi{};

    int rv = ngtcp2_conn_read_pkt(conn_, &path, &pi, data, len, timestamp());
    if (rv != 0) {
        // Handle specific error codes
        if (rv == NGTCP2_ERR_DRAINING || rv == NGTCP2_ERR_CLOSING) {
            return 0;  // connection closing gracefully
        }
        // Get more detailed error info from ngtcp2 and OpenSSL
        const ngtcp2_ccerr* ccerr = ngtcp2_conn_get_ccerr(conn_);
        int tls_error = ngtcp2_conn_get_tls_error(conn_);
        unsigned long ssl_err = ERR_peek_last_error();
        char ssl_err_buf[256];
        ERR_error_string_n(ssl_err, ssl_err_buf, sizeof(ssl_err_buf));
        int ssl_get_err = ssl_ ? SSL_get_error(ssl_, 0) : -1;
        xsink->raiseException("QUIC-READ-ERROR",
            "ngtcp2_conn_read_pkt failed: %s (rv=%d); ccerr: type=%d error_code=0x%llx; "
            "tls_error=%d; ssl_err: %s; SSL_get_error=%d",
            ngtcp2_strerror(rv), rv,
            (int)ccerr->type, (long long)ccerr->error_code,
            tls_error, ssl_err_buf, ssl_get_err);
        return -1;
    }

    // Signal that we have pending ACKs/crypto data to flush.
    // NOTE: This is set unconditionally because ngtcp2_conn_read_pkt() always
    // generates ACK frames that need to be sent.  Skipping this would cause
    // delayed ACKs and potential retransmissions from the peer.
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

int QuicSession::readPacket(const uint8_t* data, size_t len,
                            const ngtcp2_path& path, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    return readPacketLocked(data, len, path, xsink);
}

int QuicSession::readPacketBatch(const QuicReceivedPacket* packets, int count,
                                  ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!conn_) {
        xsink->raiseException("QUIC-ERROR", "QUIC connection not initialized");
        return -1;
    }

    for (int i = 0; i < count; ++i) {
        ngtcp2_pkt_info pi{};

        int rv = ngtcp2_conn_read_pkt(conn_, &packets[i].path, &pi,
                                       packets[i].data, packets[i].len, timestamp());
        if (rv != 0) {
            if (rv == NGTCP2_ERR_DRAINING || rv == NGTCP2_ERR_CLOSING) {
                break;  // connection shutting down — no more packets can be processed
            }
            // Log per-packet errors and continue (matching existing batch error handling)
            printd(1, "QuicSession::readPacketBatch(): packet %d/%d failed: %s (rv=%d)\n",
                i, count, ngtcp2_strerror(rv), rv);
            continue;
        }
    }

    // Signal pending write once for the entire batch
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

int QuicSession::writePackets(QuicPacketBatch& packets, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    return writePacketsLocked(packets, xsink);
}

int QuicSession::writePacketsLocked(QuicPacketBatch& packets, ExceptionSink* xsink) {
    if (!conn_) {
        xsink->raiseException("QUIC-ERROR", "QUIC connection not initialized");
        return -1;
    }

    ngtcp2_tstamp ts = timestamp();
    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);
    ngtcp2_pkt_info pi{};
    int total_packets = 0;

    // Safety limit: prevent unbounded looping if ngtcp2 never returns nwrite==0.
    // 4096 iterations is generous (covers large initial windows and GSO batches)
    // while catching any infinite-loop bugs.
    constexpr int MAX_WRITE_ITERATIONS = 4096;
    int iterations = 0;

    for (;;) {
        if (++iterations > MAX_WRITE_ITERATIONS) {
            xsink->raiseException("QUIC-WRITE-ERROR",
                "writePackets() exceeded %d iteration safety limit; "
                "possible infinite loop in ngtcp2 write cycle", MAX_WRITE_ITERATIONS);
            return -1;
        }
        int64_t stream_id = -1;
        int fin = 0;
        nghttp3_vec vec[16];
        nghttp3_ssize sveccnt = 0;

        // Ask HTTP/3 layer for data to send
        if (h3_conn_ && ngtcp2_conn_get_max_data_left(conn_)) {
            sveccnt = nghttp3_conn_writev_stream(h3_conn_, &stream_id, &fin,
                                                  vec, 16);
            if (sveccnt < 0) {
                xsink->raiseException("QUIC-HTTP3-ERROR",
                    "nghttp3_conn_writev_stream failed: %s",
                    nghttp3_strerror(static_cast<int>(sveccnt)));
                return -1;
            }
        }

        // Use MORE flag only when there's actual stream data to coalesce;
        // when stream_id is -1 (no stream data), omit MORE so ngtcp2 flushes
        // any buffered data (including remembered FIN) into a packet
        uint32_t flags = 0;
        if (stream_id >= 0) {
            flags |= NGTCP2_WRITE_STREAM_FLAG_MORE;
        }
        if (fin) {
            flags |= NGTCP2_WRITE_STREAM_FLAG_FIN;
        }

        ngtcp2_ssize ndatalen = -1;
        ngtcp2_ssize nwrite = ngtcp2_conn_writev_stream(
            conn_, &ps.path, &pi, pkt_buf_, sizeof(pkt_buf_),
            &ndatalen, flags, stream_id,
            reinterpret_cast<const ngtcp2_vec*>(vec),
            static_cast<size_t>(sveccnt), ts);

        if (nwrite < 0) {
            switch (nwrite) {
            case NGTCP2_ERR_STREAM_DATA_BLOCKED:
                assert(ndatalen == -1);
                if (h3_conn_) {
                    nghttp3_conn_block_stream(h3_conn_, stream_id);
                }
                continue;
            case NGTCP2_ERR_STREAM_SHUT_WR:
                assert(ndatalen == -1);
                if (h3_conn_) {
                    nghttp3_conn_shutdown_stream_write(h3_conn_, stream_id);
                }
                continue;
            case NGTCP2_ERR_WRITE_MORE:
                assert(ndatalen >= 0);
                if (h3_conn_) {
                    nghttp3_conn_add_write_offset(h3_conn_, stream_id,
                                                   static_cast<uint64_t>(ndatalen));
                }
                continue;
            case NGTCP2_ERR_CLOSING:
            case NGTCP2_ERR_DRAINING:
                // Connection is shutting down — no more packets can be written.
                // This is expected after idle timeout or peer-initiated close.
                pending_write_.store(false, std::memory_order_release);
                return total_packets;
            default:
                xsink->raiseException("QUIC-WRITE-ERROR",
                    "ngtcp2_conn_writev_stream failed: %s",
                    ngtcp2_strerror(static_cast<int>(nwrite)));
                return -1;
            }
        }

        if (ndatalen >= 0 && h3_conn_) {
            nghttp3_conn_add_write_offset(h3_conn_, stream_id,
                                           static_cast<uint64_t>(ndatalen));
        }

        if (nwrite == 0) {
            // Only clear pending_write_ when there is genuinely nothing more
            // to write.  When stream_id >= 0, nghttp3 has queued stream data
            // but ngtcp2 cannot write it right now (congestion or flow control);
            // keep pending_write_ set so the next read cycle (ACKs that open
            // the congestion window) triggers another write attempt.
            // When stream_id < 0, no stream data is pending — clear the flag
            // to avoid busy-looping on POLLOUT.
            if (stream_id < 0) {
                pending_write_.store(false, std::memory_order_release);
            }
            break;
        }

        // Each ngtcp2_conn_writev_stream result is a separate QUIC packet
        // that must be sent as its own UDP datagram
        packets.addPacket(pkt_buf_, static_cast<size_t>(nwrite));
        ++total_packets;
    }

    return total_packets;
}

// ===== Timer Handling =====

ngtcp2_tstamp QuicSession::getExpiryLocked() const {
    if (!conn_) {
        return UINT64_MAX;
    }
    return ngtcp2_conn_get_expiry(conn_);
}

ngtcp2_tstamp QuicSession::getExpiry() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    return getExpiryLocked();
}

int QuicSession::handleExpiryLocked(ExceptionSink* xsink) {
    if (!conn_) {
        xsink->raiseException("QUIC-ERROR", "QUIC connection not initialized");
        return -1;
    }

    int rv = ngtcp2_conn_handle_expiry(conn_, timestamp());
    if (rv != 0) {
        if (rv == NGTCP2_ERR_IDLE_CLOSE) {
            // Idle timeout is a normal condition — the connection should be
            // closed gracefully without raising an exception.  The caller will
            // detect the closed state via isClosed().
            printd(5, "QuicSession::handleExpiry(): idle timeout (session %lld)\n",
                (long long)session_id_);
            // RFC 9000 Section 10.1: idle timeout is a silent close — no
            // CONNECTION_CLOSE frame is sent.  Do NOT set pending_write_ here;
            // ngtcp2 has already transitioned the connection to draining state
            // and writePackets() would get NGTCP2_ERR_DRAINING.
            return 0;
        }
        xsink->raiseException("QUIC-TIMER-ERROR", "ngtcp2_conn_handle_expiry failed: %s",
                              ngtcp2_strerror(rv));
        return -1;
    }

    // Timer expiry generates retransmission packets
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

int QuicSession::handleExpiry(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    return handleExpiryLocked(xsink);
}

QuicTimerWriteResult QuicSession::processTimerAndWrite(QuicPacketBatch& packets,
                                                        ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    QuicTimerWriteResult result;

    // Check and handle timer expiry
    ngtcp2_tstamp expiry = getExpiryLocked();
    if (expiry != UINT64_MAX) {
        ngtcp2_tstamp now = timestamp();
        if (expiry <= now) {
            if (handleExpiryLocked(xsink) < 0) {
                result.error = true;
                return result;
            }
        }
    }

    // Generate outgoing packets
    if (writePacketsLocked(packets, xsink) < 0) {
        result.error = true;
        return result;
    }

    // Get next expiry for poll timeout computation
    result.next_expiry = getExpiryLocked();

    return result;
}

// ===== HTTP/3 Request/Response =====

int64_t QuicSession::submitRequest(const char* method, const char* path,
                                   const strcase_str_map_t& headers,
                                   const void* body, size_t body_len, ExceptionSink* xsink) {
    // Build header name-value pairs OUTSIDE the lock (reserve to avoid
    // reallocations: 4 pseudo-headers + user headers).
    //
    // LIFETIME: nghttp3_nv holds raw pointers into the local variables below
    // (authority, lower_keys, method, path, and headers values).  All are
    // function-scoped and live until nghttp3_conn_submit_request() returns,
    // which copies the data internally.  Do NOT move nva usage past the
    // lifetime of these locals.
    std::vector<nghttp3_nv> nva;
    nva.reserve(headers.size() + 4);
    auto add_nv = [&nva](const char* name, size_t namelen,
                          const char* value, size_t valuelen) {
        nva.push_back({
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)),
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value)),
            namelen, valuelen, NGHTTP3_NV_FLAG_NONE
        });
    };

    // Pseudo-headers
    add_nv(":method", 7, method, strlen(method));
    add_nv(":path", 5, path, strlen(path));
    add_nv(":scheme", 7, "https", 5);

    // Authority from headers or fallback to stored host:port
    std::string authority;
    auto auth_it = headers.find("host");
    if (auth_it != headers.end()) {
        authority = auth_it->second;
    } else if (!host_.empty()) {
        authority = host_;
        if (port_ != 0 && port_ != 443) {
            authority += ":" + std::to_string(port_);
        }
    }
    if (!authority.empty()) {
        add_nv(":authority", 10, authority.c_str(), authority.size());
    }

    // Regular headers (HTTP/3 requires lowercase header names and prohibits
    // hop-by-hop headers from HTTP/1.x per RFC 9114 Section 4.2)
    std::vector<std::string> lower_keys;
    lower_keys.reserve(headers.size());
    for (const auto& h : headers) {
        if (h.first.empty() || h.first[0] == ':' || strcasecmp(h.first.c_str(), "host") == 0) {
            continue;
        }
        std::string lower_key = h.first;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        // Skip HTTP/1.x hop-by-hop headers forbidden in HTTP/3
        if (h3_forbidden_headers.count(lower_key)) {
            continue;
        }
        lower_keys.push_back(std::move(lower_key));
        add_nv(lower_keys.back().c_str(), lower_keys.back().size(),
                h.second.c_str(), h.second.size());
    }

    // Set up body data reader
    nghttp3_data_reader dr;
    nghttp3_data_reader* drp = nullptr;
    if (body && body_len > 0) {
        dr.read_data = h3ReadDataCallback;
        drp = &dr;
    }

    // Lock only for ngtcp2/nghttp3 API calls and state mutations
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }

    // Open a new bidirectional stream
    int64_t stream_id;
    int rv = ngtcp2_conn_open_bidi_stream(conn_, &stream_id, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-ERROR", "failed to open bidi stream: %s",
                              ngtcp2_strerror(rv));
        return -1;
    }

    // Guard: ngtcp2 assigns unique stream IDs, so this should never collide
    // with an in-flight request's body data (matches the pattern in submitResponse())
    assert(!body_data_.count(stream_id));

    // Copy body into owned storage (must be under lock since body_data_ is shared)
    if (body && body_len > 0) {
        auto bp = static_cast<const uint8_t*>(body);
        body_data_[stream_id] = {std::vector<uint8_t>(bp, bp + body_len), 0};
    }

    rv = nghttp3_conn_submit_request(h3_conn_, stream_id, nva.data(), nva.size(),
                                     drp, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "nghttp3_conn_submit_request failed: %s",
                              nghttp3_strerror(rv));
        body_data_.erase(stream_id);
        // Shut down the orphaned bidi stream so it doesn't consume a stream slot
        ngtcp2_conn_shutdown_stream_write(conn_, 0, stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        return -1;
    }

    // Create stream info
    auto* stream = getOrCreateStream(stream_id);
    stream->method = method;
    stream->path = path;
    stream->state = QuicStreamState::Open;

    // Signal that there's data to write
    pending_write_.store(true, std::memory_order_release);

    return stream_id;
}

int QuicSession::submitResponse(int64_t stream_id, int status_code,
                                const strcase_str_map_t& headers,
                                const void* body, size_t body_len, ExceptionSink* xsink) {
    // Build header name-value pairs OUTSIDE the lock (reserve to avoid
    // reallocations: 1 status pseudo-header + user headers).
    //
    // LIFETIME: nghttp3_nv holds raw pointers into the local variables below
    // (status_str, lower_keys, and headers values).  All are function-scoped
    // and live until nghttp3_conn_submit_response() returns, which copies the
    // data internally.  Do NOT move nva usage past the lifetime of these locals.
    std::vector<nghttp3_nv> nva;
    nva.reserve(headers.size() + 1);
    auto add_nv = [&nva](const char* name, size_t namelen,
                          const char* value, size_t valuelen) {
        nva.push_back({
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)),
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value)),
            namelen, valuelen, NGHTTP3_NV_FLAG_NONE
        });
    };

    // Status pseudo-header
    std::string status_str = std::to_string(status_code);
    add_nv(":status", 7, status_str.c_str(), status_str.size());

    // Regular headers (HTTP/3 requires lowercase header names and prohibits
    // hop-by-hop headers from HTTP/1.x per RFC 9114 Section 4.2)
    std::vector<std::string> lower_keys;
    lower_keys.reserve(headers.size());
    for (const auto& h : headers) {
        if (h.first.empty() || h.first[0] == ':') {
            continue;
        }
        std::string lower_key = h.first;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        // Skip HTTP/1.x hop-by-hop headers forbidden in HTTP/3
        if (h3_forbidden_headers.count(lower_key)) {
            continue;
        }
        lower_keys.push_back(std::move(lower_key));
        add_nv(lower_keys.back().c_str(), lower_keys.back().size(),
                h.second.c_str(), h.second.size());
    }

    // Set up body data reader
    nghttp3_data_reader dr;
    nghttp3_data_reader* drp = nullptr;
    if (body && body_len > 0) {
        dr.read_data = h3ReadDataCallback;
        drp = &dr;
    }

    // Lock only for nghttp3 API calls and state mutations
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }

    // Guard against double submitResponse() for the same stream (would overwrite
    // body_data_ while nghttp3 may still hold pointers to the previous body)
    if (body_data_.count(stream_id)) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "response already submitted for stream %" PRId64, stream_id);
        return -1;
    }

    // Copy body into owned storage (must be under lock since body_data_ is shared)
    if (body && body_len > 0) {
        auto bp = static_cast<const uint8_t*>(body);
        body_data_[stream_id] = {std::vector<uint8_t>(bp, bp + body_len), 0};
    }

    int rv = nghttp3_conn_submit_response(h3_conn_, stream_id, nva.data(), nva.size(), drp);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "nghttp3_conn_submit_response failed: %s",
                              nghttp3_strerror(rv));
        body_data_.erase(stream_id);
        return -1;
    }

    // Signal that there's data to write
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

int QuicSession::submitResponseStreaming(int64_t stream_id, int status_code,
                                          const strcase_str_map_t& headers,
                                          ExceptionSink* xsink) {
    // Build header name-value pairs OUTSIDE the lock (same pattern as submitResponse)
    std::vector<nghttp3_nv> nva;
    nva.reserve(headers.size() + 1);
    auto add_nv = [&nva](const char* name, size_t namelen,
                          const char* value, size_t valuelen) {
        nva.push_back({
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)),
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value)),
            namelen, valuelen, NGHTTP3_NV_FLAG_NONE
        });
    };

    // Status pseudo-header
    std::string status_str = std::to_string(status_code);
    add_nv(":status", 7, status_str.c_str(), status_str.size());

    // Regular headers (same filtering as submitResponse)
    std::vector<std::string> lower_keys;
    lower_keys.reserve(headers.size());
    for (const auto& h : headers) {
        if (h.first.empty() || h.first[0] == ':') {
            continue;
        }
        std::string lower_key = h.first;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (h3_forbidden_headers.count(lower_key)) {
            continue;
        }
        lower_keys.push_back(std::move(lower_key));
        add_nv(lower_keys.back().c_str(), lower_keys.back().size(),
                h.second.c_str(), h.second.size());
    }

    // Always set up data reader (streaming will provide data via sendStreamData)
    nghttp3_data_reader dr;
    dr.read_data = h3ReadDataCallback;

    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }

    // Guard against double submit for the same stream
    if (body_data_.count(stream_id) || streaming_body_data_.count(stream_id)) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "response already submitted for stream %" PRId64, stream_id);
        return -1;
    }

    // Create empty streaming body data entry in deferred state
    auto& sbd = streaming_body_data_[stream_id];
    sbd = QuicStreamingBodyData{};
    sbd.deferred = true;

    int rv = nghttp3_conn_submit_response(h3_conn_, stream_id, nva.data(), nva.size(), &dr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "nghttp3_conn_submit_response failed: %s",
                              nghttp3_strerror(rv));
        streaming_body_data_.erase(stream_id);
        return -1;
    }

    // Signal that there's data to write (HEADERS frame)
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

int QuicSession::sendStreamData(int64_t stream_id, const void* data, size_t len,
                                 bool end_stream, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    auto it = streaming_body_data_.find(stream_id);
    if (it == streaming_body_data_.end()) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "no streaming response for stream %" PRId64, stream_id);
        return -1;
    }

    auto& sbd = it->second;

    // Check backpressure: if buffer > 1MB, signal caller to retry
    if (sbd.data.size() - sbd.offset > QUIC_MAX_STREAM_BODY) {
        return 1;
    }

    // Compact buffer if consumed data exceeds half the buffer size
    if (sbd.offset > 0 && sbd.offset > sbd.data.size() / 2) {
        sbd.data.erase(sbd.data.begin(), sbd.data.begin() + sbd.offset);
        sbd.offset = 0;
    }

    // Append new data
    if (data && len > 0) {
        auto bp = static_cast<const uint8_t*>(data);
        sbd.data.insert(sbd.data.end(), bp, bp + len);
    }

    if (end_stream) {
        sbd.eof = true;
    }

    // Resume the deferred data reader if it was waiting
    if (sbd.deferred && h3_conn_) {
        int rv = nghttp3_conn_resume_stream(h3_conn_, stream_id);
        if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND) {
            xsink->raiseException("QUIC-HTTP3-ERROR",
                "nghttp3_conn_resume_stream failed: %s", nghttp3_strerror(rv));
            return -1;
        }
        sbd.deferred = false;
    }

    pending_write_.store(true, std::memory_order_release);
    return 0;
}

int QuicSession::waitForStreamDrain(int64_t stream_id, int timeout_ms) {
    // Phase 1: check predicate under mtx_
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        auto it = streaming_body_data_.find(stream_id);
        if (it == streaming_body_data_.end()) {
            return -1;  // stream not found
        }
        if (it->second.data.size() - it->second.offset <= QUIC_MAX_STREAM_BODY) {
            return 0;  // buffer already below threshold
        }
    }

    if (timeout_ms == 0) {
        return 1;  // no wait requested, buffer is full
    }

    // Phase 2: wait on drain_cv_ with generation-based wakeup
    // The generation counter prevents lost wakeups: if a signal fires between
    // releasing mtx_ above and entering wait below, the gen change is visible
    // immediately when the waiter checks the predicate on drain_cv_.
    unsigned gen = drain_gen_.load(std::memory_order_acquire);
    auto deadline = (timeout_ms > 0)
        ? std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms)
        : std::chrono::steady_clock::time_point::max();

    while (true) {
        // Wait for generation change (signal from h3ReadDataCallback or streamCloseCallback)
        {
            std::unique_lock<std::mutex> dlock(drain_mtx_);
            auto gen_changed = [this, gen]() {
                return drain_gen_.load(std::memory_order_acquire) != gen;
            };
            if (timeout_ms < 0) {
                drain_cv_.wait(dlock, gen_changed);
            } else {
                drain_cv_.wait_until(dlock, deadline, gen_changed);
            }
        }

        // Re-check actual predicate under mtx_
        {
            std::lock_guard<std::recursive_mutex> lock(mtx_);
            auto it = streaming_body_data_.find(stream_id);
            if (it == streaming_body_data_.end()) {
                return -1;  // stream removed
            }
            if (it->second.data.size() - it->second.offset <= QUIC_MAX_STREAM_BODY) {
                return 0;  // buffer drained
            }
        }

        // Update generation for next wait iteration
        gen = drain_gen_.load(std::memory_order_acquire);

        // Check timeout
        if (timeout_ms > 0 && std::chrono::steady_clock::now() >= deadline) {
            return 1;  // timed out
        }
    }
}

// ===== Stream Management =====
// THREADING INVARIANT: getOrCreateStream() and markStreamComplete() must only be
// called while holding mtx_. All current call paths satisfy this:
//   - submitRequest()/submitResponse()/setupHttp3(): explicit lock_guard<recursive_mutex>
//   - nghttp3 callbacks (h3BeginHeaders, h3RecvHeader, h3EndHeaders, h3RecvData,
//     h3EndStream): called synchronously from ngtcp2_conn_read_pkt() within
//     readPacket() which acquires mtx_ at the top of the method

QuicStreamInfo* QuicSession::getOrCreateStream(int64_t stream_id) {
    auto it = streams_.find(stream_id);
    if (it != streams_.end()) {
        return it->second.get();
    }
    auto stream = std::make_unique<QuicStreamInfo>();
    stream->stream_id = stream_id;
    stream->state = QuicStreamState::Open;
    auto* ptr = stream.get();
    streams_[stream_id] = std::move(stream);
    return ptr;
}

void QuicSession::markStreamComplete(int64_t stream_id) {
    auto it = streams_.find(stream_id);
    if (it != streams_.end() && !it->second->body_complete) {
        it->second->state = QuicStreamState::Closed;
        it->second->body_complete = true;
        completed_streams_.push(stream_id);
        has_completed_streams_.store(true, std::memory_order_release);
    }
}

std::unique_ptr<QuicStreamInfo> QuicSession::takeCompletedStream() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    // Skip stream IDs that were cancelled (removed from streams_) after being
    // queued in completed_streams_.  cancelStream() erases from streams_ but
    // cannot efficiently remove from the middle of the queue.
    while (!completed_streams_.empty()) {
        int64_t stream_id = completed_streams_.front();
        completed_streams_.pop();
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            has_completed_streams_.store(!completed_streams_.empty(), std::memory_order_release);
            auto result = std::move(it->second);
            streams_.erase(it);
            return result;
        }
    }
    has_completed_streams_.store(false, std::memory_order_release);
    return nullptr;
}

bool QuicSession::hasCompletedStreams() const {
    return has_completed_streams_.load(std::memory_order_acquire);
}

bool QuicSession::isHandshakeComplete() const {
    // No lock needed: handshake_completed_ is std::atomic<bool>
    return handshake_completed_.load(std::memory_order_acquire);
}

bool QuicSession::isClosed() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!conn_) {
        return true;
    }
    return ngtcp2_conn_in_closing_period(conn_) || ngtcp2_conn_in_draining_period(conn_);
}

ssize_t QuicSession::writeConnectionClose(uint8_t* buf, size_t buflen) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!conn_ || ngtcp2_conn_in_draining_period(conn_)) {
        return 0;
    }

    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);
    ngtcp2_pkt_info pi{};
    ngtcp2_ccerr ccerr;
    ngtcp2_ccerr_default(&ccerr);

    ngtcp2_ssize nwrite = ngtcp2_conn_write_connection_close(
        conn_, &ps.path, &pi, buf, buflen, &ccerr, timestamp());
    return nwrite > 0 ? static_cast<ssize_t>(nwrite) : 0;
}

ngtcp2_ccerr QuicSession::getCloseError() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    ngtcp2_ccerr err;
    ngtcp2_ccerr_default(&err);
    if (conn_) {
        const ngtcp2_ccerr* ccerr = ngtcp2_conn_get_ccerr(conn_);
        if (ccerr) {
            err = *ccerr;
        }
    }
    return err;
}

int QuicSession::cancelStream(int64_t stream_id, uint64_t app_error_code, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!conn_) {
        xsink->raiseException("QUIC-ERROR", "cancelStream() called on closed QUIC connection");
        return -1;
    }

    // Notify HTTP/3 layer first (must happen before ngtcp2 shutdown)
    if (h3_conn_) {
        nghttp3_conn_shutdown_stream_write(h3_conn_, stream_id);
        int rv = nghttp3_conn_shutdown_stream_read(h3_conn_, stream_id);
        if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND) {
            xsink->raiseException("QUIC-HTTP3-ERROR",
                "nghttp3_conn_shutdown_stream_read failed: %s", nghttp3_strerror(rv));
            return -1;
        }
    }

    // Shutdown stream at QUIC transport layer (sends RESET_STREAM + STOP_SENDING)
    int rv = ngtcp2_conn_shutdown_stream(conn_, 0, stream_id, app_error_code);
    if (rv != 0 && rv != NGTCP2_ERR_STREAM_NOT_FOUND) {
        xsink->raiseException("QUIC-ERROR",
            "ngtcp2_conn_shutdown_stream failed: %s", ngtcp2_strerror(rv));
        return -1;
    }

    // Do NOT erase body_data_ here: nghttp3 may still hold nghttp3_vec pointers
    // into the body data buffer from h3ReadDataCallback().  Cleanup happens in
    // streamCloseCallback() after nghttp3_conn_close_stream() releases all
    // internal references.

    // Remove from tracking
    streams_.erase(stream_id);

    // Signal that packets need to be written (RESET_STREAM frame)
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

// ===== HTTP/3 GOAWAY (Graceful Shutdown) =====

int QuicSession::submitShutdownNotice(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }
    int rv = nghttp3_conn_submit_shutdown_notice(h3_conn_);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to submit shutdown notice: %s",
                              nghttp3_strerror(rv));
        return -1;
    }
    pending_write_.store(true, std::memory_order_release);
    return 0;
}

int QuicSession::submitShutdown(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }
    int rv = nghttp3_conn_shutdown(h3_conn_);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "failed to submit shutdown: %s",
                              nghttp3_strerror(rv));
        return -1;
    }
    // Note: goaway_sent_ is set eagerly before the GOAWAY frame is flushed
    // to the wire.  pending_write_ ensures the next I/O cycle writes the
    // queued nghttp3 data (including the GOAWAY frame) via writeStreams().
    goaway_sent_.store(true, std::memory_order_release);
    pending_write_.store(true, std::memory_order_release);
    return 0;
}

// ===== ngtcp2 Static Callbacks =====

ngtcp2_conn* QuicSession::getConnFromRef(ngtcp2_crypto_conn_ref* conn_ref) {
    auto* session = static_cast<QuicSession*>(conn_ref->user_data);
    return session->conn_;
}

void QuicSession::randCallback(uint8_t* dest, size_t destlen,
                               const ngtcp2_rand_ctx* /* rand_ctx */) {
    assert(destlen <= INT_MAX);
    if (RAND_bytes(dest, static_cast<int>(destlen)) != 1) {
        // Void return — cannot propagate errors.  Falling back to zeros would
        // produce predictable CIDs and tokens, so abort to avoid silent
        // security degradation.
        printd(0, "QuicSession::randCallback() FATAL: RAND_bytes failed for %zu bytes\n", destlen);
        abort();
    }
}

int QuicSession::getNewConnectionIdCallback(ngtcp2_conn* /* conn */,
                                            ngtcp2_cid* cid,
                                            uint8_t* token, size_t cidlen,
                                            void* user_data) {
    if (RAND_bytes(cid->data, static_cast<int>(cidlen)) != 1) {
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }
    cid->datalen = cidlen;

    if (RAND_bytes(token, NGTCP2_STATELESS_RESET_TOKENLEN) != 1) {
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    // Register new CID with dispatcher for packet routing.
    // THREADING: dispatcher_ access is safe here — this callback is invoked from
    // ngtcp2_conn_writev_stream() or ngtcp2_conn_read_pkt(), both of which hold mtx_.
    auto* session = static_cast<QuicSession*>(user_data);
    if (session->dispatcher_) {
        try {
            std::string cid_str(reinterpret_cast<const char*>(cid->data), cid->datalen);
            session->dispatcher_->registerConnectionId(cid_str, session);
        } catch (...) {
            // std::bad_alloc must not propagate through ngtcp2 C code
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
    }

    return 0;
}

int QuicSession::recvStreamDataCallback(ngtcp2_conn* conn, uint32_t flags,
                                         int64_t stream_id, uint64_t /* offset */,
                                         const uint8_t* data, size_t datalen,
                                         void* user_data, void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(user_data);
    bool fin = (flags & NGTCP2_STREAM_DATA_FLAG_FIN) != 0;

    // Forward to HTTP/3 layer if available
    if (session->h3_conn_) {
        nghttp3_ssize nconsumed = nghttp3_conn_read_stream2(
            session->h3_conn_, stream_id, data, datalen,
            fin ? 1 : 0, ngtcp2_conn_get_timestamp(conn));

        if (nconsumed < 0) {
            printd(1, "QuicSession::recvStreamDataCallback() nghttp3_conn_read_stream2() "
                "failed: %s (stream_id: " QLLD ")\n",
                nghttp3_strerror(static_cast<int>(nconsumed)), stream_id);
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }

        // Extend flow control for frame overhead bytes (headers, frame
        // framing, etc.).  DATA payload flow control is handled separately
        // in h3RecvDataCallback().
        ngtcp2_conn_extend_max_stream_offset(conn, stream_id,
                                              static_cast<uint64_t>(nconsumed));
        ngtcp2_conn_extend_max_offset(conn, static_cast<uint64_t>(nconsumed));
    } else {
        // HTTP/3 not yet initialized — buffer the data for replay in setupHttp3()
        // Check running total and entry count to prevent unbounded accumulation (O(1) per packet)
        if (session->pre_h3_buffer_size_ + datalen > QUIC_MAX_PRE_H3_BUFFER
            || session->pre_h3_buffer_.size() >= QUIC_MAX_PRE_H3_ENTRIES) {
            printd(0, "QuicSession: pre-HTTP/3 buffer overflow: %zu + %zu > %zu (entries: %zu)\n",
                session->pre_h3_buffer_size_, datalen, QUIC_MAX_PRE_H3_BUFFER,
                session->pre_h3_buffer_.size());
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
        try {
            session->pre_h3_buffer_.push_back({
                stream_id,
                std::vector<uint8_t>(data, data + datalen),
                fin
            });
            session->pre_h3_buffer_size_ += datalen;
        } catch (...) {
            // std::bad_alloc must not propagate through ngtcp2 C code
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
    }

    return 0;
}

int QuicSession::ackedStreamDataOffsetCallback(ngtcp2_conn* /* conn */,
                                                int64_t stream_id,
                                                uint64_t /* offset */,
                                                uint64_t datalen,
                                                void* user_data,
                                                void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(user_data);
    if (session->h3_conn_) {
        int rv = nghttp3_conn_add_ack_offset(session->h3_conn_, stream_id, datalen);
        if (rv != 0) {
            return NGTCP2_ERR_CALLBACK_FAILURE;
        }
    }
    return 0;
}

int QuicSession::streamCloseCallback(ngtcp2_conn* /* conn */, uint32_t flags,
                                      int64_t stream_id, uint64_t app_error_code,
                                      void* user_data, void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(user_data);

    try {
        if (session->h3_conn_) {
            if (!(flags & NGTCP2_STREAM_CLOSE_FLAG_APP_ERROR_CODE_SET)) {
                app_error_code = NGHTTP3_H3_NO_ERROR;
            }
            int rv = nghttp3_conn_close_stream(session->h3_conn_, stream_id, app_error_code);
            // STREAM_NOT_FOUND: stream was never registered with nghttp3
            // CLOSED_CRITICAL_STREAM: HTTP/3 critical uni streams (control, QPACK) cannot
            //   be closed; ngtcp2 may signal close for these during connection teardown
            if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND
                         && rv != NGHTTP3_ERR_H3_CLOSED_CRITICAL_STREAM) {
                return NGTCP2_ERR_CALLBACK_FAILURE;
            }
        }

        // Clean up body data
        session->body_data_.erase(stream_id);
        session->streaming_body_data_.erase(stream_id);

        // Wake any handler threads blocked in waitForStreamDrain() for this stream
        session->drain_gen_.fetch_add(1, std::memory_order_release);
        {
            std::lock_guard<std::mutex> dlock(session->drain_mtx_);
        }
        session->drain_cv_.notify_all();

        // Mark stream complete if it wasn't already (e.g. peer reset before
        // h3EndStreamCallback fired).  Without this, the stream would remain
        // in streams_ forever and callers waiting for it would hang.
        // NOTE: safe for uni-directional streams (control, QPACK) — markStreamComplete()
        // checks streams_.find() first, so unknown stream IDs are harmlessly skipped.
        session->markStreamComplete(stream_id);

        // Extend max remote bidi streams so the peer can open new ones
        if (session->is_server_) {
            ngtcp2_conn_extend_max_streams_bidi(session->conn_, 1);
            session->pending_write_.store(true, std::memory_order_release);
        }
    } catch (...) {
        // std::bad_alloc (e.g. from markStreamComplete → completed_streams_.push)
        // must not propagate through ngtcp2 C code
        return NGTCP2_ERR_CALLBACK_FAILURE;
    }

    return 0;
}

int QuicSession::handshakeCompletedCallback(ngtcp2_conn* /* conn */, void* user_data) {
    auto* session = static_cast<QuicSession*>(user_data);
    session->handshake_completed_.store(true, std::memory_order_release);
    return 0;
}

int QuicSession::extendMaxLocalStreamsBidiCallback(ngtcp2_conn* /* conn */,
                                                    uint64_t /* max_streams */,
                                                    void* /* user_data */) {
    // Client: can open more request streams
    return 0;
}

int QuicSession::extendMaxRemoteStreamsBidiCallback(ngtcp2_conn* /* conn */,
                                                     uint64_t /* max_streams */,
                                                     void* /* user_data */) {
    // Server: can accept more request streams from client
    return 0;
}

int QuicSession::extendMaxStreamDataCallback(ngtcp2_conn* /* conn */,
                                              int64_t stream_id,
                                              uint64_t max_data,
                                              void* user_data,
                                              void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(user_data);

    // Unblock the stream in HTTP/3 layer if it was blocked
    if (session->h3_conn_) {
        nghttp3_conn_unblock_stream(session->h3_conn_, stream_id);
    }

    // Signal that there may be more data to write now that the flow control
    // window has been extended
    session->pending_write_.store(true, std::memory_order_release);

    return 0;
}

int QuicSession::recvTxKeyCallback(ngtcp2_conn* /* conn */,
                                   ngtcp2_encryption_level /* level */,
                                   void* /* user_data */) {
    // HTTP/3 setup is deferred to the poll loop (QCS_SETUP_HTTP3 state)
    // after the handshake is fully complete and transport parameters are
    // exchanged, so that uni-stream limits are available.
    return 0;
}

// ===== nghttp3 Static Callbacks =====

int QuicSession::h3BeginHeadersCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                         void* conn_user_data, void* /* stream_user_data */) {
    try {
        auto* session = static_cast<QuicSession*>(conn_user_data);
        session->getOrCreateStream(stream_id);
    } catch (...) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

int QuicSession::h3RecvHeaderCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                       int32_t /* token */, nghttp3_rcbuf* name,
                                       nghttp3_rcbuf* value, uint8_t /* flags */,
                                       void* conn_user_data, void* /* stream_user_data */) {
    try {
        auto* session = static_cast<QuicSession*>(conn_user_data);
        auto* stream = session->getOrCreateStream(stream_id);

        nghttp3_vec name_vec = nghttp3_rcbuf_get_buf(name);
        nghttp3_vec value_vec = nghttp3_rcbuf_get_buf(value);

        std::string name_str(reinterpret_cast<const char*>(name_vec.base), name_vec.len);
        std::string value_str(reinterpret_cast<const char*>(value_vec.base), value_vec.len);

        // Handle pseudo-headers
        if (name_str == ":status") {
            char* end = nullptr;
            long val = strtol(value_str.c_str(), &end, 10);
            if (end == value_str.c_str() || *end != '\0' || val < 100 || val >= 600) {
                return NGHTTP3_ERR_MALFORMED_HTTP_HEADER;
            }
            stream->status_code = static_cast<int>(val);
        } else if (name_str == ":method") {
            stream->method = value_str;
        } else if (name_str == ":path") {
            stream->path = value_str;
        } else if (name_str == ":authority") {
            stream->authority = value_str;
        } else if (name_str == ":scheme") {
            stream->scheme = value_str;
        } else {
            stream->headers[name_str].push_back(value_str);
        }
    } catch (...) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }

    return 0;
}

int QuicSession::h3EndHeadersCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                       int fin, void* conn_user_data,
                                       void* /* stream_user_data */) {
    try {
        auto* session = static_cast<QuicSession*>(conn_user_data);
        auto* stream = session->getOrCreateStream(stream_id);
        stream->headers_complete = true;

        // Pre-allocate body buffer using content-length hint to avoid repeated
        // reallocation in h3RecvDataCallback
        if (!fin) {
            auto cl_it = stream->headers.find("content-length");
            if (cl_it != stream->headers.end() && !cl_it->second.empty()) {
                char* end = nullptr;
                long cl = strtol(cl_it->second[0].c_str(), &end, 10);
                if (cl > 0 && cl <= static_cast<long>(QUIC_MAX_STREAM_BODY)) {
                    stream->body.reserve(static_cast<size_t>(cl));
                }
            }
        }

        // If fin is set, the stream has no body — mark it complete now
        if (fin) {
            session->markStreamComplete(stream_id);
        }
    } catch (...) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

int QuicSession::h3RecvDataCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                     const uint8_t* data, size_t datalen,
                                     void* conn_user_data, void* /* stream_user_data */) {
    try {
        auto* session = static_cast<QuicSession*>(conn_user_data);
        auto* stream = session->getOrCreateStream(stream_id);
        // Guard against unbounded body accumulation — reset just this stream
        if (stream->body.size() + datalen > QUIC_MAX_STREAM_BODY) {
            printd(1, "h3RecvDataCallback: body too large (%zu + %zu > %zu) stream %lld\n",
                stream->body.size(), datalen, QUIC_MAX_STREAM_BODY, (long long)stream_id);
            stream->body.clear();
            stream->error_message = "response body exceeded maximum size ("
                + std::to_string(QUIC_MAX_STREAM_BODY) + " bytes)";
            // Notify nghttp3 before ngtcp2 so both layers stay in sync
            nghttp3_conn_shutdown_stream_read(session->h3_conn_, stream_id);
            // Reset just this stream, not the whole connection
            int rv = ngtcp2_conn_shutdown_stream_read(session->conn_, 0, stream_id,
                NGHTTP3_H3_REQUEST_CANCELLED);
            // Stream may already be closed during shutdown
            if (rv != 0 && rv != NGTCP2_ERR_STREAM_NOT_FOUND) {
                return NGHTTP3_ERR_CALLBACK_FAILURE;
            }
            // Extend offsets so the connection can continue
            ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                                  static_cast<uint64_t>(datalen));
            ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(datalen));
            // Mark stream complete with error so callers don't spin until timeout
            session->markStreamComplete(stream_id);
            return 0;
        }
        stream->body.insert(stream->body.end(), data, data + datalen);

        // Extend flow control for DATA payload bytes.
        // nghttp3_conn_read_stream2() intentionally excludes DATA payload from its
        // return value (see nghttp3 docs: "It does not include the amount of data
        // carried by DATA frame which contains application data").  The recv_data
        // callback is responsible for extending flow control by datalen.
        ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                              static_cast<uint64_t>(datalen));
        ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(datalen));
    } catch (...) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }

    return 0;
}

int QuicSession::h3EndStreamCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                      void* conn_user_data, void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    session->markStreamComplete(stream_id);
    return 0;
}

int QuicSession::h3DeferredConsumeCallback(nghttp3_conn* conn, int64_t stream_id,
                                            size_t consumed, void* conn_user_data,
                                            void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                          static_cast<uint64_t>(consumed));
    ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(consumed));
    return 0;
}

int QuicSession::h3StopSendingCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                        uint64_t app_error_code,
                                        void* conn_user_data,
                                        void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    int rv = ngtcp2_conn_shutdown_stream_read(session->conn_, 0, stream_id, app_error_code);
    // During shutdown, ngtcp2 may have already closed the stream
    if (rv != 0 && rv != NGTCP2_ERR_STREAM_NOT_FOUND) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

int QuicSession::h3ResetStreamCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                        uint64_t app_error_code,
                                        void* conn_user_data,
                                        void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    int rv = ngtcp2_conn_shutdown_stream_write(session->conn_, 0, stream_id, app_error_code);
    // During shutdown, ngtcp2 may have already closed the stream
    if (rv != 0 && rv != NGTCP2_ERR_STREAM_NOT_FOUND) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
    return 0;
}

nghttp3_ssize QuicSession::h3ReadDataCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                               nghttp3_vec* vec, size_t veccnt,
                                               uint32_t* pflags,
                                               void* conn_user_data,
                                               void* /* stream_user_data */) {
    auto* session = static_cast<QuicSession*>(conn_user_data);

    // Check non-streaming body data first
    auto it = session->body_data_.find(stream_id);
    if (it != session->body_data_.end()) {
        if (it->second.offset >= it->second.data.size()) {
            *pflags = NGHTTP3_DATA_FLAG_EOF;
            return 0;
        }

        auto& bd = it->second;
        size_t remaining = bd.data.size() - bd.offset;

        if (veccnt > 0) {
            vec[0].base = const_cast<uint8_t*>(bd.data.data() + bd.offset);
            vec[0].len = remaining;
            bd.offset = bd.data.size();  // all data consumed

            *pflags = NGHTTP3_DATA_FLAG_EOF;
            return 1;
        }

        // veccnt == 0: no vector slots available; signal "call again" without EOF
        *pflags = 0;
        return 0;
    }

    // Check streaming body data
    auto sit = session->streaming_body_data_.find(stream_id);
    if (sit != session->streaming_body_data_.end()) {
        auto& sbd = sit->second;
        size_t remaining = sbd.data.size() - sbd.offset;

        if (remaining > 0) {
            if (veccnt > 0) {
                // Data available: return vec pointing into buffer
                vec[0].base = const_cast<uint8_t*>(sbd.data.data() + sbd.offset);
                vec[0].len = remaining;
                sbd.offset = sbd.data.size();  // mark all data as consumed

                // Notify waitForStreamDrain() that buffer space freed up
                session->drain_gen_.fetch_add(1, std::memory_order_release);
                {
                    std::lock_guard<std::mutex> dlock(session->drain_mtx_);
                }
                session->drain_cv_.notify_all();

                if (sbd.eof) {
                    *pflags = NGHTTP3_DATA_FLAG_EOF;
                } else {
                    *pflags = 0;
                }
                return 1;
            }
            // veccnt == 0: data available but no vec slots; signal "call again"
            *pflags = 0;
            return 0;
        }

        if (sbd.eof) {
            // No data and EOF: signal end of stream
            *pflags = NGHTTP3_DATA_FLAG_EOF;
            return 0;
        }

        // No data available and not EOF: defer (WOULDBLOCK)
        sbd.deferred = true;
        return NGHTTP3_ERR_WOULDBLOCK;
    }

    // Stream not found in either map — signal EOF
    *pflags = NGHTTP3_DATA_FLAG_EOF;
    return 0;
}

int QuicSession::h3AckedStreamDataCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                            uint64_t /* datalen */, void* /* conn_user_data */,
                                            void* /* stream_user_data */) {
    // Do NOT erase body_data_ here: nghttp3 may still hold a cached vec pointing
    // to the body data buffer.  Cleanup happens in streamCloseCallback after
    // nghttp3_conn_close_stream releases all internal references.
    return 0;
}

int QuicSession::h3ShutdownCallback(nghttp3_conn* /* conn */, int64_t id,
                                     void* conn_user_data) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    // Invariant: mtx_ is already held by the calling thread.
    // Call chain: readPacket() [acquires mtx_] -> ngtcp2_conn_read_pkt()
    //   -> nghttp3_conn_read_stream() -> h3ShutdownCallback()
    // goaway_max_stream_id_ is protected by mtx_ (not atomic), so this
    // write is safe only because the caller holds the lock.
    session->goaway_received_.store(true, std::memory_order_release);
    session->goaway_max_stream_id_ = id;
    return 0;
}
