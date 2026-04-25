/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
    QoreHttpClientConnectionManagerBase.cpp

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

#include <qore/Qore.h>
#include <qore/HttpClientConnectionManager.h>
#include <qore/QoreAbstractLoggerInterface.h>
#include <qore/QoreHttpClientObject.h>
#include <qore/QoreFuture.h>
#include "qore/intern/QoreHttp1ClientConnection.h"
#include "qore/intern/QoreHttp2ClientConnection.h"
#include "qore/intern/QoreHttp3ClientConnection.h"
#include "qore/intern/NegotiatingConnectionPollOp.h"
#include "qore/intern/QC_FutureImpl.h"
#include "qore/intern/QC_Future.h"
#include "qore/intern/QoreAsyncIoLogger.h"

#include <chrono>
#include <cstdio>
#include <cstring>

// ============================================================
// Construction / destruction
// ============================================================

HttpClientConnectionManagerBase::HttpClientConnectionManagerBase(const Options& opts,
        ExceptionSink* xsink)
    : opts_(opts) {
    // Validate options
    if (opts_.max_connections_per_host < 0) {
        xsink->raiseException("HTTPCLIENT-OPTION-ERROR",
            "max_connections_per_host must be >= 0 (0 = unlimited), got %d",
            opts_.max_connections_per_host);
        return;
    }
    if (opts_.max_streams_per_connection < 0) {
        xsink->raiseException("HTTPCLIENT-OPTION-ERROR",
            "max_streams_per_connection must be >= 0 (0 = unlimited), got %d",
            opts_.max_streams_per_connection);
        return;
    }

    // Parse proxy URL if provided.  Format: scheme://host[:port]
    // (matching Qore's parse_url for the basics — full RFC 3986 parsing
    // can wait until a downstream caller actually needs auth-in-URL etc.)
    if (!opts_.proxy_url.empty()) {
        const char* url = opts_.proxy_url.c_str();
        const char* scheme_end = strstr(url, "://");
        bool ssl = false;
        const char* host_start;
        if (scheme_end) {
            std::string scheme(url, scheme_end - url);
            if (scheme == "https") {
                ssl = true;
            } else if (scheme != "http") {
                xsink->raiseException("HTTPCLIENT-OPTION-ERROR",
                    "invalid proxy URL scheme %y in %y; must be http or https",
                    scheme.c_str(), opts_.proxy_url.c_str());
                return;
            }
            host_start = scheme_end + 3;
        } else {
            host_start = url;
        }

        // Strip any path/query — proxy connections don't use them
        const char* path_start = strchr(host_start, '/');
        std::string hostport;
        if (path_start) {
            hostport.assign(host_start, path_start - host_start);
        } else {
            hostport = host_start;
        }
        if (hostport.empty()) {
            xsink->raiseException("HTTPCLIENT-OPTION-ERROR",
                "proxy URL %y has no host", opts_.proxy_url.c_str());
            return;
        }

        // Split host:port
        std::string proxy_host;
        int proxy_port = ssl ? 443 : 80;
        size_t colon = hostport.rfind(':');
        // Skip if the colon is part of an IPv6 literal "[::]:port"
        if (colon != std::string::npos
                && (colon == 0 || hostport[colon - 1] != ']')) {
            proxy_host = hostport.substr(0, colon);
            try {
                proxy_port = std::stoi(hostport.substr(colon + 1));
            } catch (...) {
                xsink->raiseException("HTTPCLIENT-OPTION-ERROR",
                    "invalid proxy port in %y", opts_.proxy_url.c_str());
                return;
            }
        } else {
            proxy_host = hostport;
        }

        proxy_info_.reset(new ProxyInfo{std::move(proxy_host), proxy_port, ssl});
    }
}

HttpClientConnectionManagerBase::~HttpClientConnectionManagerBase() {
    // Lifetime contract (section 7.3): drain the pool, null every
    // connection's manager back-pointer, then close + deref.  This is
    // done by closeAll(), which the destructor calls explicitly so the
    // base destructor only frees the pool member containers (already
    // empty by that point).
    ExceptionSink xsink;
    closeAll(&xsink);
    xsink.clear();
}

// ============================================================
// Pool key + helpers
// ============================================================

