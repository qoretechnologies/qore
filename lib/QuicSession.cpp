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
#include "qore/intern/QuicSessionTicketCache.h"
#include "qore/intern/qore_socket_private.h"

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

// ===== Shared Server SSL_CTX =====

SSL_CTX* qore_socket_private::getOrCreateQuicServerSslCtx(QoreSSLCertificate* cert,
                                                            QoreSSLPrivateKey* pk,
                                                            ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lock(quic_server_ssl_ctx_lock_);

    // Check if already created (under lock)
    if (quic_server_ssl_ctx_) {
        return quic_server_ssl_ctx_;
    }

    assert(cert);
    assert(pk);

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        xsink->raiseException("QUIC-SSL-ERROR", "failed to create shared server SSL_CTX");
        return nullptr;
    }

    // Set minimum TLS version to 1.3 (required for QUIC)
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // Load the X.509 certificate for the TLS 1.3 handshake
    if (SSL_CTX_use_certificate(ctx, cert->getData()) != 1) {
        SSL_CTX_free(ctx);
        xsink->raiseException("QUIC-SSL-ERROR", "failed to load server certificate into shared SSL_CTX");
        return nullptr;
    }

    // Load the private key for the TLS 1.3 handshake
    EVP_PKEY* pk_data = pk->getData();
    if (!pk_data) {
        SSL_CTX_free(ctx);
        xsink->raiseException("QUIC-SSL-ERROR", "server private key getData() returned null");
        return nullptr;
    }
    if (SSL_CTX_use_PrivateKey(ctx, pk_data) != 1) {
        unsigned long ssl_err = ERR_peek_last_error();
        char errbuf[256];
        ERR_error_string_n(ssl_err, errbuf, sizeof(errbuf));
        SSL_CTX_free(ctx);
        xsink->raiseException("QUIC-SSL-ERROR",
            "failed to load server private key into shared SSL_CTX: %s (EVP_PKEY type=%d)",
            errbuf, EVP_PKEY_base_id(pk_data));
        return nullptr;
    }

    // Verify cert/key match
    if (SSL_CTX_check_private_key(ctx) != 1) {
        SSL_CTX_free(ctx);
        xsink->raiseException("QUIC-SSL-ERROR", "server certificate and private key do not match");
        return nullptr;
    }

    // Set ALPN selection callback for the server
    SSL_CTX_set_alpn_select_cb(ctx,
        [](SSL*, const unsigned char** out, unsigned char* outlen,
           const unsigned char* in, unsigned int inlen, void*) -> int {
            if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                    H3_ALPN, H3_ALPN_LEN, in, inlen) != OPENSSL_NPN_NEGOTIATED) {
                return SSL_TLSEXT_ERR_NOACK;
            }
            return SSL_TLSEXT_ERR_OK;
        }, nullptr);

    // Enable 0-RTT (early data) for QUIC: RFC 9001 §4.6.1 requires exactly 0xffffffff
    SSL_CTX_set_max_early_data(ctx, 0xffffffff);

    // Issue 2 session tickets per connection to allow concurrent 0-RTT attempts
    SSL_CTX_set_num_tickets(ctx, 2);

    quic_server_ssl_ctx_ = ctx;

    printd(3, "qore_socket_private::getOrCreateQuicServerSslCtx(): created shared server SSL_CTX %p "
        "(max_early_data=0xffffffff, num_tickets=2)\n", ctx);

    return ctx;
}

// ===== Peer Certificate =====

X509* QuicSession::getPeerCertificate() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!ssl_) {
        return nullptr;
    }
    return SSL_get_peer_certificate(ssl_);
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
        // Always free: owned contexts have refcount=1, shared contexts were
        // explicitly up-ref'd in setupServerSslCtx() so this drops our reference
        SSL_CTX_free(ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    // Release any remaining Queue references
    if (!connect_stream_queues_.empty()) {
        ExceptionSink xsink;
        for (auto& [id, q] : connect_stream_queues_) {
            q->deref(&xsink);
        }
        connect_stream_queues_.clear();
    }
}

// ===== Factory Methods =====