std::string HttpClientConnectionManagerBase::poolKey(const char* host, int port) const {
    char buf[512];
    if (proxy_info_) {
        // Proxied: include the proxy in the key so the same target
        // reached through different proxies gets distinct entries.
        snprintf(buf, sizeof(buf), "%s:%d|%s:%d",
            proxy_info_->host.c_str(), proxy_info_->port, host, port);
    } else {
        snprintf(buf, sizeof(buf), "%s:%d", host, port);
    }
    return std::string(buf);
}

// ============================================================
// acquireConnection
// ============================================================

HttpClientConnectionBase* HttpClientConnectionManagerBase::findReusableLocked(
        const std::string& key) {
    auto it = pool_.find(key);
    if (it == pool_.end()) {
        return nullptr;
    }

    // For NEGOTIATE managers the pool may hold a mix of H1 and H2
    // connections under the same (host, port) key.  H2 is nearly always
    // cheaper to reuse than H1 — a single H2 connection multiplexes up
    // to max_concurrent_streams requests concurrently, whereas an H1
    // connection serializes one request at a time.  A dual-pass search
    // prefers H2 when available, falling back to H1 otherwise.
    //
    // For fixed-protocol managers (H1, H2, H3) every pool entry has the
    // same protocol, so the first pass either matches every entry or
    // none — the second pass is a no-op.  The extra traversal is O(n)
    // per pool key with small constants and skipped entirely for
    // non-NEGOTIATE managers.
    if (opts_.protocol == HttpClientProtocol::NEGOTIATE) {
        // Pass 1: prefer H2.
        for (HttpClientConnectionBase* conn : it->second) {
            if (conn->isClosed() || conn->isDraining()) {
                continue;
            }
            if (conn->getProtocol() != HttpClientProtocol::H2) {
                continue;
            }
            if (conn->tryReserveStream()) {
                return conn;
            }
        }
        // Pass 2: any live H1 (or other) connection.
        for (HttpClientConnectionBase* conn : it->second) {
            if (conn->isClosed() || conn->isDraining()) {
                continue;
            }
            if (conn->getProtocol() == HttpClientProtocol::H2) {
                continue;  // already tried above
            }
            if (conn->tryReserveStream()) {
                return conn;
            }
        }
        return nullptr;
    }

    // Fixed-protocol manager: single pass, insertion order.
    for (HttpClientConnectionBase* conn : it->second) {
        if (conn->isClosed() || conn->isDraining()) {
            continue;
        }
        if (conn->tryReserveStream()) {
            return conn;
        }
    }
    return nullptr;
}

void HttpClientConnectionManagerBase::evictDeadLocked(const std::string& key) {
    auto it = pool_.find(key);
    if (it == pool_.end()) {
        return;
    }
    auto& conns = it->second;
    auto write_it = conns.begin();
    for (auto read_it = conns.begin(); read_it != conns.end(); ++read_it) {
        if (!(*read_it)->isClosed()) {
            *write_it++ = *read_it;
        } else {
            // Closed connection — drop our pool ref.  setManager(nullptr)
            // is called separately by closeAndEvict / onConnectionClosed
            // / closeAll, so we don't need to do that here.
            ExceptionSink xs;
            (*read_it)->deref(&xs);
            xs.clear();
        }
    }
    conns.erase(write_it, conns.end());
    if (conns.empty()) {
        pool_.erase(it);
    }
}

HttpClientConnectionBase* HttpClientConnectionManagerBase::acquireConnection(
        const char* scheme, const char* host, int port, ExceptionSink* xsink) {
    return acquireConnectionImpl(scheme, host, port, /*wait_for_ready=*/true, xsink);
}

HttpClientConnectionBase* HttpClientConnectionManagerBase::acquireConnectionAsync(
        const char* scheme, const char* host, int port, ExceptionSink* xsink) {
    return acquireConnectionImpl(scheme, host, port, /*wait_for_ready=*/false, xsink);
}

HttpClientConnectionBase* HttpClientConnectionManagerBase::acquireConnectionImpl(
        const char* scheme, const char* host, int port,
        bool wait_for_ready, ExceptionSink* xsink) {
    // All three protocols (H1, H2, H3) are supported as of Phase P5.

    // Drain any deferred derefs from onConnectionClosed.  Safe here
    // because we're on an app thread, not the I/O thread.
    processDeferredDeref(xsink);

    bool ssl_required = (strcmp(scheme, "https") == 0);

    std::string key = poolKey(host, port);

    while (true) {
        // 1. Read-lock pool scan for a reusable connection.
        {
            std::shared_lock<std::shared_mutex> rl(pool_lock_);
            if (shutdown_) {
                xsink->raiseException("HTTPCLIENT-SHUTDOWN",
                    "connection manager is shutting down");
                return nullptr;
            }
            HttpClientConnectionBase* live = findReusableLocked(key);
            if (live) {
                return live;
            }
        }

        // 2. No live connection — serialize creation per key.  First
        // thread sets `creating_[key]`, others wait on `create_cond_`
        // up to connect_timeout.  On timeout, the waiter takes over.
        {
            std::unique_lock<std::mutex> cl(create_lock_);
            if (creating_.count(key)) {
                auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(opts_.connect_timeout_ms);
                bool became_free = create_cond_.wait_until(cl, deadline,
                    [this, &key]() { return creating_.count(key) == 0; });
                if (became_free) {
                    // Another thread finished creating — loop back and
                    // try the pool scan again.
                    continue;
                }
                // Timeout — take over creation.  Fall through.
            }
            creating_.insert(key);
        }

        // RAII to clear creating_ on any exit path.  scope_guard
        // pattern using a small lambda holder.
        struct CreatingGuard {
            HttpClientConnectionManagerBase* mgr;
            std::string key;
            ~CreatingGuard() {
                std::lock_guard<std::mutex> cl(mgr->create_lock_);
                mgr->creating_.erase(key);
                mgr->create_cond_.notify_all();
            }
        } cg{this, key};

        // 3. Recheck pool under write lock + capacity check.  Another
        // thread may have added a usable connection between our scan
        // (step 1) and acquiring `creating_` (step 2).
        {
            std::unique_lock<std::shared_mutex> wl(pool_lock_);
            if (shutdown_) {
                xsink->raiseException("HTTPCLIENT-SHUTDOWN",
                    "connection manager is shutting down");
                return nullptr;
            }
            evictDeadLocked(key);
            HttpClientConnectionBase* live = findReusableLocked(key);
            if (live) {
                return live;
            }

            // Capacity check
            if (opts_.max_connections_per_host) {
                auto it = pool_.find(key);
                int count = (it == pool_.end()) ? 0 : (int)it->second.size();
                if (count >= opts_.max_connections_per_host) {
                    xsink->raiseException("HTTPCLIENT-CAPACITY-ERROR",
                        "all %d connections to %s have reached the "
                        "concurrent stream limit", count, key.c_str());
                    return nullptr;
                }
            }
        }

        // 4. Create the connection OUTSIDE the pool lock.  Constructor
        // does DNS resolution + controller submission, both of which
        // can block.
        ReferenceHolder<HttpClientConnectionBase> conn(
            createConnection(key, host, port, ssl_required, xsink,
                wait_for_ready), xsink);
        if (*xsink || !conn) {
            return nullptr;
        }

        // 5. Add to pool under write lock and reserve a stream slot
        // for the creator.  Re-check shutdown_ in case closeAll raced
        // with us.
        HttpClientConnectionBase* conn_raw = conn.release();
        bool reserved;
        {
            std::unique_lock<std::shared_mutex> wl(pool_lock_);
            if (shutdown_) {
                wl.unlock();
                ExceptionSink shutdown_xs;
                conn_raw->closeConnection(&shutdown_xs);
                conn_raw->deref(&shutdown_xs);
                shutdown_xs.clear();
                xsink->raiseException("HTTPCLIENT-SHUTDOWN",
                    "connection manager is shutting down");
                return nullptr;
            }
            conn_raw->setPoolKey(key);
            pool_[key].push_back(conn_raw);
            // Record the observed protocol in the sticky bitmask — see
            // hasEverObservedProtocol() docs for the race this guards
            // against (H2 eviction between response arrival and the
            // test's isHttp2Active() check).
            observed_protocols_.fetch_or(
                1u << static_cast<unsigned>(conn_raw->getProtocol()),
                std::memory_order_release);
            reserved = conn_raw->tryReserveStream();
        }

        // The newly-created connection has 0 active streams and 0
        // pending — tryReserveStream cannot fail unless max_streams=0
        // and we just hit some weird ordering edge.  Validate
        // defensively.
        if (!reserved) {
            xsink->raiseException("HTTPCLIENT-CAPACITY-ERROR",
                "freshly-created connection unexpectedly at capacity");
            return nullptr;
        }

        return conn_raw;
    }
}

// ============================================================
// createConnection (H1 only in P3)
// ============================================================