std::shared_ptr<QuicSession> QuicSession::createClient(
    qore_socket_private* sock, ExceptionSink* xsink,
    const char* host, uint16_t port,
    const struct sockaddr* local_addr, socklen_t local_addrlen,
    const struct sockaddr* remote_addr, socklen_t remote_addrlen,
    int ssl_verify_mode, bool enable_0rtt,
    QoreSSLCertificate* client_cert, QoreSSLPrivateKey* client_pk) {
    assert(local_addr);
    assert(remote_addr);

    ensureOsslInit();

    auto session = std::shared_ptr<QuicSession>(new QuicSession());
    if (session->initClient(sock, xsink, host, port,
                            local_addr, local_addrlen,
                            remote_addr, remote_addrlen,
                            ssl_verify_mode, enable_0rtt,
                            client_cert, client_pk) != 0) {
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
    QoreDatagramDispatcher* dispatcher,
    SSL_CTX* shared_ssl_ctx,
    int ssl_verify_mode,
    bool ssl_accept_all_certs) {
    assert(cert);
    assert(pk);
    assert(local_addr);
    assert(remote_addr);

    ensureOsslInit();

    auto session = std::shared_ptr<QuicSession>(new QuicSession());
    if (session->initServer(sock, xsink, initial_hdr, cert, pk,
                            local_addr, local_addrlen,
                            remote_addr, remote_addrlen,
                            dispatcher, shared_ssl_ctx,
                            ssl_verify_mode, ssl_accept_all_certs) != 0) {
        return nullptr;
    }
    // Clear the temporary socket pointer; it was only needed during init
    session->clearSockPtr();
    return session;
}

// ===== SSL Context Setup =====

int QuicSession::setupClientSslCtx(const char* host, int ssl_verify_mode, ExceptionSink* xsink,
                                    QoreSSLCertificate* client_cert, QoreSSLPrivateKey* client_pk) {
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

    // Load client certificate and private key for mTLS (mutual TLS)
    if (client_cert && client_pk) {
        if (SSL_CTX_use_certificate(ssl_ctx_, client_cert->getData()) != 1) {
            xsink->raiseException("QUIC-SSL-ERROR", "failed to load client certificate into SSL_CTX");
            return -1;
        }
        EVP_PKEY* pk_data = client_pk->getData();
        if (!pk_data) {
            xsink->raiseException("QUIC-SSL-ERROR", "client private key getData() returned null");
            return -1;
        }
        if (SSL_CTX_use_PrivateKey(ssl_ctx_, pk_data) != 1) {
            xsink->raiseException("QUIC-SSL-ERROR", "failed to load client private key into SSL_CTX");
            return -1;
        }
        if (SSL_CTX_check_private_key(ssl_ctx_) != 1) {
            xsink->raiseException("QUIC-SSL-ERROR", "client certificate and private key do not match");
            return -1;
        }
        printd(3, "QuicSession::setupClientSslCtx(): client certificate loaded for mTLS\n");
    }

    // Enable session caching for 0-RTT ticket capture
    // NO_INTERNAL_STORE: we manage our own cache (QuicSessionTicketCache)
    SSL_CTX_set_session_cache_mode(ssl_ctx_,
        SSL_SESS_CACHE_CLIENT | SSL_SESS_CACHE_NO_INTERNAL_STORE);
    SSL_CTX_sess_set_new_cb(ssl_ctx_, newSessionTicketCallback);

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

// Permissive verify callback for mTLS with ssl_accept_all_certs=true.
// Accepts all client certificates (including self-signed, expired, etc.)
// while still extracting the certificate for inspection.
static int quic_server_accept_all_verify_cb(int preverify_ok, X509_STORE_CTX* ctx) {
    return 1;
}

int QuicSession::setupServerSslCtx(QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                                    ExceptionSink* xsink, SSL_CTX* shared_ssl_ctx,
                                    int ssl_verify_mode,
                                    bool ssl_accept_all_certs) {
    // When mTLS is requested, we must create a per-session SSL_CTX with
    // SSL_CTX_set_verify() — the shared context cannot be used because it
    // was created without client certificate verification. This means 0-RTT
    // session ticket continuity across sessions is not available for mTLS
    // listeners, which is an acceptable trade-off.
    if (shared_ssl_ctx && ssl_verify_mode == SSL_VERIFY_NONE) {
        // Use the shared SSL_CTX (for session ticket key continuity / 0-RTT).
        // Take an explicit reference so the destructor can always call SSL_CTX_free()
        // unconditionally. Without this, if setupServerSslCtx() fails between storing
        // ssl_ctx_ and SSL_new() (which internally up-refs), the ref would be unbalanced.
        SSL_CTX_up_ref(shared_ssl_ctx);
        ssl_ctx_ = shared_ssl_ctx;
        // The shared context already has cert, key, ALPN, TLS 1.3, early data,
        // and ticket settings configured by getOrCreateQuicServerSslCtx()
    } else {
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

        // Enable client certificate verification if requested (mTLS)
        if (ssl_verify_mode != SSL_VERIFY_NONE) {
            if (ssl_accept_all_certs) {
                // Accept all client certificates (including self-signed) but still
                // extract the cert for inspection via getPeerCertificate()
                SSL_CTX_set_verify(ssl_ctx_, ssl_verify_mode, quic_server_accept_all_verify_cb);
                printd(3, "QuicSession::setupServerSslCtx(): mTLS enabled "
                    "(verify_mode=0x%x, accept_all_certs=true)\n", ssl_verify_mode);
            } else {
                SSL_CTX_set_verify(ssl_ctx_, ssl_verify_mode, nullptr);
                // Load system default CA certificates for client cert validation
                SSL_CTX_set_default_verify_paths(ssl_ctx_);
                printd(3, "QuicSession::setupServerSslCtx(): mTLS enabled "
                    "(verify_mode=0x%x, accept_all_certs=false)\n", ssl_verify_mode);
            }
        }
    }

    // Create SSL connection from the context
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
                            int ssl_verify_mode, bool enable_0rtt,
                            QoreSSLCertificate* client_cert, QoreSSLPrivateKey* client_pk) {
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

    // Set up SSL with certificate verification and optional client cert for mTLS
    if (setupClientSslCtx(host, ssl_verify_mode, xsink, client_cert, client_pk) != 0) {
        return -1;
    }

    // Set up conn_ref for TLS integration
    conn_ref_.get_conn = getConnFromRef;
    conn_ref_.user_data = this;
    SSL_set_app_data(ssl_, &conn_ref_);

    // Attempt 0-RTT: restore a cached session ticket if available
    std::vector<uint8_t> cached_tp;
    if (enable_0rtt) {
        SSL_SESSION* cached_session = nullptr;
        std::string origin = std::string(host) + ":" + std::to_string(port);
        if (QuicSessionTicketCache::instance().lookup(origin, &cached_session, cached_tp)) {
            SSL_set_session(ssl_, cached_session);
            SSL_SESSION_free(cached_session);  // SSL_set_session up-refs it
            attempting_0rtt_.store(true, std::memory_order_release);
            printd(3, "QuicSession::initClient(): restoring 0-RTT session for '%s' "
                "(tp_size=%zu)\n", origin.c_str(), cached_tp.size());
        }
    }

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

    // Register 0-RTT rejection callback when attempting early data
    if (attempting_0rtt_) {
        callbacks.tls_early_data_rejected = earlyDataRejectedCallback;
    }

    // Connection migration callbacks (RFC 9000 §9)
    callbacks.path_validation = pathValidationCallback;
    callbacks.begin_path_validation = beginPathValidationCallback;
    callbacks.remove_connection_id = removeConnectionIdCallback;
    callbacks.dcid_status = dcidStatusCallback;

    // RFC 9221: QUIC datagram callback
    callbacks.recv_datagram = recvDatagramCallback;

    // Settings
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = timestamp();

    // Transport parameters
    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_streams_uni = QUIC_INITIAL_MAX_STREAMS_UNI;
    params.initial_max_streams_bidi = QUIC_INITIAL_MAX_STREAMS_BIDI;
    params.initial_max_stream_data_bidi_local = QUIC_CLIENT_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_bidi_remote = QUIC_CLIENT_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_uni = QUIC_CLIENT_INITIAL_MAX_STREAM_DATA;
    params.initial_max_data = QUIC_CLIENT_INITIAL_MAX_DATA;
    // Idle timeout: ngtcp2 default is 0 (no timeout), which prevents cleanup of
    // lost connections and enables resource exhaustion.  30s is standard for clients.
    params.max_idle_timeout = QUIC_IDLE_TIMEOUT_NS;
    params.active_connection_id_limit = QUIC_ACTIVE_CONNECTION_ID_LIMIT;
    // RFC 9221: Advertise willingness to receive QUIC DATAGRAM frames
    params.max_datagram_frame_size = QUIC_MAX_DATAGRAM_FRAME_SIZE;

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

    // Decode and set 0-RTT transport params after connection is created
    if (attempting_0rtt_ && !cached_tp.empty()) {
        rv = ngtcp2_conn_decode_and_set_0rtt_transport_params(conn_, cached_tp.data(), cached_tp.size());
        if (rv != 0) {
            printd(2, "QuicSession::initClient(): 0-RTT transport params decode failed: %s\n",
                ngtcp2_strerror(rv));
            attempting_0rtt_.store(false, std::memory_order_release);
        }
    }

    return 0;
}

int QuicSession::initServer(qore_socket_private* sock, ExceptionSink* xsink,
                            const ngtcp2_pkt_hd* initial_hdr,
                            QoreSSLCertificate* cert, QoreSSLPrivateKey* pk,
                            const struct sockaddr* local_addr, socklen_t local_addrlen,
                            const struct sockaddr* remote_addr, socklen_t remote_addrlen,
                            QoreDatagramDispatcher* dispatcher,
                            SSL_CTX* shared_ssl_ctx,
                            int ssl_verify_mode,
                            bool ssl_accept_all_certs) {
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

    // Set up SSL with cert/key for TLS 1.3 (use shared SSL_CTX if provided)
    // When mTLS is requested (ssl_verify_mode != SSL_VERIFY_NONE), the shared context
    // is bypassed and a per-session context with client certificate verification is created
    if (setupServerSslCtx(cert, pk, xsink, shared_ssl_ctx, ssl_verify_mode, ssl_accept_all_certs) != 0) {
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

    // Connection migration callbacks (RFC 9000 §9)
    callbacks.path_validation = pathValidationCallback;
    callbacks.begin_path_validation = beginPathValidationCallback;
    callbacks.remove_connection_id = removeConnectionIdCallback;
    callbacks.dcid_status = dcidStatusCallback;

    // RFC 9221: QUIC datagram callback
    callbacks.recv_datagram = recvDatagramCallback;

    // Settings
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = timestamp();

    // Transport parameters
    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_streams_uni = QUIC_INITIAL_MAX_STREAMS_UNI;
    params.initial_max_streams_bidi = QUIC_INITIAL_MAX_STREAMS_BIDI;
    params.initial_max_stream_data_bidi_local = QUIC_SERVER_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_bidi_remote = QUIC_SERVER_INITIAL_MAX_STREAM_DATA;
    params.initial_max_stream_data_uni = QUIC_SERVER_INITIAL_MAX_STREAM_DATA;
    params.initial_max_data = QUIC_SERVER_INITIAL_MAX_DATA;
    // Idle timeout: ngtcp2 default is 0 (no timeout), which allows malicious clients
    // to hold server sessions open indefinitely.  30s matches the client setting.
    params.max_idle_timeout = QUIC_IDLE_TIMEOUT_NS;
    params.active_connection_id_limit = QUIC_ACTIVE_CONNECTION_ID_LIMIT;
    // RFC 9221: Advertise willingness to receive QUIC DATAGRAM frames
    params.max_datagram_frame_size = QUIC_MAX_DATAGRAM_FRAME_SIZE;
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

int QuicSession::resetHttp3(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (h3_conn_) {
        nghttp3_conn_del(h3_conn_);
        h3_conn_ = nullptr;
    }
    // Clear all stream state from the rejected 0-RTT attempt: streams opened during
    // 0-RTT (including internal uni-streams created by nghttp3) were discarded by ngtcp2
    streams_.clear();
    body_data_.clear();
    streaming_body_data_.clear();
    // Clear any pre-H3 buffered data and completed streams from the 0-RTT attempt
    pre_h3_buffer_.clear();
    pre_h3_buffer_size_ = 0;
    while (!completed_streams_.empty()) {
        completed_streams_.pop();
    }
    has_completed_streams_.store(false, std::memory_order_release);

    printd(2, "QuicSession::resetHttp3(): HTTP/3 layer torn down for re-initialization "
        "(session %lld)\n", session_id_);

    return setupHttp3(xsink);
}

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
    // RFC 9220: Detect remote peer's enable_connect_protocol setting
#if NGHTTP3_VERSION_NUM >= 0x010e00  // v1.14.0+
    h3_cbs.recv_settings2 = h3RecvSettings2Callback;
#else
    h3_cbs.recv_settings = h3RecvSettingsCallback;
#endif

    nghttp3_settings h3_settings;
    nghttp3_settings_default(&h3_settings);
    // Disable QPACK dynamic table — eliminates the need for deferred_consume
    h3_settings.qpack_max_dtable_capacity = 0;
    h3_settings.qpack_blocked_streams = 0;
    // RFC 9220: Server advertises Extended CONNECT support for WebSocket over HTTP/3
    if (is_server_) {
        h3_settings.enable_connect_protocol = 1;
    }
    // RFC 9297: Enable HTTP/3 datagrams (SETTINGS_H3_DATAGRAM)
    h3_settings.h3_datagram = 1;

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

    // Tell nghttp3 how many bidirectional streams the client is allowed to open.
    // This must match the initial_max_streams_bidi we advertised in our QUIC transport
    // params.  Without this call conn->remote.bidi.max_client_streams stays at 0,
    // causing NGHTTP3_ERR_H3_ID_ERROR for every PRIORITY_UPDATE frame the client sends,
    // which in turn corrupts the read-state machine and triggers an assertion on the
    // next packet.  Only the server side needs this — on the client we are the one
    // sending PRIORITY_UPDATE frames.
    if (is_server_) {
        nghttp3_conn_set_max_client_streams_bidi(h3_conn_, QUIC_INITIAL_MAX_STREAMS_BIDI);
    }

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
    size_t total_bytes = 0;

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

        // Burst cap: limit per-call output to prevent overflowing the peer's
        // UDP receive buffer.  On Linux, SO_RCVBUF is capped by
        // net.core.rmem_max (default ~208KB); sending a burst larger than this
        // causes silent packet loss on localhost, forcing PTO-based recovery
        // that can take 30+ seconds on slow platforms (QEMU, constrained CI).
        // 128KB leaves headroom for in-flight data already in the buffer.
        // Remaining data is sent in the next continuePoll() cycle — since
        // packets are now in-flight, ngtcp2 sets a PTO timer that bounds the
        // poll() wait, guaranteeing forward progress.
        if (total_bytes >= QUIC_MAX_WRITE_BURST_BYTES) {
            break;
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
            if (sveccnt > 0 || stream_id >= 0) {
                size_t vec_total = 0;
                for (nghttp3_ssize i = 0; i < sveccnt; ++i) {
                    vec_total += vec[i].len;
                }
                printd(5, "writePacketsLocked() nghttp3_writev: stream_id=" QLLD " fin=%d sveccnt=%d vec_total=%d\n",
                    stream_id, fin, (int)sveccnt, (int)vec_total);
            }
        } else if (h3_conn_) {
            printd(5, "writePacketsLocked() max_data_left=0, skipping nghttp3_conn_writev_stream\n");
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
                printd(5, "writePacketsLocked() STREAM_DATA_BLOCKED stream_id=" QLLD "\n", stream_id);
                if (h3_conn_) {
                    nghttp3_conn_block_stream(h3_conn_, stream_id);
                }
                continue;
            case NGTCP2_ERR_STREAM_SHUT_WR:
                assert(ndatalen == -1);
                printd(5, "writePacketsLocked() STREAM_SHUT_WR stream_id=" QLLD "\n", stream_id);
                if (h3_conn_) {
                    nghttp3_conn_shutdown_stream_write(h3_conn_, stream_id);
                }
                continue;
            case NGTCP2_ERR_WRITE_MORE:
                assert(ndatalen >= 0);
                printd(5, "writePacketsLocked() WRITE_MORE stream_id=" QLLD " ndatalen=%d\n",
                    stream_id, (int)ndatalen);
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
            // No stream data to write — try sending queued datagrams (RFC 9221)
            if (!pending_datagrams_.empty()) {
                auto& [dgram_id, dgram_data] = pending_datagrams_.front();
                ngtcp2_vec datav;
                datav.base = dgram_data.data();
                datav.len = dgram_data.size();
                int accepted = 0;
                ngtcp2_ssize dg_nwrite = ngtcp2_conn_writev_datagram(
                    conn_, &ps.path, &pi, pkt_buf_, sizeof(pkt_buf_),
                    &accepted, NGTCP2_WRITE_DATAGRAM_FLAG_NONE,
                    dgram_id, &datav, 1, ts);
                if (dg_nwrite < 0) {
                    if (dg_nwrite == NGTCP2_ERR_CLOSING || dg_nwrite == NGTCP2_ERR_DRAINING) {
                        pending_write_.store(false, std::memory_order_release);
                        return total_packets;
                    }
                    // Non-fatal datagram write error — skip this datagram
                    pending_datagrams_.pop_front();
                    continue;
                }
                if (accepted) {
                    pending_datagrams_.pop_front();
                }
                if (dg_nwrite > 0) {
                    packets.addPacket(pkt_buf_, static_cast<size_t>(dg_nwrite));
                    ++total_packets;
                    continue;
                }
            }

            printd(5, "writePacketsLocked() nwrite=0 stream_id=" QLLD " total_packets=%d\n",
                stream_id, total_packets);
            // Only clear pending_write_ when there is genuinely nothing more
            // to write.  When stream_id >= 0, nghttp3 has queued stream data
            // but ngtcp2 cannot write it right now (congestion or flow control);
            // keep pending_write_ set so the next read cycle (ACKs that open
            // the congestion window) triggers another write attempt.
            // When stream_id < 0, no stream data is pending — clear the flag
            // to avoid busy-looping on POLLOUT.
            if (stream_id < 0 && pending_datagrams_.empty()) {
                pending_write_.store(false, std::memory_order_release);
            }
            break;
        }

        // Each ngtcp2_conn_writev_stream result is a separate QUIC packet
        // that must be sent as its own UDP datagram
        packets.addPacket(pkt_buf_, static_cast<size_t>(nwrite));
        ++total_packets;
        total_bytes += static_cast<size_t>(nwrite);
        printd(5, "writePacketsLocked() packet #%d: %d bytes, stream_id=" QLLD " ndatalen=%d\n",
            total_packets, (int)nwrite, stream_id, (int)ndatalen);

        // Update stored addresses from ngtcp2's output path.  During migration,
        // ngtcp2 updates the active path and outputs packets for the new address;
        // we must track this so the caller sends to the correct destination.
        // This runs under mtx_ (caller holds the lock).
        //
        // Performance: the memcmp per packet is negligible — addresses are typically
        // 16 bytes (IPv4) or 28 bytes (IPv6), memcmp short-circuits on the first
        // differing byte (matching addresses are the common case), and this code
        // already runs under the session mutex.  This is the only place that catches
        // implicit path updates from ngtcp2 (e.g. server-side passive migration),
        // so it cannot be moved to a less frequent path.
        if (ps.path.remote.addrlen > 0 && ps.path.remote.addrlen <= sizeof(remote_addr_)) {
            if (memcmp(&remote_addr_, ps.path.remote.addr, ps.path.remote.addrlen) != 0) {
                memcpy(&remote_addr_, ps.path.remote.addr, ps.path.remote.addrlen);
                remote_addrlen_ = ps.path.remote.addrlen;
                path_migrated_.store(true, std::memory_order_release);
                migration_gen_.fetch_add(1, std::memory_order_release);
                printd(2, "QuicSession::writePacketsLocked(): remote address updated from "
                    "ngtcp2 output path (session %lld)\n", (long long)session_id_);
            }
        }
        if (ps.path.local.addrlen > 0 && ps.path.local.addrlen <= sizeof(local_addr_)) {
            if (memcmp(&local_addr_, ps.path.local.addr, ps.path.local.addrlen) != 0) {
                memcpy(&local_addr_, ps.path.local.addr, ps.path.local.addrlen);
                local_addrlen_ = ps.path.local.addrlen;
            }
        }
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

int64_t QuicSession::submitRequestStreaming(const char* method, const char* path,
                                            const strcase_str_map_t& headers,
                                            ExceptionSink* xsink) {
    // Build header name-value pairs OUTSIDE the lock (same pattern as submitRequest)
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

    // RFC 9220: if :protocol is in headers, include it as a pseudo-header
    // (extended CONNECT for WebSocket, A2A, etc.)
    auto proto_it = headers.find(":protocol");
    if (proto_it != headers.end() && !proto_it->second.empty()) {
        add_nv(":protocol", 9, proto_it->second.c_str(), proto_it->second.size());
    }

    // Regular headers (same filtering as submitRequest)
    std::vector<std::string> lower_keys;
    lower_keys.reserve(headers.size());
    for (const auto& h : headers) {
        if (h.first.empty() || h.first[0] == ':' || strcasecmp(h.first.c_str(), "host") == 0) {
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

    // Always set up deferred data reader (body data will arrive via sendStreamData)
    nghttp3_data_reader dr;
    dr.read_data = h3ReadDataCallback;

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

    // Set up empty streaming body data with deferred=true — the h3ReadDataCallback
    // will return NGHTTP3_ERR_WOULDBLOCK until data is provided via sendStreamData()
    streaming_body_data_[stream_id] = QuicStreamingBodyData{};
    streaming_body_data_[stream_id].deferred = true;

    rv = nghttp3_conn_submit_request(h3_conn_, stream_id, nva.data(), nva.size(),
                                     &dr, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "nghttp3_conn_submit_request failed: %s",
                              nghttp3_strerror(rv));
        streaming_body_data_.erase(stream_id);
        ngtcp2_conn_shutdown_stream_write(conn_, 0, stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        return -1;
    }

    // Create stream info
    auto* stream = getOrCreateStream(stream_id);
    stream->method = method;
    stream->path = path;
    stream->state = QuicStreamState::Open;

    // RFC 9220: mark extended CONNECT streams as connect tunnels
    if (proto_it != headers.end() && !proto_it->second.empty()
            && strcmp(method, "CONNECT") == 0) {
        stream->is_connect = true;
        stream->connect_protocol = proto_it->second;
        stream->connect_tunnel_active = true;
    }

    // Signal that there's data to write (headers)
    pending_write_.store(true, std::memory_order_release);

    return stream_id;
}

int64_t QuicSession::submitConnectRequest(const char* path, const strcase_str_map_t& headers,
                                           const char* protocol, ExceptionSink* xsink) {
    // Build header name-value pairs OUTSIDE the lock
    // Reserve: 5 pseudo-headers (:method, :path, :scheme, :authority, :protocol) + user headers
    std::vector<nghttp3_nv> nva;
    nva.reserve(headers.size() + 5);
    auto add_nv = [&nva](const char* name, size_t namelen,
                          const char* value, size_t valuelen) {
        nva.push_back({
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(name)),
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(value)),
            namelen, valuelen, NGHTTP3_NV_FLAG_NONE
        });
    };

    // Pseudo-headers — RFC 9220 Section 3: extended CONNECT includes all 5
    add_nv(":method", 7, "CONNECT", 7);
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

    // RFC 9220: :protocol pseudo-header for extended CONNECT
    add_nv(":protocol", 9, protocol, strlen(protocol));

    // Regular headers (same filtering as submitRequest)
    std::vector<std::string> lower_keys;
    lower_keys.reserve(headers.size());
    for (const auto& h : headers) {
        if (h.first.empty() || h.first[0] == ':' || strcasecmp(h.first.c_str(), "host") == 0) {
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

    // Deferred data reader — CONNECT tunnel keeps the stream open indefinitely
    nghttp3_data_reader dr;
    dr.read_data = h3ReadDataCallback;

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

    // Set up deferred streaming body data — h3ReadDataCallback returns
    // NGHTTP3_ERR_WOULDBLOCK until data is pushed via sendStreamData()
    streaming_body_data_[stream_id] = QuicStreamingBodyData{};
    streaming_body_data_[stream_id].deferred = true;

    rv = nghttp3_conn_submit_request(h3_conn_, stream_id, nva.data(), nva.size(),
                                     &dr, nullptr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "nghttp3_conn_submit_request failed for CONNECT: %s",
            nghttp3_strerror(rv));
        streaming_body_data_.erase(stream_id);
        ngtcp2_conn_shutdown_stream_write(conn_, 0, stream_id, NGHTTP3_H3_INTERNAL_ERROR);
        return -1;
    }

    // Create stream info — mark as CONNECT so h3EndHeadersCallback and
    // h3RecvDataCallback handle the bidirectional tunnel correctly
    auto* stream = getOrCreateStream(stream_id);
    stream->method = "CONNECT";
    stream->path = path;
    stream->connect_protocol = protocol;
    stream->is_connect = true;
    stream->connect_tunnel_active = true;
    stream->state = QuicStreamState::Open;

    pending_write_.store(true, std::memory_order_release);

    return stream_id;
}

int QuicSession::submitTrailers(int64_t stream_id, const strcase_str_map_t& trailers,
                                ExceptionSink* xsink) {
    // Build trailer name-value pairs OUTSIDE the lock
    std::vector<nghttp3_nv> nva;
    nva.reserve(trailers.size());
    std::vector<std::string> lower_keys;
    lower_keys.reserve(trailers.size());
    for (const auto& t : trailers) {
        if (t.first.empty() || t.first[0] == ':') {
            continue;
        }
        std::string lower_key = t.first;
        std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        lower_keys.push_back(std::move(lower_key));
        nva.push_back({
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(lower_keys.back().c_str())),
            const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(t.second.c_str())),
            lower_keys.back().size(), t.second.size(), NGHTTP3_NV_FLAG_NONE
        });
    }

    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }

    // Signal EOF on the streaming body data so the data reader callback
    // returns 0 bytes with EOF flag, then submit trailers
    auto it = streaming_body_data_.find(stream_id);
    if (it != streaming_body_data_.end()) {
        it->second.eof = true;
        // Resume the data reader if it was deferred, so it can signal EOF
        if (it->second.deferred) {
            nghttp3_conn_resume_stream(h3_conn_, stream_id);
        }
    }

    int rv = nghttp3_conn_submit_trailers(h3_conn_, stream_id, nva.data(), nva.size());
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "nghttp3_conn_submit_trailers failed: %s",
                              nghttp3_strerror(rv));
        return -1;
    }

    pending_write_.store(true, std::memory_order_release);
    return 0;
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

    printd(5, "QuicSession::submitResponseStreaming() stream_id=" QLLD " status=%d - streaming_body_data_ entry created\n",
        stream_id, status_code);
    return 0;
}