HttpClientConnectionBase* HttpClientConnectionManagerBase::createConnection(
        const std::string& /*key*/, const char* host, int port,
        bool ssl_required, ExceptionSink* xsink, bool wait_for_ready) {
    // Phase P3: H1 only.  Phase P4 added H2 dispatch.  Phase P5 will
    // add H3 (Http3ClientConnection) here.
    // Pass `this` as the manager so the back-pointer is set BEFORE the
    // constructor submits to the I/O controller.  This eliminates the race
    // window where the I/O thread fires onClosedHook before setManager.
    HttpClientConnectionBase* conn = nullptr;
    switch (opts_.protocol) {
        case HttpClientProtocol::H1: {
            Http1SslConfig ssl_cfg;
            ssl_cfg.verify_mode = opts_.ssl_verify_mode;
            ssl_cfg.accept_all = opts_.accept_all_certs;
            ssl_cfg.cert = opts_.client_cert;
            ssl_cfg.key = opts_.client_key;
            if (proxy_info_) {
                conn = new Http1ClientConnection(host, port, ssl_required,
                    proxy_info_->host.c_str(), proxy_info_->port,
                    xsink, this, ssl_cfg);
            } else {
                conn = new Http1ClientConnection(host, port, ssl_required,
                    xsink, this, ssl_cfg);
            }
            break;
        }
        case HttpClientProtocol::H2:
            if (proxy_info_ && ssl_required) {
                // H2 through an HTTP proxy: use the NEGOTIATE path which
                // creates an H1 CONNECT tunnel, does SSL+ALPN inside, and
                // adopts the result into an H2 connection.  Fall through
                // to the NEGOTIATE case.
                goto negotiate_case;
            }
            conn = new Http2ClientConnection(host, port, ssl_required,
                opts_.max_streams_per_connection, xsink, this);
            break;
        case HttpClientProtocol::H3:
            if (proxy_info_) {
                xsink->raiseException("HTTPCLIENT-PROXY-ERROR",
                    "HTTP/3 (QUIC) connections cannot use HTTP proxies");
                return nullptr;
            }
            conn = new Http3ClientConnection(host, port,
                opts_.max_streams_per_connection, xsink, this,
                opts_.ssl_verify_mode, opts_.accept_all_certs,
                opts_.client_cert, opts_.client_key);
            break;
        case HttpClientProtocol::NEGOTIATE:
        negotiate_case: {
            // Per-connect ALPN negotiation.  Two paths:
            //
            // 1. Direct (no proxy): runs NegotiatingHttpClientConnection
            //    which does TCP connect + TLS handshake with ALPN offer
            //    {"h2","http/1.1"}, then adopts into H1 or H2.
            //
            // 2. Proxy: creates an H1 connection with proxy_tunnel=true
            //    and ALPN configured.  The H1 poll op handles CONNECT +
            //    SSL upgrade inside the tunnel.  After READY, reads ALPN;
            //    if h2, extracts the socket and adopts into H2.
            //
            // NEGOTIATE requires SSL; plain-HTTP AUTO is always H1 at
            // the HTTPClient level and never reaches this case.
            if (!ssl_required) {
                xsink->raiseException("HTTPCLIENT-NEGOTIATE-SSL-REQUIRED",
                    "HttpClientProtocol::NEGOTIATE requires SSL (ALPN "
                    "only runs over TLS)");
                return nullptr;
            }

            Http1SslConfig ssl_cfg;
            ssl_cfg.verify_mode = opts_.ssl_verify_mode;
            ssl_cfg.accept_all = opts_.accept_all_certs;
            ssl_cfg.cert = opts_.client_cert;
            ssl_cfg.key = opts_.client_key;

            if (proxy_info_) {
                // Enable ALPN on the H1 socket so the SSL handshake
                // inside the CONNECT tunnel advertises h2+http/1.1.
                ssl_cfg.negotiate_alpn = true;

                // Proxy path: H1 CONNECT tunnel + SSL + ALPN check.
                // The H1 connection's socket is configured with ALPN
                // {"h2","http/1.1"} via the SSL config — setAlpnProtocols
                // is called in buildAndSubmit before the connect op starts.
                //
                // After the CONNECT tunnel + SSL upgrade completes (H1
                // connection reaches READY), we read the negotiated ALPN
                // from the socket:
                // - "h2" → extract socket, adopt into Http2ClientConnection
                // - "http/1.1" or empty → keep the H1 connection
                ReferenceHolder<Http1ClientConnection> h1(
                    new Http1ClientConnection(host, port, ssl_required,
                        proxy_info_->host.c_str(), proxy_info_->port,
                        xsink, this, ssl_cfg),
                    xsink);
                if (*xsink) {
                    return nullptr;
                }

                bool ready = h1->waitForReadyOrError(
                    opts_.connect_timeout_ms, xsink);
                if (!ready || *xsink) {
                    if (!*xsink) {
                        qore_async_io_log(QORE_LOG_LEVEL_WARN,
                            "negotiate-timeout-proxy target='%s:%d' "
                            "timeout_ms=%d",
                            host, port, opts_.connect_timeout_ms);
                        xsink->raiseException("HTTPCLIENT-NEGOTIATE-TIMEOUT",
                            "ALPN negotiation through proxy to %s:%d "
                            "timed out after %d ms",
                            host, port, opts_.connect_timeout_ms);
                    }
                    ExceptionSink dx;
                    h1->setManager(nullptr);
                    h1->closeConnection(&dx);
                    dx.clear();
                    return nullptr;
                }

                // Read the ALPN result from the tunneled+SSL socket.
                QoreSocketObject* h1_sock = h1->getSocketPriv();
                SimpleRefHolder<QoreStringNode> alpn(
                    h1_sock ? h1_sock->getAlpnProtocol() : nullptr);
                std::string alpn_id;
                if (alpn && !alpn->empty()) {
                    alpn_id = alpn->c_str();
                }

                if (alpn_id == "h2") {
                    // Protocol escalation: extract socket, adopt into H2.
                    QoreObject* adopted_obj = nullptr;
                    QoreSocketObject* adopted_priv = nullptr;
                    h1->setManager(nullptr);
                    if (h1->takeSocket(adopted_obj, adopted_priv, xsink)) {
                        return nullptr;
                    }
                    conn = new Http2ClientConnection(adopted_obj,
                        adopted_priv, host, port,
                        opts_.max_streams_per_connection, xsink, this);
                    if (*xsink) {
                        if (conn) {
                            conn->deref(xsink);
                        }
                        return nullptr;
                    }
                } else {
                    // H1 (or no ALPN) — keep the H1 connection.
                    conn = h1.release();
                }
                break;
            }

            // Direct path (no proxy): use NegotiatingHttpClientConnection.
            ReferenceHolder<NegotiatingHttpClientConnection> neg(
                new NegotiatingHttpClientConnection(host, port, ssl_cfg, xsink),
                xsink);
            if (*xsink) {
                return nullptr;
            }

            // Block on the negotiation handshake regardless of the
            // caller's wait_for_ready setting — we cannot construct
            // the concrete H1/H2 adopt-socket connection until ALPN
            // has been decided.  The resulting concrete connection
            // will be returned in READY state, which satisfies the
            // acquireConnectionAsync contract's "caller waits on
            // CONNECTING via registerReadyNotifier" because there
            // will be nothing to wait for.  True async takeover
            // (returning a transitional NegotiatingHttpClientConnection
            // that reports CONNECTING and later morphs into the
            // concrete) is a later enhancement — blocking here keeps
            // the phase 5 bypass removal atomic.
            bool ready = neg->waitForReadyOrError(opts_.connect_timeout_ms, xsink);
            if (!ready || *xsink) {
                if (!*xsink) {
                    // Diagnostic: surface the inner negotiate poll op's
                    // current state and elapsed-since-submit so the
                    // qorus-core log shows which step the state machine
                    // was stuck in when the timeout fired.  See
                    // /tmp/httpclient-negotiate-timeout-investigation.md
                    // for the audit that motivated this instrumentation.
                    int us;
                    int64_t now_us = q_epoch_us(us) * 1000000LL + us;
                    int64_t submit_us = neg->getNegSubmitTimeUs();
                    int64_t elapsed_us = submit_us > 0 ? (now_us - submit_us) : -1;
                    const char* neg_state = neg->getNegStateName();
                    qore_async_io_log(QORE_LOG_LEVEL_WARN,
                        "negotiate-timeout target='%s:%d' state='%s' "
                        "elapsed_us=%lld timeout_ms=%d",
                        host, port, neg_state, (long long)elapsed_us,
                        opts_.connect_timeout_ms);
                    xsink->raiseException("HTTPCLIENT-NEGOTIATE-TIMEOUT",
                        "ALPN negotiation with %s:%d timed out after %d ms "
                        "(neg state: %s)",
                        host, port, opts_.connect_timeout_ms, neg_state);
                }
                ExceptionSink dx;
                neg->closeConnection(&dx);
                dx.clear();
                return nullptr;
            }

            // Hand off the adopted socket to the concrete H1 or H2
            // connection.  The adopt-socket ctor submits the socket
            // back to the AsyncIoController under its own poll op.
            conn = neg->takeOver(opts_.max_streams_per_connection, this, xsink);
            if (*xsink) {
                return nullptr;
            }
            // The neg helper is taken over; its destructor (on
            // ReferenceHolder scope exit) will not double-close the
            // socket.
            break;
        }
    }
    if (*xsink) {
        if (conn) {
            ExceptionSink dx;
            conn->deref(&dx);
            dx.clear();
        }
        return nullptr;
    }

    // If the caller wants a synchronous, already-READY connection (the
    // historical behavior), wait here.  Otherwise return immediately —
    // the connection is already submitted to the I/O controller and the
    // caller will wait asynchronously (typically via
    // AbstractHttpPollConnectionPriv::registerReadyNotifier).
    if (wait_for_ready) {
        bool ready = conn->waitForReadyOrError(opts_.connect_timeout_ms, xsink);
        if (!ready || *xsink) {
            if (!*xsink) {
                xsink->raiseException("HTTPCLIENT-CONNECT-ERROR",
                    "connection to %s:%d timed out after %d ms",
                    host, port, opts_.connect_timeout_ms);
            }
            conn->setManager(nullptr);
            ExceptionSink dx;
            conn->closeConnection(&dx);
            conn->deref(&dx);
            dx.clear();
            return nullptr;
        }
    }

    return conn;
}

// ============================================================
// releaseConnection / closeAndEvict / closeAll
// ============================================================

void HttpClientConnectionManagerBase::releaseConnection(HttpClientConnectionBase* conn) {
    if (!conn) {
        return;
    }
    // Decrement the pending stream reservation.  The active stream count
    // (tracked inside the poll op) is decremented separately when the
    // request completes.
    conn->releaseStreamReservation();
}

void HttpClientConnectionManagerBase::closeAndEvict(HttpClientConnectionBase* conn,
        ExceptionSink* xsink) {
    if (!conn) {
        return;
    }

    // Find and remove from pool using the stashed pool key for O(1) lookup.
    {
        std::unique_lock<std::shared_mutex> wl(pool_lock_);
        const std::string& key = conn->getPoolKey();
        if (key.empty()) {
            return;
        }
        auto it = pool_.find(key);
        if (it == pool_.end()) {
            return;
        }
        auto& conns = it->second;
        for (auto cit = conns.begin(); cit != conns.end(); ++cit) {
            if (*cit == conn) {
                conns.erase(cit);
                if (conns.empty()) {
                    pool_.erase(it);
                }
                goto found;
            }
        }
        // Not found in this key's vector — already evicted.
        return;
    }
found:
    // Null the manager back-pointer so the impending closeConnection's
    // setClosed → onClosedHook does NOT call back into us (we already
    // removed it from the pool — the callback would be a no-op anyway,
    // but cleanly breaks the back-pointer).  setManager lives on the
    // base class so this works generically for H1/H2/H3.
    conn->setManager(nullptr);
    conn->closeConnection(xsink);
    conn->deref(xsink);
}