int QuicSession::sendStreamData(int64_t stream_id, const void* data, size_t len,
                                 bool end_stream, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    auto it = streaming_body_data_.find(stream_id);
    if (it == streaming_body_data_.end()) {
        printd(1, "QuicSession::sendStreamData() stream_id=" QLLD " NOT FOUND in streaming_body_data_\n",
            stream_id);
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "no streaming response for stream %" PRId64, stream_id);
        return -1;
    }

    auto& sbd = it->second;

    // Check backpressure: if staging buffer > 1MB, signal caller to retry.
    // sent_bufs holds data already given to nghttp3/ngtcp2 (not counted here
    // because it will be freed via h3AckedStreamDataCallback).
    // Skip backpressure for FIN-only submissions (no data to append) — rejecting
    // the FIN when the buffer is transiently above the threshold would leave the
    // stream open forever if the caller has no retry loop for the FIN.
    if (sbd.data.size() > QUIC_MAX_STREAM_BODY && (data && len > 0)) {
        printd(5, "QuicSession::sendStreamData() stream_id=" QLLD " BACKPRESSURE pending=%d\n",
            stream_id, (int)sbd.data.size());
        return 1;
    }

    // Append new data to staging buffer.
    // h3ReadDataCallback moves data into sent_bufs via std::move,
    // leaving data empty for new appends.
    if (data && len > 0) {
        auto bp = static_cast<const uint8_t*>(data);
        sbd.data.insert(sbd.data.end(), bp, bp + len);
    }

    if (end_stream) {
        sbd.eof = true;
    }

    // Resume the deferred data reader if it was waiting
    if (sbd.deferred && h3_conn_) {
        printd(5, "QuicSession::sendStreamData() stream_id=" QLLD " resuming deferred stream\n",
            stream_id);
        int rv = nghttp3_conn_resume_stream(h3_conn_, stream_id);
        if (rv != 0 && rv != NGHTTP3_ERR_STREAM_NOT_FOUND) {
            xsink->raiseException("QUIC-HTTP3-ERROR",
                "nghttp3_conn_resume_stream failed: %s", nghttp3_strerror(rv));
            return -1;
        }
        sbd.deferred = false;
    }

    printd(5, "QuicSession::sendStreamData() stream_id=" QLLD " len=%d eof=%d pending=%d deferred=%d\n",
        stream_id, (int)len, end_stream, (int)sbd.data.size(), sbd.deferred);
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
        if (it->second.data.size() <= QUIC_MAX_STREAM_BODY) {
            return 0;  // buffer already below threshold
        }
    }

    if (closed_.load(std::memory_order_acquire)) {
        return -1;  // session closing
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
                return closed_.load(std::memory_order_acquire)
                    || drain_gen_.load(std::memory_order_acquire) != gen;
            };
            if (timeout_ms < 0) {
                drain_cv_.wait(dlock, gen_changed);
            } else {
                drain_cv_.wait_until(dlock, deadline, gen_changed);
            }
        }

        if (closed_.load(std::memory_order_acquire)) {
            return -1;  // session closing
        }

        // Re-check actual predicate under mtx_
        {
            std::lock_guard<std::recursive_mutex> lock(mtx_);
            auto it = streaming_body_data_.find(stream_id);
            if (it == streaming_body_data_.end()) {
                return -1;  // stream removed
            }
            if (it->second.data.size() <= QUIC_MAX_STREAM_BODY) {
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

// ===== Extended CONNECT (RFC 9220) =====

int QuicSession::submitConnectResponse(int64_t stream_id, int status_code,
        const strcase_str_map_t& headers, ExceptionSink* xsink) {
    // Build header name-value pairs OUTSIDE the lock
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

    // Regular headers (HTTP/3 requires lowercase, no hop-by-hop)
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

    // Always set up data reader — CONNECT response uses deferred streaming
    // with no_end_stream semantics (stream stays open for bidirectional tunnel)
    nghttp3_data_reader dr;
    dr.read_data = h3ReadDataCallback;

    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!h3_conn_) {
        xsink->raiseException("QUIC-HTTP3-ERROR", "HTTP/3 layer not initialized");
        return -1;
    }

    // Validate: stream must exist and be a CONNECT request
    auto sit = streams_.find(stream_id);
    if (sit == streams_.end()) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "stream %" PRId64 " not found", stream_id);
        return -1;
    }
    if (!sit->second->is_connect) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "stream %" PRId64 " is not a CONNECT request", stream_id);
        return -1;
    }

    // Guard against double submit
    if (body_data_.count(stream_id) || streaming_body_data_.count(stream_id)) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "response already submitted for stream %" PRId64, stream_id);
        return -1;
    }

    // Create deferred streaming body data entry — the CONNECT tunnel keeps the
    // stream open indefinitely; h3ReadDataCallback returns NGHTTP3_ERR_WOULDBLOCK
    // until data is pushed via sendStreamData()
    auto& sbd = streaming_body_data_[stream_id];
    sbd = QuicStreamingBodyData{};
    sbd.deferred = true;

    int rv = nghttp3_conn_submit_response(h3_conn_, stream_id, nva.data(), nva.size(), &dr);
    if (rv != 0) {
        xsink->raiseException("QUIC-HTTP3-ERROR",
            "nghttp3_conn_submit_response failed for CONNECT stream: %s",
            nghttp3_strerror(rv));
        streaming_body_data_.erase(stream_id);
        return -1;
    }

    pending_write_.store(true, std::memory_order_release);
    return 0;
}

QoreValue QuicSession::readConnectStreamData(int64_t stream_id, ExceptionSink* xsink) {
    std::lock_guard<std::mutex> lg(connect_data_mutex_);
    auto it = connect_stream_data_.find(stream_id);
    if (it == connect_stream_data_.end() || it->second.empty()) {
        return QoreValue();  // NOTHING
    }

    // Extract data with swap — avoids copy and releases vector memory
    std::vector<uint8_t> data;
    data.swap(it->second);
    SimpleRefHolder<BinaryNode> bin(new BinaryNode());
    bin->append(data.data(), data.size());
    return bin.release();
}

void QuicSession::registerConnectStreamQueue(int64_t stream_id, Queue* queue) {
    ExceptionSink xsink;

    // Check body_complete under mtx_ FIRST to maintain consistent lock ordering
    // (mtx_ -> connect_data_mutex_), matching cleanupStream() and callbacks.
    bool already_complete = false;
    {
        std::lock_guard<std::recursive_mutex> lock(mtx_);
        auto sit = streams_.find(stream_id);
        if (sit != streams_.end() && sit->second->body_complete) {
            already_complete = true;
        }
    }

    // Now do all Queue/buffer work under connect_data_mutex_ only
    std::lock_guard<std::mutex> lg(connect_data_mutex_);

    // Deref any existing queue for this stream (shouldn't happen, but be safe)
    auto existing = connect_stream_queues_.find(stream_id);
    if (existing != connect_stream_queues_.end()) {
        existing->second->deref(&xsink);
    }
    connect_stream_queues_[stream_id] = queue;

    // Flush any data that was buffered before the Queue was registered
    auto it = connect_stream_data_.find(stream_id);
    if (it != connect_stream_data_.end() && !it->second.empty()) {
        SimpleRefHolder<BinaryNode> bin(new BinaryNode());
        bin->append(it->second.data(), it->second.size());
        printd(5, "QuicSession::registerConnectStreamQueue() stream_id=" QLLD
            " flushing %d buffered bytes to Queue\n", stream_id, (int)it->second.size());
        queue->pushAndTakeRef(bin.release());
        it->second.clear();
    }
    connect_stream_data_.erase(stream_id);

    // If END_STREAM already arrived before registration, push sentinel and release
    if (already_complete) {
        printd(5, "QuicSession::registerConnectStreamQueue() stream_id=" QLLD
            " END_STREAM already received, pushing sentinel\n", stream_id);
        queue->pushAndTakeRef(QoreValue());
        connect_stream_queues_.erase(stream_id);
        queue->deref(&xsink);
    }
}

void QuicSession::deregisterConnectStreamQueue(int64_t stream_id) {
    ExceptionSink xsink;
    std::lock_guard<std::mutex> lg(connect_data_mutex_);
    auto it = connect_stream_queues_.find(stream_id);
    if (it != connect_stream_queues_.end()) {
        it->second->deref(&xsink);
        connect_stream_queues_.erase(it);
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
        printd(5, "QuicSession::markStreamComplete() stream_id=" QLLD " body_size=%d status=%d dispatched=%d\n",
            stream_id, (int)it->second->body.size(), it->second->status_code,
            it->second->dispatched ? 1 : 0);
        it->second->state = QuicStreamState::Closed;
        it->second->body_complete = true;

        // If stream was dispatched via headers-only mode, the handler is reading DATA
        // incrementally. Keep stream in map for continued DATA accumulation; don't push
        // to completed_streams or erase. Notify the handler's condition variable.
        if (it->second->dispatched) {
            printd(5, "QuicSession::markStreamComplete() stream_id=" QLLD " dispatched, keeping in map\n",
                stream_id);
            // Wake handler threads waiting on body data
            {
                std::lock_guard<std::mutex> lg(stream_data_mtx_);
                stream_data_gen_.fetch_add(1, std::memory_order_release);
            }
            stream_data_cv_.notify_all();
            return;
        }

        // In headers-only mode, if headers are complete but the stream hasn't been
        // dispatched yet (e.g., HEADERS + DATA + END_STREAM arrived in one batch),
        // keep the stream in the map so takeHeadersReadyStreamCopy() can find it.
        if (headers_only_mode_ && it->second->headers_complete) {
            printd(5, "QuicSession::markStreamComplete() stream_id=" QLLD " headers-only mode, keeping in map\n",
                stream_id);
            return;
        }

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
            if (it->second->connect_tunnel_active || it->second->streaming) {
                // CONNECT tunnel streams and streaming response streams must stay
                // in streams_ for continued DATA accumulation.  Clone essential
                // fields for the caller; move body and clear the original so the
                // next DATA chunk starts fresh.
                auto result = std::make_unique<QuicStreamInfo>();
                result->stream_id = it->second->stream_id;
                result->method = it->second->method;
                result->path = it->second->path;
                result->authority = it->second->authority;
                result->scheme = it->second->scheme;
                result->status_code = it->second->status_code;
                result->headers = it->second->headers;
                result->state = it->second->state;
                result->body_complete = it->second->body_complete;
                result->connect_protocol = it->second->connect_protocol;
                result->is_connect = it->second->is_connect;
                result->connect_tunnel_active = it->second->connect_tunnel_active;
                result->streaming = it->second->streaming;
                // Move body for incremental data delivery (client side)
                result->body = std::move(it->second->body);
                it->second->body.clear();
                return result;
            }
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

void QuicSession::setHeadersOnlyMode(bool v) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    headers_only_mode_ = v;
}

void QuicSession::setStreamStreaming(int64_t stream_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = streams_.find(stream_id);
    if (it != streams_.end()) {
        it->second->streaming = true;
    }
}

std::unique_ptr<QuicStreamInfo> QuicSession::takeHeadersReadyStreamCopy() {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    for (auto& [id, info] : streams_) {
        if (info->headers_complete && !info->dispatched) {
            // Copy stream info for the caller (headers, method, path, etc.)
            auto copy = std::make_unique<QuicStreamInfo>(*info);
            // Clear body on the COPY: any DATA that arrived with HEADERS stays
            // in the original for takeStreamData()/readQuicStreamDataBlock() to return.
            copy->body.clear();
            info->dispatched = true;

            printd(5, "QuicSession::takeHeadersReadyStreamCopy() stream_id=" QLLD "\n", id);
            return copy;
        }
    }
    return nullptr;
}

bool QuicSession::isStreamComplete(int64_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        return true;  // Stream not found, treat as complete
    }
    return it->second->body_complete;
}

bool QuicSession::isStreamFullyAcked(int64_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    return closed_streams_.count(stream_id) > 0;
}

void QuicSession::removeClosedStream(int64_t stream_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    closed_streams_.erase(stream_id);
}

uint64_t QuicSession::getBytesInFlight() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!conn_) {
        return 0;
    }
    ngtcp2_conn_info cinfo;
    ngtcp2_conn_get_conn_info(conn_, &cinfo);
    return cinfo.bytes_in_flight;
}

void QuicSession::cleanupStream(int64_t stream_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = streams_.find(stream_id);
    if (it != streams_.end()) {
        printd(5, "QuicSession::cleanupStream() stream_id=" QLLD " removing dispatched stream\n",
            stream_id);
        streams_.erase(it);
    }
    // Deregister Queue and release references — handler has exited
    {
        ExceptionSink xsink;
        std::lock_guard<std::mutex> lg(connect_data_mutex_);
        auto qit = connect_stream_queues_.find(stream_id);
        if (qit != connect_stream_queues_.end()) {
            qit->second->deref(&xsink);
            connect_stream_queues_.erase(qit);
        }
        connect_stream_data_.erase(stream_id);
    }
}

int QuicSession::resetStream(int64_t stream_id) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    printd(5, "QuicSession::resetStream() stream_id=" QLLD "\n", stream_id);
    int result = 0;
    if (h3_conn_) {
        nghttp3_conn_shutdown_stream_read(h3_conn_, stream_id);
    }
    if (conn_) {
        int rv = ngtcp2_conn_shutdown_stream_read(conn_, 0, stream_id,
            NGHTTP3_H3_REQUEST_CANCELLED);
        if (rv != 0 && rv != NGTCP2_ERR_STREAM_NOT_FOUND) {
            // Transport-level reset failed, but nghttp3 shutdown already ran
            // above.  Always clean up local state to avoid resource leaks —
            // the handler is done and stale entries would never be reclaimed.
            printd(1, "QuicSession::resetStream() ngtcp2_conn_shutdown_stream_read "
                "failed: %s (cleaning up local state anyway)\n", ngtcp2_strerror(rv));
            result = -1;
        }
    }
    // Always clean up local stream state regardless of transport-level errors
    auto it = streams_.find(stream_id);
    if (it != streams_.end()) {
        streams_.erase(it);
    }
    // Clean up any buffered extended-CONNECT tunnel data for this stream
    {
        std::lock_guard<std::mutex> cg(connect_data_mutex_);
        connect_stream_data_.erase(stream_id);
    }
    return result;
}



QoreValue QuicSession::takeStreamData(int64_t stream_id, bool& complete) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    auto it = streams_.find(stream_id);
    if (it == streams_.end()) {
        complete = true;  // Stream not found, treat as complete
        return QoreValue();
    }

    // Atomic: extract data AND check completion under one lock
    complete = it->second->body_complete;

    if (it->second->body.empty()) {
        return QoreValue();  // NOTHING — caller checks 'complete' to decide next step
    }

    // Extract data with swap — avoids copy and releases vector memory
    std::vector<char> data;
    data.swap(it->second->body);
    size_t consumed = data.size();

    // Consumption-driven flow control: extend QUIC windows only when the handler
    // consumes data, not when it arrives. This naturally caps the buffer to the
    // initial flow control window (QUIC_SERVER_INITIAL_MAX_STREAM_DATA = 256KB)
    // and provides backpressure when the handler is slow.
    if (conn_) {
        ngtcp2_conn_extend_max_stream_offset(conn_, stream_id,
                                              static_cast<uint64_t>(consumed));
        ngtcp2_conn_extend_max_offset(conn_, static_cast<uint64_t>(consumed));
        // Signal that MAX_STREAM_DATA/MAX_DATA frames need to be sent so the peer
        // can continue sending. Without this, the window update is delayed until
        // the next natural I/O cycle (timer or incoming packet).
        pending_write_.store(true, std::memory_order_release);
    }

    SimpleRefHolder<BinaryNode> bin(new BinaryNode());
    bin->append(data.data(), data.size());
    return bin.release();
}