void HttpClientConnectionManagerBase::closeAll(ExceptionSink* xsink) {
    // Drain the pool into a local vector under the write lock, then
    // process each connection without the lock held.  This avoids
    // taking onclose_lock while holding pool_lock (which would invert
    // the lock order from the I/O-thread close-hook path).
    std::vector<HttpClientConnectionBase*> drained;
    {
        std::unique_lock<std::shared_mutex> wl(pool_lock_);
        if (shutdown_ && pool_.empty() && deferred_deref_.empty()) {
            return;
        }
        shutdown_ = true;
        for (auto& kv : pool_) {
            for (HttpClientConnectionBase* conn : kv.second) {
                drained.push_back(conn);
            }
        }
        pool_.clear();
        // Also drain any deferred derefs
        for (auto* conn : deferred_deref_) {
            drained.push_back(conn);
        }
        deferred_deref_.clear();
    }

    // Wake any thread waiting in acquireConnection's create_cond_ wait.
    {
        std::lock_guard<std::mutex> cl(create_lock_);
        creating_.clear();
        create_cond_.notify_all();
    }

    // For each drained connection: null the manager back-pointer
    // (lifetime contract section 7.3), close, and deref.  setManager
    // lives on the base class so this works generically.
    for (HttpClientConnectionBase* conn : drained) {
        conn->setManager(nullptr);
        ExceptionSink local_xs;
        conn->closeConnection(&local_xs);
        local_xs.clear();
        conn->deref(xsink);
    }
}