void QuicSession::waitForStreamData(int timeout_ms) {
    if (closed_.load(std::memory_order_acquire)) {
        return;
    }
    unsigned gen = stream_data_gen_.load(std::memory_order_acquire);
    std::unique_lock<std::mutex> lk(stream_data_mtx_);
    int wait_ms = timeout_ms >= 0 ? std::min(timeout_ms, 100) : 100;
    stream_data_cv_.wait_for(lk, std::chrono::milliseconds(wait_ms),
        [this, gen]() {
            return closed_.load(std::memory_order_acquire)
                || stream_data_gen_.load(std::memory_order_acquire) != gen;
        });
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

void QuicSession::markClosed() {
    closed_.store(true, std::memory_order_release);

    // Wake all handler threads blocked in waitForStreamData()
    stream_data_gen_.fetch_add(1, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lg(stream_data_mtx_);
    }
    stream_data_cv_.notify_all();

    // Wake all handler threads blocked in waitForStreamDrain()
    drain_gen_.fetch_add(1, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lg(drain_mtx_);
    }
    drain_cv_.notify_all();
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

    // Track 0-RTT data reception on existing streams only (don't create entries
    // for HTTP/3 internal uni-streams — they never complete and would leak)
    if (flags & NGTCP2_STREAM_DATA_FLAG_0RTT) {
        auto it = session->streams_.find(stream_id);
        if (it != session->streams_.end()) {
            it->second->received_0rtt_data = true;
        }
    }

    // Forward to HTTP/3 layer if available
    if (session->h3_conn_) {
        printd(5, "recvStreamDataCallback() stream_id=" QLLD " datalen=%d fin=%d\n",
            stream_id, (int)datalen, fin);
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
    printd(5, "QuicSession::streamCloseCallback() stream_id=" QLLD " flags=0x%x error_code=%llu\n",
        stream_id, flags, (unsigned long long)app_error_code);

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

        // Record that this stream is fully closed (all data ACKed by peer).
        // Poll operations check this via isStreamFullyAcked() to avoid
        // transitioning to SENT before retransmission can recover data
        // lost during connection migration.
        session->closed_streams_.insert(stream_id);

        // Clean up body data
        session->body_data_.erase(stream_id);
        session->streaming_body_data_.erase(stream_id);

        // Clean up extended CONNECT tunnel data, Queue, and notify callback
        {
            ExceptionSink xsink;
            std::lock_guard<std::mutex> lg(session->connect_data_mutex_);
            // Push NOTHING sentinel to registered Queue if still active, then release ref
            auto qit = session->connect_stream_queues_.find(stream_id);
            if (qit != session->connect_stream_queues_.end()) {
                Queue* q = qit->second;
                session->connect_stream_queues_.erase(qit);
                q->pushAndTakeRef(QoreValue());
                q->deref(&xsink);
                printd(5, "QuicSession::streamCloseCallback() stream_id=" QLLD
                    " pushed sentinel to Queue on stream close\n", stream_id);
            }
            // Close and clean up the notifier so the watcher thread exits
            // Clean up fallback buffer — only erase if empty (data may still
            // be needed by readConnectStreamData callers)
            auto cit = session->connect_stream_data_.find(stream_id);
            if (cit != session->connect_stream_data_.end() && cit->second.empty()) {
                session->connect_stream_data_.erase(cit);
            }
        }

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
        {
            auto sit = session->streams_.find(stream_id);
            if (sit != session->streams_.end() && sit->second->is_connect) {
                // For CONNECT tunnel streams, set body_complete directly without
                // re-dispatching via markStreamComplete()
                sit->second->body_complete = true;
            } else {
                session->markStreamComplete(stream_id);
            }
        }

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

int QuicSession::handshakeCompletedCallback(ngtcp2_conn* conn, void* user_data) {
    auto* session = static_cast<QuicSession*>(user_data);
    session->handshake_completed_.store(true, std::memory_order_release);

    // Configure keepalive pings based on peer's max_idle_timeout.
    // Without this, a dead peer (crashed without CONNECTION_CLOSE) is only
    // detected after our own idle timeout expires.  Following curl's pattern:
    // send pings at half the peer's idle timeout to keep the connection alive
    // and detect dead peers quickly via timeout on the ping response.
    const ngtcp2_transport_params* rp = ngtcp2_conn_get_remote_transport_params(conn);
    if (rp && rp->max_idle_timeout > 0) {
        ngtcp2_duration keep_ns = rp->max_idle_timeout / 2;
        if (keep_ns < 1) {
            keep_ns = 1;
        }
        ngtcp2_conn_set_keep_alive_timeout(conn, keep_ns);
    }

    return 0;
}

int QuicSession::extendMaxLocalStreamsBidiCallback(ngtcp2_conn* /* conn */,
                                                    uint64_t /* max_streams */,
                                                    void* /* user_data */) {
    // Client: can open more request streams
    return 0;
}

int QuicSession::extendMaxRemoteStreamsBidiCallback(ngtcp2_conn* /* conn */,
                                                     uint64_t max_streams,
                                                     void* user_data) {
    // Server: the client is now permitted to open more bidirectional request streams.
    // Keep nghttp3 in sync so PRIORITY_UPDATE frames referencing those new stream IDs
    // are accepted rather than rejected with NGHTTP3_ERR_H3_ID_ERROR.
    auto* session = static_cast<QuicSession*>(user_data);
    if (session->is_server_ && session->h3_conn_) {
        nghttp3_conn_set_max_client_streams_bidi(session->h3_conn_, max_streams);
    }
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
                                   ngtcp2_encryption_level level,
                                   void* user_data) {
    // Detect 0-RTT TX key installation — when the 0-RTT encryption level key
    // is installed, the client can start sending early data
    auto* session = static_cast<QuicSession*>(user_data);
    if (level == NGTCP2_ENCRYPTION_LEVEL_0RTT && session->attempting_0rtt_.load(std::memory_order_acquire)) {
        session->early_data_ready_.store(true, std::memory_order_release);
        printd(3, "QuicSession::recvTxKeyCallback(): 0-RTT TX key installed (session %lld)\n",
            (long long)session->session_id_);
    }
    // HTTP/3 setup is deferred to the poll loop (QCS_SETUP_HTTP3 state)
    // after the handshake is fully complete and transport parameters are
    // exchanged, so that uni-stream limits are available.
    return 0;
}

int QuicSession::newSessionTicketCallback(SSL* ssl, SSL_SESSION* session) {
    // NOTE: This is called from OpenSSL (C code) — C++ exceptions must NOT propagate.
    // All allocating operations (std::string, std::vector) are wrapped in try/catch.
    try {
        // Get the QuicSession from SSL app_data
        auto* conn_ref = static_cast<ngtcp2_crypto_conn_ref*>(SSL_get_app_data(ssl));
        if (!conn_ref || !conn_ref->user_data) {
            return 0;
        }
        auto* qs = static_cast<QuicSession*>(conn_ref->user_data);

        // Verify the session ticket has max_early_data == 0xffffffff (QUIC requirement)
        // RFC 9001 §4.6.1: "A client MUST treat receipt of a TLS NewSessionTicket
        // message with a max_early_data_size of any value other than 0xffffffff as
        // a connection error of type PROTOCOL_VIOLATION."
        uint32_t max_early_data = SSL_SESSION_get_max_early_data(session);
        if (max_early_data != 0xffffffff) {
            printd(3, "QuicSession::newSessionTicketCallback(): ignoring ticket with "
                "max_early_data=%u (expected 0xffffffff)\n", max_early_data);
            // Return 0: OpenSSL frees the session
            return 0;
        }

        // Encode transport params for 0-RTT restoration
        uint8_t tp_buf[512];
        ngtcp2_ssize tp_len = ngtcp2_conn_encode_0rtt_transport_params(qs->conn_, tp_buf, sizeof(tp_buf));
        if (tp_len < 0) {
            printd(2, "QuicSession::newSessionTicketCallback(): encode_0rtt_transport_params failed: %s\n",
                ngtcp2_strerror(static_cast<int>(tp_len)));
            return 0;
        }

        // Build cached ticket — order: assign vector first (can throw), then up_ref session
        // (can't fail). This avoids leaking a ref count if vector::assign throws.
        QuicCachedTicket ticket;
        ticket.transport_params.assign(tp_buf, tp_buf + tp_len);
        SSL_SESSION_up_ref(session);
        ticket.session = session;

        // Store in cache keyed by origin
        std::string origin = qs->host_ + ":" + std::to_string(qs->port_);
        QuicSessionTicketCache::instance().store(origin, std::move(ticket));

        printd(3, "QuicSession::newSessionTicketCallback(): cached ticket for '%s' "
            "(tp_len=%zd, session %lld)\n", origin.c_str(), tp_len, (long long)qs->session_id_);
    } catch (const std::exception& e) {
        printd(0, "QuicSession::newSessionTicketCallback(): exception: %s\n", e.what());
    } catch (...) {
        printd(0, "QuicSession::newSessionTicketCallback(): unknown exception\n");
    }

    return 0;
}

int QuicSession::earlyDataRejectedCallback(ngtcp2_conn* /* conn */, void* user_data) {
    // NOTE: This is called from ngtcp2 (C code) — C++ exceptions must NOT propagate.
    auto* session = static_cast<QuicSession*>(user_data);
    session->early_data_rejected_.store(true, std::memory_order_release);

    try {
        // Invalidate the cached ticket since the server rejected 0-RTT
        std::string origin = session->host_ + ":" + std::to_string(session->port_);
        QuicSessionTicketCache::instance().remove(origin);

        printd(2, "QuicSession::earlyDataRejectedCallback(): 0-RTT rejected by server, "
            "removed cached ticket for '%s' (session %lld)\n",
            origin.c_str(), (long long)session->session_id_);
    } catch (const std::exception& e) {
        printd(0, "QuicSession::earlyDataRejectedCallback(): exception: %s\n", e.what());
    } catch (...) {
        printd(0, "QuicSession::earlyDataRejectedCallback(): unknown exception\n");
    }

    return 0;
}

// ===== Path Validation / Migration Callbacks =====
//
// All ngtcp2 callbacks below are invoked from C code (ngtcp2_conn_read_pkt,
// ngtcp2_conn_writev_stream, etc.).  C++ exceptions must NEVER propagate
// through C stack frames — this is undefined behavior.  Every callback wraps
// its body in try/catch(...) to guarantee this invariant.

int QuicSession::pathValidationCallback(ngtcp2_conn* /* conn */,
                                         uint32_t /* flags */,
                                         const ngtcp2_path* path,
                                         const ngtcp2_path* /* old_path */,
                                         ngtcp2_path_validation_result res,
                                         void* user_data) {
    auto* session = static_cast<QuicSession*>(user_data);
    try {
        switch (res) {
            case NGTCP2_PATH_VALIDATION_RESULT_SUCCESS:
                printd(2, "QuicSession::pathValidationCallback(): path validation SUCCESS "
                    "(session %lld)\n", (long long)session->session_id_);
                // Use *Locked variants: this callback is invoked from
                // ngtcp2_conn_read_pkt() inside readPacketLocked() which
                // already holds mtx_.  Avoids recursive lock acquisition.
                session->updateRemoteAddrLocked(
                    reinterpret_cast<const struct sockaddr*>(path->remote.addr),
                    path->remote.addrlen);
                session->updateLocalAddrLocked(
                    reinterpret_cast<const struct sockaddr*>(path->local.addr),
                    path->local.addrlen);
                break;
            case NGTCP2_PATH_VALIDATION_RESULT_FAILURE:
                printd(2, "QuicSession::pathValidationCallback(): path validation FAILURE "
                    "(session %lld)\n", (long long)session->session_id_);
                break;
            case NGTCP2_PATH_VALIDATION_RESULT_ABORTED:
                printd(2, "QuicSession::pathValidationCallback(): path validation ABORTED "
                    "(session %lld)\n", (long long)session->session_id_);
                break;
        }
    } catch (const std::exception& e) {
        printd(0, "QuicSession::pathValidationCallback(): exception: %s\n", e.what());
    } catch (...) {
        printd(0, "QuicSession::pathValidationCallback(): unknown exception\n");
    }
    return 0;
}

int QuicSession::beginPathValidationCallback(ngtcp2_conn* /* conn */,
                                              uint32_t /* flags */,
                                              const ngtcp2_path* /* path */,
                                              const ngtcp2_path* /* fallback_path */,
                                              void* user_data) {
    try {
        auto* session = static_cast<QuicSession*>(user_data);
        printd(3, "QuicSession::beginPathValidationCallback(): path validation started "
            "(session %lld)\n", (long long)session->session_id_);
    } catch (const std::exception& e) {
        printd(0, "QuicSession::beginPathValidationCallback(): exception: %s\n", e.what());
    } catch (...) {
        printd(0, "QuicSession::beginPathValidationCallback(): unknown exception\n");
    }
    return 0;
}

int QuicSession::removeConnectionIdCallback(ngtcp2_conn* /* conn */,
                                             const ngtcp2_cid* cid,
                                             void* user_data) {
    auto* session = static_cast<QuicSession*>(user_data);
    // Lifecycle guarantee: dispatcher_ is set once during initServer() and cleared
    // only in ~QuicSession() (after ngtcp2_conn_del destroys conn_, which prevents
    // further callbacks).  This callback runs inside ngtcp2_conn_read_pkt() under
    // mtx_, so dispatcher_ is guaranteed non-null for the session's entire active
    // lifetime.  The null check is defensive — the try/catch guards against
    // unregisterConnectionId() throwing if the CID was already removed.
    try {
        if (session->dispatcher_) {
            std::string cid_str(reinterpret_cast<const char*>(cid->data), cid->datalen);
            session->dispatcher_->unregisterConnectionId(cid_str);
            printd(3, "QuicSession::removeConnectionIdCallback(): unregistered retired CID "
                "(session %lld, cid_len=%zu)\n",
                (long long)session->session_id_, cid->datalen);
        }
    } catch (const std::exception& e) {
        printd(0, "QuicSession::removeConnectionIdCallback(): exception: %s\n", e.what());
    } catch (...) {
        printd(0, "QuicSession::removeConnectionIdCallback(): unknown exception\n");
    }
    return 0;
}

int QuicSession::dcidStatusCallback(ngtcp2_conn* /* conn */,
                                     ngtcp2_connection_id_status_type type,
                                     uint64_t seq,
                                     const ngtcp2_cid* cid,
                                     const uint8_t* /* token */,
                                     void* user_data) {
    try {
        auto* session = static_cast<QuicSession*>(user_data);
        const char* type_str;
        switch (type) {
            case NGTCP2_CONNECTION_ID_STATUS_TYPE_ACTIVATE:
                type_str = "ACTIVATE";
                break;
            case NGTCP2_CONNECTION_ID_STATUS_TYPE_DEACTIVATE:
                type_str = "DEACTIVATE";
                break;
            default:
                type_str = "UNKNOWN";
                break;
        }
        printd(3, "QuicSession::dcidStatusCallback(): DCID %s seq=%" PRIu64 " cid_len=%zu "
            "(session %lld)\n",
            type_str, seq, cid->datalen, (long long)session->session_id_);
    } catch (const std::exception& e) {
        printd(0, "QuicSession::dcidStatusCallback(): exception: %s\n", e.what());
    } catch (...) {
        printd(0, "QuicSession::dcidStatusCallback(): unknown exception\n");
    }
    return 0;
}

// ===== Path Update Methods =====

void QuicSession::updateRemoteAddr(const struct sockaddr* addr, socklen_t len) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    updateRemoteAddrLocked(addr, len);
}

void QuicSession::updateRemoteAddrLocked(const struct sockaddr* addr, socklen_t len) {
    if (len > sizeof(remote_addr_)) {
        printd(0, "QuicSession::updateRemoteAddrLocked(): address too large (%d > %zu), "
            "ignoring (session %lld)\n", (int)len, sizeof(remote_addr_),
            (long long)session_id_);
        return;
    }
    memcpy(&remote_addr_, addr, len);
    remote_addrlen_ = len;
    path_migrated_.store(true, std::memory_order_release);
    migration_gen_.fetch_add(1, std::memory_order_release);
    printd(3, "QuicSession::updateRemoteAddrLocked(): remote address updated (session %lld, gen=%" PRIu64 ")\n",
        (long long)session_id_, migration_gen_.load(std::memory_order_relaxed));
}

void QuicSession::updateLocalAddr(const struct sockaddr* addr, socklen_t len) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    updateLocalAddrLocked(addr, len);
}

void QuicSession::updateLocalAddrLocked(const struct sockaddr* addr, socklen_t len) {
    if (len > sizeof(local_addr_)) {
        printd(0, "QuicSession::updateLocalAddrLocked(): address too large (%d > %zu), "
            "ignoring (session %lld)\n", (int)len, sizeof(local_addr_),
            (long long)session_id_);
        return;
    }
    memcpy(&local_addr_, addr, len);
    local_addrlen_ = len;
    printd(3, "QuicSession::updateLocalAddrLocked(): local address updated (session %lld)\n",
        (long long)session_id_);
}

// ===== Connection Migration =====

int QuicSession::initiateMigration(
    const struct sockaddr* new_local, socklen_t new_local_len,
    const struct sockaddr* new_remote, socklen_t new_remote_len,
    ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!conn_) {
        xsink->raiseException("QUIC-MIGRATION-ERROR", "QUIC connection not initialized");
        return -1;
    }
    if (is_server_) {
        xsink->raiseException("QUIC-MIGRATION-ERROR",
            "connection migration can only be initiated by the client");
        return -1;
    }
    if (!handshake_completed_.load(std::memory_order_acquire)) {
        xsink->raiseException("QUIC-MIGRATION-ERROR",
            "cannot migrate before handshake is complete");
        return -1;
    }

    // Validate address family consistency — QUIC does not support cross-family
    // migration (e.g. IPv4 → IPv6); ngtcp2 would reject it, but we provide a
    // clearer error message here
    sa_family_t current_family = remote_addr_.ss_family;
    if (new_remote->sa_family != current_family) {
        xsink->raiseException("QUIC-MIGRATION-ERROR",
            "address family mismatch: connection uses %s but new remote address is %s",
            current_family == AF_INET ? "IPv4" : "IPv6",
            new_remote->sa_family == AF_INET ? "IPv4" : "IPv6");
        return -1;
    }
    if (new_local->sa_family != current_family) {
        xsink->raiseException("QUIC-MIGRATION-ERROR",
            "address family mismatch: connection uses %s but new local address is %s",
            current_family == AF_INET ? "IPv4" : "IPv6",
            new_local->sa_family == AF_INET ? "IPv4" : "IPv6");
        return -1;
    }

    // Build the new path
    ngtcp2_path path = {};
    // NOTE: ngtcp2_conn_initiate_immediate_migration() copies the path addresses,
    // so stack-local storage is safe here
    struct sockaddr_storage local_storage, remote_storage;
    memcpy(&local_storage, new_local, new_local_len);
    memcpy(&remote_storage, new_remote, new_remote_len);
    path.local.addr = reinterpret_cast<ngtcp2_sockaddr*>(&local_storage);
    path.local.addrlen = new_local_len;
    path.remote.addr = reinterpret_cast<ngtcp2_sockaddr*>(&remote_storage);
    path.remote.addrlen = new_remote_len;

    int rv = ngtcp2_conn_initiate_immediate_migration(conn_, &path, timestamp());
    if (rv != 0) {
        const char* detail;
        switch (rv) {
            case NGTCP2_ERR_INVALID_STATE:
                detail = "connection not in a migratable state "
                    "(handshake may not be confirmed or connection is closing)";
                break;
            case NGTCP2_ERR_CONN_ID_BLOCKED:
                detail = "no spare connection IDs available for migration "
                    "(active_connection_id_limit exhausted)";
                break;
            case NGTCP2_ERR_INVALID_ARGUMENT:
                detail = "invalid path addresses";
                break;
            default:
                detail = ngtcp2_strerror(rv);
                break;
        }
        xsink->raiseException("QUIC-MIGRATION-ERROR",
            "ngtcp2_conn_initiate_immediate_migration failed: %s (rv=%d)", detail, rv);
        return -1;
    }

    // Exception safety: the ngtcp2 call above is the only fallible operation.
    // If it fails, we return early without modifying any session state.  The
    // operations below are all infallible (memcpy, atomic store/fetch_add on
    // POD types), so there is no partial-update risk.  The entire sequence runs
    // under mtx_ to prevent concurrent readers from seeing inconsistent state.

    // Update stored addresses to the new path
    memcpy(&local_addr_, new_local, new_local_len);
    local_addrlen_ = new_local_len;
    memcpy(&remote_addr_, new_remote, new_remote_len);
    remote_addrlen_ = new_remote_len;

    // Set migration flag and increment generation
    path_migrated_.store(true, std::memory_order_release);
    migration_gen_.fetch_add(1, std::memory_order_release);

    // Flush PATH_CHALLENGE packets
    pending_write_.store(true, std::memory_order_release);

    printd(2, "QuicSession::initiateMigration(): migration initiated "
        "(session %lld, gen=%" PRIu64 ")\n",
        (long long)session_id_, migration_gen_.load(std::memory_order_relaxed));

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
            // RFC 9220: Detect CONNECT request (may already have :protocol)
            if (value_str == "CONNECT" && !stream->connect_protocol.empty()) {
                stream->is_connect = true;
                stream->connect_tunnel_active = true;
            }
        } else if (name_str == ":path") {
            stream->path = value_str;
        } else if (name_str == ":authority") {
            stream->authority = value_str;
        } else if (name_str == ":scheme") {
            stream->scheme = value_str;
        } else if (name_str == ":protocol") {
            // RFC 9220: :protocol pseudo-header for extended CONNECT
            stream->connect_protocol = value_str;
            if (stream->method == "CONNECT") {
                stream->is_connect = true;
                stream->connect_tunnel_active = true;
            }
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
        stream->headers_end_stream = (fin != 0);

        // Pre-allocate body buffer using content-length hint to avoid repeated
        // reallocation in h3RecvDataCallback
        if (!fin) {
            auto cl_it = stream->headers.find("content-length");
            if (cl_it != stream->headers.end() && !cl_it->second.empty()) {
                char* end = nullptr;
                long cl = strtol(cl_it->second[0].c_str(), &end, 10);
                // Server: limit pre-alloc to QUIC_MAX_STREAM_BODY (DoS protection)
                // Client: allow larger pre-alloc (up to 64MB) for response bodies
                size_t max_reserve = session->is_server_
                    ? QUIC_MAX_STREAM_BODY : (64 * 1024 * 1024);
                if (cl > 0 && static_cast<size_t>(cl) <= max_reserve) {
                    stream->body.reserve(static_cast<size_t>(cl));
                }
            }
        }

        // RFC 9220: Extended CONNECT streams stay open for bidirectional tunnel
        // (e.g., WebSocket framing) — do NOT mark complete even if fin is set.
        // Push to completed_streams_ for handler dispatch WITHOUT calling
        // markStreamComplete(), which would set body_complete=true and cause
        // isStreamComplete() to return true immediately — preventing the
        // Queue-based drain from buffering tunnel data before the sentinel.
        if (stream->is_connect && !stream->connect_protocol.empty()) {
            stream->headers_complete = true;
            session->completed_streams_.push(stream_id);
            session->has_completed_streams_.store(true, std::memory_order_release);
            printd(5, "QuicSession::h3EndHeadersCallback() stream_id=" QLLD
                " CONNECT dispatched (body_complete=false, tunnel open)\n", stream_id);
            return 0;
        }

        // In headers-only mode, dispatch immediately on HEADERS
        if (session->headers_only_mode_) {
            if (fin) {
                // FIN on HEADERS = no body; mark truly complete
                printd(5, "QuicSession::h3EndHeadersCallback() stream_id=" QLLD " headers-only mode "
                    "FIN on HEADERS - marking complete\n", stream_id);
                session->markStreamComplete(stream_id);
            } else {
                // No FIN = body expected; stream stays in map with headers_complete=true
                // (set above) so takeHeadersReadyStreamCopy() can find it.
                // body_complete is NOT set — DATA frames are expected.
                printd(5, "QuicSession::h3EndHeadersCallback() stream_id=" QLLD " headers-only mode "
                    "no FIN - headers ready for dispatch\n", stream_id);
            }
            return 0;
        }

        // If fin is set, the stream has no body — mark it complete now
        if (fin) {
            printd(5, "QuicSession::h3EndHeadersCallback() stream_id=" QLLD " FIN set - marking complete (body=%d)\n",
                stream_id, (int)stream->body.size());
            session->markStreamComplete(stream_id);
        } else {
            printd(5, "QuicSession::h3EndHeadersCallback() stream_id=" QLLD " no FIN - waiting for body\n",
                stream_id);
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
        // Use find() instead of getOrCreateStream() to avoid recreating zombie streams
        // that were already cleaned up by the handler (via cleanupStream())
        auto it = session->streams_.find(stream_id);
        if (it == session->streams_.end()) {
            // Stream was cleaned up — extend flow control so the connection continues,
            // but discard the data
            printd(5, "h3RecvDataCallback() stream_id=" QLLD " datalen=%d — stream already cleaned up\n",
                stream_id, (int)datalen);
            ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                                  static_cast<uint64_t>(datalen));
            ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(datalen));
            return 0;
        }
        auto* stream = it->second.get();
        printd(5, "h3RecvDataCallback() stream_id=" QLLD " datalen=%d body_total=%d\n",
            stream_id, (int)datalen, (int)(stream->body.size() + datalen));

        // RFC 9220: Extended CONNECT tunnel — deliver data to handler (server side)
        if (stream->is_connect && session->is_server_) {
            {
                std::lock_guard<std::mutex> lg(session->connect_data_mutex_);
                auto qit = session->connect_stream_queues_.find(stream_id);
                if (qit != session->connect_stream_queues_.end()) {
                    // Direct push to registered Queue — data is available to
                    // the handler immediately without waiting for the I/O loop
                    SimpleRefHolder<BinaryNode> bin(new BinaryNode());
                    bin->append(data, datalen);
                    qit->second->pushAndTakeRef(bin.release());
                    printd(5, "h3RecvDataCallback() stream_id=" QLLD
                        " datalen=%d direct-push to Queue\n",
                        stream_id, (int)datalen);
                } else {
                    // Fallback: buffer for later (Queue not yet registered)
                    auto& buf = session->connect_stream_data_[stream_id];
                    buf.insert(buf.end(), data, data + datalen);
                    printd(5, "h3RecvDataCallback() stream_id=" QLLD
                        " datalen=%d buffered (no Queue registered)\n",
                        stream_id, (int)datalen);
                }
                // Stream data is dispatched by the I/O controller after this
                // continuePoll cycle completes (via on_exit stream queue check
                // in Http3ServerPollOperation). No explicit wake needed — epoll
                // POLLIN on the UDP socket handles inter-cycle data.
            }
            // Extend flow control
            ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                                  static_cast<uint64_t>(datalen));
            ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(datalen));
            return 0;
        }

        // Client-side CONNECT tunnel: deliver each DATA chunk to the
        // connect_stream_data_ buffer so readConnectStreamData() (used by
        // the low-level readHttp3StreamData API) can retrieve it.
        // Also push to completed_streams_ for the async I/O pipeline
        // (ChannelAction → Channel → readData()).
        if (stream->is_connect && !session->is_server_) {
            {
                std::lock_guard<std::mutex> lg(session->connect_data_mutex_);
                auto& buf = session->connect_stream_data_[stream_id];
                buf.insert(buf.end(), data, data + datalen);
            }
            stream->body.insert(stream->body.end(), data, data + datalen);
            // Mark as "completed" for incremental delivery — getOutput() builds
            // the response hash with the accumulated body and resets it.
            session->completed_streams_.push(stream_id);
            session->has_completed_streams_.store(true, std::memory_order_release);
            // Extend flow control
            ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                                  static_cast<uint64_t>(datalen));
            ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(datalen));
            return 0;
        }

        // For dispatched streams (headers-only mode), buffer data and notify handler.
        // Flow control is NOT extended here — it's extended in takeStreamData() when
        // the handler actually consumes data (consumption-driven flow control). This
        // naturally caps the buffer to the initial flow control window
        // (QUIC_SERVER_INITIAL_MAX_STREAM_DATA = 256KB) and provides backpressure
        // when the handler is slow.
        if (stream->dispatched) {
            stream->body.insert(stream->body.end(), data, data + datalen);
            // Wake session-wide CV for handler threads waiting on body data
            {
                std::lock_guard<std::mutex> lg(session->stream_data_mtx_);
                session->stream_data_gen_.fetch_add(1, std::memory_order_release);
            }
            session->stream_data_cv_.notify_all();
            return 0;
        }

        // Client-side streaming responses (SSE, etc.): incremental delivery
        // to ChannelAction via completed_streams_ — same pattern as CONNECT
        // tunnel (lines above).  Each DATA chunk is accumulated in body, then
        // the stream is pushed to completed_streams_.  takeCompletedStream()
        // moves the body and clears the original, keeping the stream in the
        // map for the next chunk.
        if (stream->streaming && !session->is_server_) {
            stream->body.insert(stream->body.end(), data, data + datalen);
            session->completed_streams_.push(stream_id);
            session->has_completed_streams_.store(true, std::memory_order_release);
            // Extend flow control
            ngtcp2_conn_extend_max_stream_offset(session->conn_, stream_id,
                                                  static_cast<uint64_t>(datalen));
            ngtcp2_conn_extend_max_offset(session->conn_, static_cast<uint64_t>(datalen));
            return 0;
        }

        // Guard against unbounded body accumulation — server side only.
        // Uses max_request_body_size_ (set via setMaxRequestBodySize(), propagated
        // from HttpServer's max_request_body_size option).  0 = unlimited.
        // Consistent with Http2Session::onDataChunkRecvCallback().
        if (session->is_server_ && session->max_request_body_size_ > 0
                && (int64_t)(stream->body.size() + datalen) > session->max_request_body_size_) {
            printd(1, "h3RecvDataCallback: body too large (%zu + %zu > " QLLD ") stream %lld\n",
                stream->body.size(), datalen, session->max_request_body_size_,
                (long long)stream_id);
            stream->body.clear();
            stream->error_message = "request body exceeded maximum size ("
                + std::to_string(session->max_request_body_size_) + " bytes)";
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

    try {
        // For CONNECT tunnel streams, set body_complete and push the NOTHING sentinel
        // directly to the registered Queue.  Do NOT call markStreamComplete() — the
        // stream was already dispatched via h3EndHeaders and must not be re-dispatched.
        auto it = session->streams_.find(stream_id);
        if (it != session->streams_.end() && it->second->is_connect) {
            it->second->body_complete = true;
            // Push NOTHING sentinel to registered Queue (if any) and release reference
            {
                ExceptionSink xsink;
                std::lock_guard<std::mutex> lg(session->connect_data_mutex_);
                auto qit = session->connect_stream_queues_.find(stream_id);
                if (qit != session->connect_stream_queues_.end()) {
                    Queue* q = qit->second;
                    session->connect_stream_queues_.erase(qit);
                    q->pushAndTakeRef(QoreValue());
                    q->deref(&xsink);
                    printd(5, "QuicSession::h3EndStreamCallback() stream_id=" QLLD
                        " CONNECT tunnel END_STREAM — sentinel pushed to Queue\n",
                        stream_id);
                } else {
                    printd(5, "QuicSession::h3EndStreamCallback() stream_id=" QLLD
                        " CONNECT tunnel END_STREAM — no Queue registered\n", stream_id);
                }
            }
            return 0;
        }

        session->markStreamComplete(stream_id);
    } catch (...) {
        return NGHTTP3_ERR_CALLBACK_FAILURE;
    }
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
        if (!sbd.data.empty()) {
            if (veccnt > 0) {
                // Move staging data into sent_bufs for stable pointer lifetime.
                // ngtcp2 retains raw pointers to stream data in frame chain
                // entries for retransmission — the buffer must remain valid
                // until acked_stream_data fires or the stream is closed.
                sbd.sent_bufs.emplace_back(std::move(sbd.data));
                // data is now moved-from (empty), ready for new appends
                auto& buf = sbd.sent_bufs.back();

                vec[0].base = const_cast<uint8_t*>(buf.data());
                vec[0].len = buf.size();
                printd(5, "h3ReadDataCallback() stream_id=" QLLD " providing %d bytes eof=%d sent_bufs=%d\n",
                    stream_id, (int)buf.size(), sbd.eof, (int)sbd.sent_bufs.size());

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
        printd(5, "h3ReadDataCallback() stream_id=" QLLD " WOULDBLOCK (no data, not eof)\n", stream_id);
        sbd.deferred = true;
        return NGHTTP3_ERR_WOULDBLOCK;
    }

    // Stream not found in either map — signal EOF
    *pflags = NGHTTP3_DATA_FLAG_EOF;
    return 0;
}

int QuicSession::h3AckedStreamDataCallback(nghttp3_conn* /* conn */, int64_t stream_id,
                                            uint64_t datalen, void* conn_user_data,
                                            void* /* stream_user_data */) {
    // Do NOT erase body_data_ here: nghttp3 may still hold a cached vec pointing
    // to the body data buffer.  Cleanup happens in streamCloseCallback after
    // nghttp3_conn_close_stream releases all internal references.

    // For streaming body data, free acknowledged sent buffers.  ngtcp2 retains
    // raw pointers to stream data in frame chain entries for retransmission, so
    // sent_bufs must stay alive until acknowledged here.
    auto* session = static_cast<QuicSession*>(conn_user_data);
    auto sit = session->streaming_body_data_.find(stream_id);
    if (sit != session->streaming_body_data_.end()) {
        auto& sbd = sit->second;
        sbd.front_acked += datalen;
        while (!sbd.sent_bufs.empty() && sbd.front_acked >= sbd.sent_bufs.front().size()) {
            sbd.front_acked -= sbd.sent_bufs.front().size();
            sbd.sent_bufs.pop_front();
        }
        printd(5, "h3AckedStreamDataCallback() stream_id=" QLLD " datalen=%d remaining_bufs=%d\n",
            stream_id, (int)datalen, (int)sbd.sent_bufs.size());
    }

    return 0;
}

#if NGHTTP3_VERSION_NUM >= 0x010e00  // v1.14.0+: recv_settings2 with nghttp3_proto_settings
int QuicSession::h3RecvSettings2Callback(nghttp3_conn* /* conn */,
                                          const nghttp3_proto_settings* settings,
                                          void* conn_user_data) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    session->remote_settings_received_.store(true, std::memory_order_release);
    if (settings->enable_connect_protocol) {
        session->remote_enable_connect_protocol_.store(true, std::memory_order_release);
    }
    return 0;
}
#else
int QuicSession::h3RecvSettingsCallback(nghttp3_conn* /* conn */,
                                         const nghttp3_settings* settings,
                                         void* conn_user_data) {
    auto* session = static_cast<QuicSession*>(conn_user_data);
    session->remote_settings_received_.store(true, std::memory_order_release);
    if (settings->enable_connect_protocol) {
        session->remote_enable_connect_protocol_.store(true, std::memory_order_release);
    }
    return 0;
}
#endif

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

void QuicSession::setStreamInputStream(int64_t stream_id, InputStream* is, ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    stream_input_streams_.emplace(stream_id, StreamInputStreamInfo(is));
    has_active_input_streams_.store(true, std::memory_order_release);
    printd(5, "QuicSession::setStreamInputStream() stream_id=" QLLD " pollable=%d fd=%d\n",
        stream_id, stream_input_streams_[stream_id].is_pollable,
        stream_input_streams_[stream_id].stream_fd);
}

bool QuicSession::hasActiveStreamInputStreams() const {
    return has_active_input_streams_.load(std::memory_order_acquire);
}

void QuicSession::processStreamInputStreams(ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    printd(5, "QuicSession::processStreamInputStreams() stream_count=%d\n",
        (int)stream_input_streams_.size());
    for (auto& [stream_id, info] : stream_input_streams_) {
        if (info.eof) {
            continue;
        }

        // Backpressure: skip if staging buffer full
        auto it = streaming_body_data_.find(stream_id);
        if (it != streaming_body_data_.end()) {
            if (it->second.data.size() > QUIC_MAX_STREAM_BODY) {
                continue;
            }
        }

        // Thread reassignment (first call only)
        if (info.need_reassign) {
            info.input_stream->reassignThread(xsink);
            if (*xsink) {
                return;
            }
            info.need_reassign = false;
        }

        // Read one chunk
        if (info.is_pollable) {
            SimpleRefHolder<BinaryNode> chunk(new BinaryNode);
            chunk->preallocate(65536);
            int64 count = info.input_stream->readNonBlock(
                const_cast<void*>(chunk->getPtr()), 65536, xsink);
            if (*xsink) {
                info.eof = true;
                continue;
            }
            if (count < 0) {
                // EAGAIN — not ready yet, event loop will wake us
                continue;
            }
            if (count == 0) {
                // EOF
                info.eof = true;
                sendStreamData(stream_id, nullptr, 0, true, xsink);
            } else {
                chunk->setSize(count);
                sendStreamData(stream_id, chunk->getPtr(), count, false, xsink);
            }
        } else {
            // Non-pollable (memory streams) — readHelper never blocks
            SimpleRefHolder<BinaryNode> chunk(info.input_stream->readHelper(65536, xsink));
            if (*xsink) {
                printd(1, "QuicSession::processStreamInputStreams() stream_id=" QLLD " readHelper exception\n",
                    stream_id);
                info.eof = true;
                continue;
            }
            if (!chunk || !chunk->size()) {
                printd(5, "QuicSession::processStreamInputStreams() stream_id=" QLLD " EOF\n",
                    stream_id);
                info.eof = true;
                sendStreamData(stream_id, nullptr, 0, true, xsink);
            } else {
                printd(5, "QuicSession::processStreamInputStreams() stream_id=" QLLD
                    " read %d bytes, calling sendStreamData\n",
                    stream_id, (int)chunk->size());
                sendStreamData(stream_id, chunk->getPtr(), chunk->size(), false, xsink);
            }
        }
        if (*xsink) {
            return;
        }
    }

    // Clean up completed streams (unassignThread + erase)
    for (auto it = stream_input_streams_.begin(); it != stream_input_streams_.end(); ) {
        if (it->second.eof) {
            if (!it->second.need_reassign) {
                ExceptionSink tmp;
                it->second.input_stream->unassignThread(&tmp);
            }
            it = stream_input_streams_.erase(it);
        } else {
            ++it;
        }
    }
    if (stream_input_streams_.empty()) {
        has_active_input_streams_.store(false, std::memory_order_release);
    }
}