bool HttpClientConnectionManagerBase::hasProtocolInPool(HttpClientProtocol proto) const {
    std::shared_lock<std::shared_mutex> rl(pool_lock_);
    for (const auto& kv : pool_) {
        for (const auto* conn : kv.second) {
            if (conn->getProtocol() == proto) {
                return true;
            }
        }
    }
    return false;
}

bool HttpClientConnectionManagerBase::hasEverObservedProtocol(
        HttpClientProtocol proto) const {
    const unsigned bit = 1u << static_cast<unsigned>(proto);
    return (observed_protocols_.load(std::memory_order_acquire) & bit) != 0;
}

int HttpClientConnectionManagerBase::getPoolSize() const {
    std::shared_lock<std::shared_mutex> rl(pool_lock_);
    int total = 0;
    for (const auto& kv : pool_) {
        total += (int)kv.second.size();
    }
    return total;
}

int HttpClientConnectionManagerBase::getConnectionCount(const char* host, int port) const {
    std::string key = poolKey(host, port);
    std::shared_lock<std::shared_mutex> rl(pool_lock_);
    auto it = pool_.find(key);
    return (it == pool_.end()) ? 0 : (int)it->second.size();
}

// ============================================================
// processDeferredDeref — drain deferred connection derefs
// ============================================================

void HttpClientConnectionManagerBase::processDeferredDeref(ExceptionSink* xsink) {
    std::vector<HttpClientConnectionBase*> to_deref;
    {
        std::unique_lock<std::shared_mutex> wl(pool_lock_);
        to_deref.swap(deferred_deref_);
    }
    for (auto* conn : to_deref) {
        conn->deref(xsink);
    }
}

// ============================================================
// onConnectionClosed (called from connection's onClosedHook)
// ============================================================

void HttpClientConnectionManagerBase::onConnectionClosed(HttpClientConnectionBase* conn) {
    if (!conn) {
        return;
    }
    // Remove the connection from its pool entry using the stashed key
    // for O(1) map lookup (the connection within the vector is still
    // a linear scan, but vectors are tiny — typically 1-3 entries per key).
    bool removed = false;
    {
        std::unique_lock<std::shared_mutex> wl(pool_lock_);
        const std::string& key = conn->getPoolKey();
        if (!key.empty()) {
            auto it = pool_.find(key);
            if (it != pool_.end()) {
                auto& conns = it->second;
                for (auto cit = conns.begin(); cit != conns.end(); ++cit) {
                    if (*cit == conn) {
                        conns.erase(cit);
                        if (conns.empty()) {
                            pool_.erase(it);
                        }
                        removed = true;
                        break;
                    }
                }
            }
        }
    }
    // Drop the pool's ref on the connection.  This callback can run on
    // the I/O thread (from continuePoll → setClosed → onClosedHook).
    // If the deref triggers destruction, the connection destructor calls
    // closeConnection → controller cancel on the poll op that is
    // currently mid-continuePoll — a use-after-free / deadlock (cancel
    // waits for the I/O thread, but we ARE the I/O thread).
    //
    // Stash the pointer and deref on a background thread so any
    // destruction runs off the I/O thread.
    if (removed) {
        deferred_deref_.push_back(conn);
    }
    // Wake any thread waiting in create_cond_ for this key — we don't
    // know the key, so notify all.  Acceptable: spurious wakeups just
    // re-check the pool.
    {
        std::lock_guard<std::mutex> cl(create_lock_);
        create_cond_.notify_all();
    }
}

// ============================================================
// request convenience method
// ============================================================