void QuicSession::cleanupStreamInputStreams(ExceptionSink* xsink) {
    for (auto& [stream_id, info] : stream_input_streams_) {
        if (!info.need_reassign) {
            info.input_stream->unassignThread(xsink);
        }
    }
    stream_input_streams_.clear();
    has_active_input_streams_.store(false, std::memory_order_release);
}

void QuicSession::getExtraFds(std::vector<std::pair<int, int>>& extra_fds) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    for (auto& [stream_id, info] : stream_input_streams_) {
        if (!info.eof && info.is_pollable && info.stream_fd >= 0) {
            extra_fds.push_back({info.stream_fd, SOCK_POLLIN});
        }
    }
}

// ===== QUIC Variable-Length Integer Encoding (RFC 9000 §16) =====

size_t QuicSession::encodeVarInt(uint8_t* buf, uint64_t value) {
    if (value <= 63) {
        buf[0] = static_cast<uint8_t>(value);
        return 1;
    }
    if (value <= 16383) {
        buf[0] = static_cast<uint8_t>(0x40 | (value >> 8));
        buf[1] = static_cast<uint8_t>(value & 0xff);
        return 2;
    }
    if (value <= 1073741823) {
        buf[0] = static_cast<uint8_t>(0x80 | (value >> 24));
        buf[1] = static_cast<uint8_t>((value >> 16) & 0xff);
        buf[2] = static_cast<uint8_t>((value >> 8) & 0xff);
        buf[3] = static_cast<uint8_t>(value & 0xff);
        return 4;
    }
    if (value <= 4611686018427387903ULL) {
        buf[0] = static_cast<uint8_t>(0xc0 | (value >> 56));
        buf[1] = static_cast<uint8_t>((value >> 48) & 0xff);
        buf[2] = static_cast<uint8_t>((value >> 40) & 0xff);
        buf[3] = static_cast<uint8_t>((value >> 32) & 0xff);
        buf[4] = static_cast<uint8_t>((value >> 24) & 0xff);
        buf[5] = static_cast<uint8_t>((value >> 16) & 0xff);
        buf[6] = static_cast<uint8_t>((value >> 8) & 0xff);
        buf[7] = static_cast<uint8_t>(value & 0xff);
        return 8;
    }
    return 0;  // value too large
}