QoreHashNode* HttpClientConnectionManagerBase::request(const char* method,
        const char* scheme, const char* host, int port, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        int timeout_ms, ExceptionSink* xsink) {
    HttpClientConnectionBase* conn = acquireConnection(scheme, host, port, xsink);
    if (!conn || *xsink) {
        return nullptr;
    }
    // Hold a strong ref for the duration of request().  The pool holds its
    // own ref; the I/O thread may fire onConnectionClosed → deref at any
    // time (even during our blocking Future wait), so without this extra
    // ref the connection can be freed under us.
    conn->ref();
    // RAII deref — fires on every return path below.
    ReferenceHolder<HttpClientConnectionBase> conn_holder(conn, xsink);

    ReferenceHolder<QoreHashNode> submit_result(
        conn->submitRequest(method, path, headers, body, body_len, xsink), xsink);
    if (!submit_result || *xsink) {
        // submitRequest failed — release the reservation we made via
        // acquireConnection's tryReserveStream.
        releaseConnection(conn);
        return nullptr;
    }

    // Extract the future from the result hash and block on it.
    QoreValue future_v = submit_result->getKeyValue("future");
    if (future_v.getType() != NT_OBJECT) {
        xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
            "submitRequest result missing 'future' key");
        releaseConnection(conn);
        return nullptr;
    }
    QoreObject* future_obj = const_cast<QoreObject*>(future_v.get<const QoreObject>());
    future_obj->ref();

    int effective_timeout = timeout_ms > 0 ? timeout_ms : opts_.request_timeout_ms;
    QoreValue result = q_future_get_blocking(future_obj,
        effective_timeout, xsink);
    future_obj->deref(xsink);

    // The PromiseAction has already cleared the active stream count
    // inside the poll op when it ran on the I/O thread.  We don't
    // need to releaseConnection because submitRequest already
    // decremented our pending reservation.

    if (*xsink) {
        result.discard(xsink);
        return nullptr;
    }
    if (result.getType() != NT_HASH) {
        result.discard(xsink);
        xsink->raiseException("HTTPCLIENT-INTERNAL-ERROR",
            "Future returned non-hash result type %d", (int)result.getType());
        return nullptr;
    }
    // The future returns result.refSelf() — a new ref on the SAME hash
    // instance.  If the hash has multiple refs (e.g., the promise still
    // holds one), setKeyValue triggers the uniqueness assertion.  Copy
    // the hash to ensure we have sole ownership before mutating.
    QoreHashNode* rv = result.get<QoreHashNode>();
    if (!rv->is_unique()) {
        QoreHashNode* copy = rv->copy();
        rv->deref(xsink);
        rv = copy;
    }
    // Stamp the response with the actual protocol of the connection that
    // served it.  The C++ multiplex ops (H1/H2/H3) don't set this — the
    // Qore-level stream handles do, but the C++ conn_mgr path doesn't go
    // through Qore.  Callers like send_internal_conn_mgr rely on these
    // fields for isHttp2Active refresh and response-uri generation.
    switch (conn->getProtocol()) {
        case HttpClientProtocol::H2:
            rv->setKeyValue("protocol", new QoreStringNode("h2"), xsink);
            rv->setKeyValue("http_version", new QoreStringNode("2"), xsink);
            break;
        case HttpClientProtocol::H3:
            rv->setKeyValue("protocol", new QoreStringNode("h3"), xsink);
            rv->setKeyValue("http_version", new QoreStringNode("3"), xsink);
            break;
        default:
            rv->setKeyValue("protocol", new QoreStringNode("h1"), xsink);
            break;
    }
    // H2/H3 don't carry a reason phrase; derive it from the status code
    // so the legacy shape has a status_message field.
    if (!rv->getKeyValue("status_message").getType()) {
        int code = (int)rv->getKeyValue("status_code").getAsBigInt();
        if (code > 0) {
            const char* msg = QoreHttpClientObject::getHttpStatusMessage(code);
            rv->setKeyValue("status_message",
                new QoreStringNode(msg), xsink);
        }
    }
    return rv;
}

int64_t HttpClientConnectionManagerBase::requestStreaming(const char* method,
        const char* scheme, const char* host, int port, const char* path,
        const QoreHashNode* headers, const void* body, size_t body_len,
        QoreChannel*& channel_out, ExceptionSink* xsink) {
    HttpClientConnectionBase* conn = acquireConnection(scheme, host, port, xsink);
    if (!conn || *xsink) {
        return -1;
    }

    int64_t stream_id = conn->submitRequestStreaming(method, path, headers,
        body, body_len, channel_out, xsink);
    if (*xsink || stream_id < 0) {
        releaseConnection(conn);
        return -1;
    }

    // Release the stream reservation — submitRequestStreaming internally
    // calls releaseStreamReservation on success (matching the non-streaming
    // request() path).  The connection itself stays in the pool; the
    // active stream count tracks in-flight work independently.
    releaseConnection(conn);
    return stream_id;
}