size_t QuicSession::decodeVarInt(const uint8_t* data, size_t datalen, uint64_t& value) {
    if (datalen == 0) {
        return 0;
    }
    uint8_t prefix = data[0] >> 6;
    size_t len = 1ULL << prefix;
    if (datalen < len) {
        return 0;  // truncated
    }
    value = data[0] & 0x3f;
    for (size_t i = 1; i < len; ++i) {
        value = (value << 8) | data[i];
    }
    return len;
}

size_t QuicSession::varIntLen(uint64_t value) {
    if (value <= 63) {
        return 1;
    }
    if (value <= 16383) {
        return 2;
    }
    if (value <= 1073741823) {
        return 4;
    }
    return 8;
}

// ===== QUIC Datagram Support (RFC 9221/9297) =====

int QuicSession::recvDatagramCallback(ngtcp2_conn* conn, uint32_t flags,
                                       const uint8_t* data, size_t datalen,
                                       void* user_data) {
    auto* session = static_cast<QuicSession*>(user_data);

    // Decode the quarter-stream-ID from the datagram payload (RFC 9297 §4)
    uint64_t quarter_stream_id;
    size_t varint_len = decodeVarInt(data, datalen, quarter_stream_id);
    if (varint_len == 0) {
        printd(2, "recvDatagramCallback(): datagram too short to decode quarter-stream-ID "
            "(len=%zu)\n", datalen);
        return 0;  // silently discard malformed datagrams
    }

    // Convert quarter-stream-ID back to stream ID; guard against overflow
    if (quarter_stream_id > static_cast<uint64_t>(INT64_MAX) / 4) {
        printd(2, "recvDatagramCallback(): quarter-stream-ID %" PRIu64
            " would overflow stream_id\n", quarter_stream_id);
        return 0;  // silently discard
    }
    int64_t stream_id = static_cast<int64_t>(quarter_stream_id * 4);

    // Extract payload after the quarter-stream-ID
    const uint8_t* payload = data + varint_len;
    size_t payload_len = datalen - varint_len;

    // Route to per-stream datagram queue
    try {
        {
            std::lock_guard<std::mutex> lg(session->datagram_mutex_);
            auto& queue = session->datagram_queues_[stream_id];
            queue.emplace_back(payload, payload + payload_len);
        }

        // Signal waiting readers
        session->datagram_gen_.fetch_add(1, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lg(session->datagram_cv_mtx_);
        }
        session->datagram_cv_.notify_all();
    } catch (...) {
        // C callback — must not propagate C++ exceptions through ngtcp2's C code
        printd(0, "recvDatagramCallback(): exception while queuing datagram "
            "(stream_id=" QLLD ")\n", stream_id);
    }

    return 0;
}

int QuicSession::submitDatagram(int64_t stream_id, const uint8_t* data, size_t len,
                                 ExceptionSink* xsink) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    if (!conn_) {
        xsink->raiseException("QUIC-DATAGRAM-ERROR", "QUIC connection not initialized");
        return -1;
    }

    // RFC 9297: quarter-stream-ID = stream_id / 4; stream_id must be a multiple of 4
    if (stream_id < 0 || (stream_id & 3) != 0) {
        xsink->raiseException("QUIC-DATAGRAM-ERROR",
            "invalid stream_id " QLLD " for datagram: must be a non-negative multiple of 4",
            stream_id);
        return -1;
    }

    // Check remote support
    const ngtcp2_transport_params* remote_params =
        ngtcp2_conn_get_remote_transport_params(conn_);
    if (!remote_params || remote_params->max_datagram_frame_size == 0) {
        xsink->raiseException("QUIC-DATAGRAM-NOT-SUPPORTED",
            "remote peer does not support QUIC datagrams (max_datagram_frame_size=0)");
        return -1;
    }

    // Compute quarter-stream-ID (RFC 9297 §4)
    uint64_t quarter_stream_id = static_cast<uint64_t>(stream_id) / 4;
    size_t qsid_len = varIntLen(quarter_stream_id);

    // Check size
    size_t total_len = qsid_len + len;
    if (total_len > remote_params->max_datagram_frame_size) {
        xsink->raiseException("QUIC-DATAGRAM-SIZE-ERROR",
            "datagram payload (%zu bytes + %zu header = %zu total) exceeds remote "
            "max_datagram_frame_size (%" PRIu64 ")",
            len, qsid_len, total_len, remote_params->max_datagram_frame_size);
        return -1;
    }

    // Build the framed datagram: quarter-stream-ID + payload
    std::vector<uint8_t> framed(total_len);
    encodeVarInt(framed.data(), quarter_stream_id);
    if (len > 0) {
        memcpy(framed.data() + qsid_len, data, len);
    }

    // Use monotonically increasing datagram IDs for tracking
    static std::atomic<uint64_t> next_dgram_id{1};
    uint64_t dgram_id = next_dgram_id.fetch_add(1, std::memory_order_relaxed);

    pending_datagrams_.emplace_back(dgram_id, std::move(framed));
    pending_write_.store(true, std::memory_order_release);

    return 0;
}

QoreValue QuicSession::readDatagram(int64_t stream_id, int timeout_ms, ExceptionSink* xsink) {
    // Try non-blocking read first
    {
        std::lock_guard<std::mutex> lg(datagram_mutex_);
        auto it = datagram_queues_.find(stream_id);
        if (it != datagram_queues_.end() && !it->second.empty()) {
            std::vector<uint8_t> data = std::move(it->second.front());
            it->second.pop_front();
            if (it->second.empty()) {
                datagram_queues_.erase(it);
            }
            // Copy data into a malloc'd buffer for BinaryNode ownership
            if (data.empty()) {
                SimpleRefHolder<BinaryNode> bn(new BinaryNode());
                return bn.release();
            }
            void* buf = malloc(data.size());
            if (!buf) {
                xsink->raiseException("QUIC-DATAGRAM-ERROR", "memory allocation failed");
                return QoreValue();
            }
            memcpy(buf, data.data(), data.size());
            SimpleRefHolder<BinaryNode> bn(new BinaryNode(buf, data.size()));
            return bn.release();
        }
    }

    if (timeout_ms == 0) {
        return QoreValue();
    }

    // Wait for data with timeout using generation-based CV
    auto deadline = std::chrono::steady_clock::now();
    if (timeout_ms > 0) {
        deadline += std::chrono::milliseconds(timeout_ms);
    }

    while (true) {
        if (closed_.load(std::memory_order_acquire)) {
            return QoreValue();
        }

        unsigned cur_gen = datagram_gen_.load(std::memory_order_acquire);

        // Check again under lock
        {
            std::lock_guard<std::mutex> lg(datagram_mutex_);
            auto it = datagram_queues_.find(stream_id);
            if (it != datagram_queues_.end() && !it->second.empty()) {
                std::vector<uint8_t> data = std::move(it->second.front());
                it->second.pop_front();
                if (it->second.empty()) {
                    datagram_queues_.erase(it);
                }
                // Copy data into a malloc'd buffer for BinaryNode ownership
                if (data.empty()) {
                    SimpleRefHolder<BinaryNode> bn(new BinaryNode());
                    return bn.release();
                }
                void* buf = malloc(data.size());
                if (!buf) {
                    xsink->raiseException("QUIC-DATAGRAM-ERROR", "memory allocation failed");
                    return QoreValue();
                }
                memcpy(buf, data.data(), data.size());
                SimpleRefHolder<BinaryNode> bn(new BinaryNode(buf, data.size()));
                return bn.release();
            }
        }

        // Wait on CV
        {
            std::unique_lock<std::mutex> lk(datagram_cv_mtx_);
            if (timeout_ms < 0) {
                // Wait up to 100ms per iteration to check for session close
                datagram_cv_.wait_for(lk, std::chrono::milliseconds(100),
                    [this, cur_gen]() {
                        return closed_.load(std::memory_order_acquire)
                            || datagram_gen_.load(std::memory_order_acquire) != cur_gen;
                    });
            } else {
                auto now = std::chrono::steady_clock::now();
                if (now >= deadline) {
                    return QoreValue();
                }
                auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - now);
                int wait_ms = std::min(static_cast<int>(remaining.count()), 100);
                datagram_cv_.wait_for(lk, std::chrono::milliseconds(wait_ms),
                    [this, cur_gen]() {
                        return closed_.load(std::memory_order_acquire)
                            || datagram_gen_.load(std::memory_order_acquire) != cur_gen;
                    });
            }
        }
    }
}

size_t QuicSession::getMaxDatagramPayloadSize(int64_t stream_id) const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!conn_) {
        return 0;
    }

    const ngtcp2_transport_params* remote_params =
        ngtcp2_conn_get_remote_transport_params(conn_);
    if (!remote_params || remote_params->max_datagram_frame_size == 0) {
        return 0;
    }

    // Subtract quarter-stream-ID encoding overhead
    uint64_t quarter_stream_id = static_cast<uint64_t>(stream_id) / 4;
    size_t qsid_len = varIntLen(quarter_stream_id);

    // Also consider the path MTU limit
    size_t max_udp = ngtcp2_conn_get_max_tx_udp_payload_size(conn_);
    // QUIC packet overhead: ~40 bytes (header + encryption), conservative estimate
    size_t max_datagram_in_packet = max_udp > 60 ? max_udp - 60 : 0;

    size_t max_frame = remote_params->max_datagram_frame_size;
    size_t effective_max = std::min(max_frame, max_datagram_in_packet);

    return effective_max > qsid_len ? effective_max - qsid_len : 0;
}

bool QuicSession::isDatagramSupported() const {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!conn_) {
        return false;
    }
    const ngtcp2_transport_params* remote_params =
        ngtcp2_conn_get_remote_transport_params(conn_);
    return remote_params && remote_params->max_datagram_frame_size > 0;
}
