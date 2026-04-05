# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientConnectionManager.qc Unified connection pool with protocol auto-selection

/** HttpClientConnectionManager.qc Copyright 2026 Qore Technologies, s.r.o.

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
*/

#! Main namespace
public namespace HttpClientIo {

#! Unified connection pool manager with protocol auto-selection
/** This class manages a pool of HTTP/1.1, HTTP/2, and HTTP/3 connections, providing:

    - **Protocol auto-selection**: Start with HTTP/2, upgrade to HTTP/3 via Alt-Svc discovery
    - **Connection pooling**: Reuse connections across requests
    - **Load balancing**: Distribute streams across connections
    - **Automatic connection creation**: Create new connections as needed
    - **Idle timeout handling**: Close idle connections
    - **GOAWAY handling**: Graceful connection shutdown
    - **Connection migration**: QUIC connection migration for HTTP/3

    @par Basic Usage (auto protocol selection)
    @code{.py}
%requires HttpClientIo

HttpClientConnectionManager manager();
hash<HttpClientResponseInfo> resp = manager.request(
    "https://example.com", "GET", "/api/resource");
printf("Status: %d, Protocol: %s\n", resp.status_code, resp.protocol);
    @endcode

    @par Forced Protocol Selection
    @code{.py}
# Force HTTP/2 only
auto mgr_h2 = new HttpClientConnectionManager({"protocol": "h2"});

# Force HTTP/3 only (HTTPS required)
auto mgr_h3 = new HttpClientConnectionManager({"protocol": "h3"});
    @endcode

    @par Shared Global Controller
    All managers share the process-wide AsyncSocketIo global controller singleton
    (via @ref AsyncSocketIo::getGlobalAsyncIoController()). The controller uses
    \c autostop=True, so its I/O thread starts when the first poll operation is
    submitted and stops when all operations complete. This single-controller
    architecture eliminates cross-controller deadlocks that can occur when object
    destructors fire on one controller's I/O thread and cancel operations on
    another controller.
*/
public class HttpClientConnectionManager {
    public {
        #! Interval before re-probing HTTP/3 for hosts that fell back to HTTP/2
        const H3_REPROBE_INTERVAL = 5m;

        #! Maximum number of entries in the protocol cache before eviction
        const MAX_PROTOCOL_CACHE_SIZE = 1000;

        #! Timeout for the h2c (HTTP/2 prior knowledge) probe on plain HTTP connections
        /** The server typically responds within milliseconds — either with a valid
            HTTP/2 SETTINGS frame or an HTTP/1.1 error response.  The nghttp2 library
            detects the incompatibility and closes the session, which triggers the
            fallback.  This timeout is a safety net for servers that don't respond.
        */
        const H2C_PROBE_TIMEOUT = 1s;

        #! Maximum retry count for connection and capacity errors in request() and submitRequestAsync()
        const MaxRequestRetries = 3;
    }

    private {
        #! Options for this manager
        hash<HttpClientConnectionManagerOptions> opts;

        #! Connection pool: "host:port" -> list of connections
        hash<string, list<HttpClientConnection>> pool;

        #! Lock for thread-safe access to pool
        RWLock pool_lock();

        #! AsyncSocketIoController for async operations
        AsyncSocketIo::AsyncSocketIoController controller;


        #! Logger
        *LoggerInterface logger;

        #! Parsed proxy info (host, port, protocol, ssl)
        *hash<auto> proxy_info;

        #! Shutdown flag
        bool shutdown = False;

        #! Mutex + condition to serialize connection creation per pool key
        Mutex create_mutex();
        Condition create_cond();
        hash<string, bool> creating;

        #! Protocol cache: "host:port" -> "h2" or "h3"
        hash<string, string> protocol_cache;

        #! Last H3 probe times: "host:port" -> timestamp
        hash<string, date> h3_probe_times;

        #! Pending Alt-Svc discoveries from async callbacks
        /** Thread-safe unlimited Queue: the I/O thread pushes {url, alt_svc}
            entries here (non-blocking because the queue has no size limit);
            user threads drain and process them in acquireStream() before pool
            lookup.  This avoids acquiring pool_lock from the I/O thread.
        */
        Queue pending_alt_svc();

        #! Counter tracking in-flight I/O callbacks
        /** Incremented before submitting a poll operation, decremented when
            the callback completes (or on submission failure).  closeAll()
            waits for this counter to reach zero so that the destructor does
            not destroy pool_lock while a callback still holds it.
        */
        Counter callback_counter();
    }

    #! Creates a new connection manager with the given options
    /** @param opts configuration options including protocol selection
    */
    constructor(*hash<HttpClientConnectionManagerOptions> opts) {
        self.opts = opts ?? <HttpClientConnectionManagerOptions>{};
        self.logger = self.opts.logger;
        if (!HttpClientProtocolMap.hasKey(self.opts.protocol)) {
            throw "HTTPCLIENT-OPTION-ERROR",
                sprintf("invalid protocol %y: must be one of %y",
                    self.opts.protocol, keys HttpClientProtocolMap);
        }
        if (self.opts.max_connections_per_host < 0) {
            throw "HTTPCLIENT-OPTION-ERROR",
                sprintf("max_connections_per_host must be >= 0 (0 = unlimited), got %d",
                    self.opts.max_connections_per_host);
        }
        if (self.opts.max_streams_per_connection < 0) {
            throw "HTTPCLIENT-OPTION-ERROR",
                sprintf("max_streams_per_connection must be >= 0 (0 = unlimited), got %d",
                    self.opts.max_streams_per_connection);
        }
        # Parse proxy URL if provided
        if (self.opts.proxy_url.val()) {
            hash<auto> purl;
            try {
                purl = parse_url(self.opts.proxy_url);
            } catch (hash<ExceptionInfo> ex) {
                throw "HTTPCLIENT-OPTION-ERROR",
                    sprintf("invalid proxy URL %y: %s", self.opts.proxy_url, ex.desc);
            }
            if (!purl.host) {
                throw "HTTPCLIENT-OPTION-ERROR",
                    sprintf("proxy URL %y has no host", self.opts.proxy_url);
            }
            bool proxy_ssl = (purl.protocol == "https");
            proxy_info = {
                "host": purl.host,
                "port": purl.port ?? (proxy_ssl ? 443 : 80),
                "protocol": purl.protocol ?? "http",
                "ssl": proxy_ssl,
            };
        }

        # Use the process-wide global controller by default; all managers share
        # a single I/O thread for efficient async operation
        controller = AsyncSocketIo::getGlobalAsyncIoController();

        log(LoggerLevel::DEBUG, "connection manager created: protocol=%s max_conn=%d max_streams=%d idle=%d",
            self.opts.protocol,
            self.opts.max_connections_per_host,
            self.opts.max_streams_per_connection,
            self.opts.idle_timeout / 1s);
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "HttpClientConnectionManager objects cannot be copied";
    }

    #! Destructor - closes all connections
    destructor() {
        try {
            closeAll();
        } catch () {
            # Destructors must not propagate exceptions
        }
    }

    #! Returns the AsyncSocketIoController
    AsyncSocketIo::AsyncSocketIoController getController() {
        return controller;
    }

    #! Acquires a stream handle for the given URL
    /** The \c url parameter is used only for connection pooling (host:port extraction
        and protocol selection). The URL's path component is \b not used for requests —
        callers must pass the full request path to
        @ref request(), @ref acquireAndStartStreaming(), or
        @ref HttpClientStreamHandle::request() "stream.request()".

        @param url the target URL (e.g., \c "https://example.com"); only the scheme,
        host, and port are used for connection management

        @return a stream handle ready for submitting requests

        @throw HTTPCLIENT-URL-ERROR if the URL is invalid
        @throw HTTPCLIENT-CAPACITY-ERROR if no connection can be acquired
    */
    HttpClientStreamHandle acquireStream(string url) {
        # Process any Alt-Svc discoveries from async callbacks before
        # resolving the protocol — ensures async-discovered H3 capability
        # takes effect on the next connection
        drainPendingAltSvc();

        hash<UrlInfo> url_info;
        try {
            url_info = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "HTTPCLIENT-URL-ERROR", sprintf("invalid URL %y: %s", url, ex.desc);
        }
        if (!url_info.host) {
            throw "HTTPCLIENT-URL-ERROR", sprintf("invalid URL: %y (missing host)", url);
        }
        if (!url_info.protocol) {
            url_info.protocol = "https";
        }
        if (!url_info.port) {
            url_info.port = url_info.protocol == "https" ? 443 : 80;
        }

        bool from_pool = False;
        HttpClientConnection conn = getConnection(url_info, \from_pool);

        # Create protocol-appropriate stream handle; release the reservation
        # if the constructor throws to avoid leaking a pending slot
        HttpClientStreamHandle stream;
        try {
            if (conn instanceof Http1ClientConnection) {
                stream = new Http1ClientStreamHandle(cast<Http1ClientConnection>(conn),
                    opts.request_timeout, logger);
            } else if (conn instanceof Http2ClientConnection) {
                stream = new Http2ClientStreamHandle(cast<Http2ClientConnection>(conn),
                    opts.request_timeout, logger);
            } else {
                stream = new Http3ClientStreamHandle(conn, opts.request_timeout, logger);
            }
        } catch () {
            conn.releaseStreamReservation();
            rethrow;
        }
        stream.markReservationHeld();
        if (from_pool) {
            stream.markConnectionReused();
        }
        return stream;
    }

    #! Acquires a stream and starts streaming with automatic retry on stale connections
    /** If the server closed the connection between acquisition and streaming start
        (a race inherent in pooled HTTP/1.1 connections), this method retries with a
        fresh connection up to \c MaxRequestRetries times.

        @param url the target URL; only scheme, host, and port are used for connection
        management — the URL's path is \b not prepended to \c path
        @param method HTTP method (e.g., "POST")
        @param path the full request path to send (must include any base path from the URL)
        @param headers request headers (optional)
        @param body_streaming if True, enables bidirectional streaming (HTTP/2+ only)
        @param body request body (optional)

        @return the stream handle ready for reading

        @throw HTTPCLIENT-CONNECT-ERROR if the connection cannot be established
        @throw HTTP1-STREAMING-ERROR if body_streaming is True on HTTP/1.1

        @note If the request times out on a pooled connection, the connection is
        evicted and the request is retried once on a fresh connection, handling
        stale connections transparently.
    */
    HttpClientStreamHandle acquireAndStartStreaming(string url, string method, string path,
            *hash<auto> headers, bool body_streaming = False, *data body) {
        int retries = 0;
        bool timeout_retried = False;
        while (True) {
            HttpClientStreamHandle stream = acquireStream(url);
            try {
                stream.startStreaming(method, path, headers, body_streaming, body);
                return stream;
            } catch (hash<ExceptionInfo> ex) {
                if (isConnectionError(ex.err) && ++retries <= MaxRequestRetries) {
                    log(LoggerLevel::DEBUG, "streaming connection error %s, retrying (%d/%d)",
                        ex.err, retries, MaxRequestRetries);
                    evictDeadConnections(url);
                    continue;
                }
                # Only retry timeout on reused pooled connections (stale connection)
                if (ex.err == "HTTPCLIENT-TIMEOUT"
                        && stream.wasConnectionReused() && !timeout_retried) {
                    timeout_retried = True;
                    HttpClientConnection timed_out_conn = stream.getConnection();
                    log(LoggerLevel::DEBUG, "streaming timed out on pooled %s connection %d, "
                        "evicting stale connection and retrying",
                        timed_out_conn.getProtocol(), timed_out_conn.getConnectionId());
                    try { timed_out_conn.close(); } catch () {}
                    evictDeadConnections(url);
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Submits an async request and returns immediately
    /** @param url the target URL; only scheme, host, and port are used for connection
        management — the URL's path is \b not prepended to \c path
        @param method HTTP method
        @param path the full request path to send (must include any base path from the URL)
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback callback invoked with response or error

        @return the stream handle (can be used to cancel)
    */
    HttpClientStreamHandle submitRequestAsync(string url, string method, string path,
            *hash<auto> headers, *data body, HttpResponseCallback callback) {
        int retries = 0;
        while (True) {
            *HttpClientStreamHandle stream;
            try {
                stream = acquireStream(url);
                # Wrap callback to capture Alt-Svc for deferred processing.
                # The callback runs on the I/O thread, so we must NOT acquire
                # pool_lock or do any blocking work.  Instead, push the
                # alt-svc value to a lock-free queue; drainPendingAltSvc()
                # processes it on the next user-thread call.
                HttpResponseCallback wrapped_cb = sub (hash<auto> resp, *hash<auto> err) {
                    if (!err && resp.headers) {
                        # NOTE: multiple Alt-Svc headers are collected into a list
                        # by Qore's HTTP parser — join them per RFC 7838 §3
                        auto alt_svc_raw = resp.headers."alt-svc"
                            ?? resp.headers."Alt-Svc";
                        *string alt_svc = alt_svc_raw.typeCode() == NT_LIST
                            ? (foldl $1 + ", " + $2, alt_svc_raw)
                            : alt_svc_raw;
                        if (alt_svc) {
                            pending_alt_svc.push({"url": url, "alt_svc": alt_svc});
                        }
                    }
                    callback(resp, err);
                };
                stream.submitAsync(method, path, headers, body, wrapped_cb);
                return stream;
            } catch (hash<ExceptionInfo> ex) {
                # NOTE: no manual reservation release needed here — submitAsync()
                # calls releaseReservationIfHeld() on all pre-submit failure paths
                # (waitForReady timeout, cancel, submitAsyncImpl exception), and
                # the destructor is a final safety net
                if ((isCapacityError(ex.err) || isConnectionError(ex.err))
                        && ++retries <= MaxRequestRetries) {
                    if (isConnectionError(ex.err)) {
                        log(LoggerLevel::DEBUG, "connection error %s, evicting and retrying (%d/%d)",
                            ex.err, retries, MaxRequestRetries);
                        evictDeadConnections(url);
                        # h2c failure on plain HTTP → fall back to h1 for this host
                        handleProtocolFallback(url, ex.err);
                    } else {
                        log(LoggerLevel::DEBUG, "capacity error %s (retry %d/3)", ex.err, retries);
                    }
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Performs a synchronous request
    /** @param url the target URL; only scheme, host, and port are used for connection
        management — the URL's path is \b not prepended to \c path
        @param method HTTP method
        @param path the full request path to send (must include any base path from the URL)
        @param headers request headers (optional)
        @param body request body (optional)
        @param timeout_ms request timeout (optional)

        @return the response hash

        @note If the request times out on a \b reused pooled connection, the
        connection is evicted and the request is retried once on a fresh connection
        with \c connect_timeout as the retry deadline. This handles stale connections
        (e.g., server restart, QUIC session death) where no transport-level error is
        detected before the timeout. The retry uses \c connect_timeout (not
        \c request_timeout) because if the server genuinely restarted, the fresh
        connection establishes quickly; if it is unreachable, the retry fails within
        \c connect_timeout rather than burning a full \c request_timeout. Timeouts on
        freshly created connections are not retried — they indicate a genuinely slow
        or unreachable server.

        @throw HTTPCLIENT-TIMEOUT if the request times out on the retry as well
    */
    hash<auto> request(string url, string method, string path, *hash<auto> headers,
            *data body, *timeout timeout_ms) {
        int retries = 0;
        bool timeout_retried = False;
        while (True) {
            *HttpClientStreamHandle stream;
            try {
                stream = acquireStream(url);
                hash<auto> resp = stream.request(method, path, headers, body,
                    timeout_retried ? opts.connect_timeout : timeout_ms);
                checkAltSvc(url, resp);
                return resp;
            } catch (hash<ExceptionInfo> ex) {
                # NOTE: no manual reservation release needed — request() calls
                # submitAsync() internally which releases on all failure paths
                if ((isCapacityError(ex.err) || isConnectionError(ex.err))
                        && ++retries <= MaxRequestRetries) {
                    if (isConnectionError(ex.err)) {
                        log(LoggerLevel::DEBUG, "connection error %s, evicting and retrying (%d/%d)",
                            ex.err, retries, MaxRequestRetries);
                        evictDeadConnections(url);
                        # h2c failure on plain HTTP → fall back to h1 for this host
                        handleProtocolFallback(url, ex.err);
                    } else {
                        log(LoggerLevel::DEBUG, "capacity error %s (retry %d/%d)",
                            ex.err, retries, MaxRequestRetries);
                    }
                    continue;
                }
                # A timeout on a reused pooled connection may indicate a stale
                # connection (server restarted, QUIC session dead). Close the
                # stale connection and retry once with connect_timeout as the
                # deadline — if the server restarted, the fresh connection
                # establishes quickly; if it is genuinely unreachable, we fail
                # within connect_timeout rather than burning another full
                # request_timeout.  Do NOT retry for fresh connections — a
                # timeout on a new connection means the server is genuinely
                # slow or unreachable.
                if (ex.err == "HTTPCLIENT-TIMEOUT" && stream
                        && stream.wasConnectionReused() && !timeout_retried) {
                    timeout_retried = True;
                    HttpClientConnection timed_out_conn = stream.getConnection();
                    log(LoggerLevel::DEBUG, "request timed out on pooled %s connection %d, "
                        "evicting stale connection and retrying with connect_timeout=%y",
                        timed_out_conn.getProtocol(), timed_out_conn.getConnectionId(),
                        opts.connect_timeout);
                    try { timed_out_conn.close(); } catch () {}
                    evictDeadConnections(url);
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Returns connection pool statistics
    hash<auto> getStats() {
        AutoReadLock al(pool_lock);

        int total_connections = 0;
        int total_active_streams = 0;
        hash<auto> per_host = {};

        foreach string key in (keys pool) {
            list<HttpClientConnection> conns = pool{key};
            int host_connections = conns.size();
            int host_streams = 0;
            int host_draining = 0;
            foreach HttpClientConnection conn in (conns) {
                host_streams += conn.getActiveStreamCount();
                if (conn.isDraining()) {
                    ++host_draining;
                }
            }
            total_connections += host_connections;
            total_active_streams += host_streams;
            per_host{key} = {
                "connections": host_connections,
                "active_streams": host_streams,
                "draining": host_draining,
            };
        }

        return {
            "total_connections": total_connections,
            "total_active_streams": total_active_streams,
            "hosts": per_host.size(),
            "per_host": per_host,
            "protocol": opts.protocol,
        };
    }

    #! Closes all connections
    closeAll() {
        {
            AutoWriteLock al(pool_lock);
            shutdown = True;
        }

        try {
            controller.cancelByOwner(self.uniqueHash());
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "closeAll: cancelByOwner error: %s: %s", ex.err, ex.desc);
        }

        # Wait for any in-flight I/O callbacks to complete before
        # destroying pool_lock; without this, a callback on the I/O
        # thread can still hold pool_lock when the destructor runs
        callback_counter.waitForZero();

        # Copy the pool and release the lock before closing connections
        hash<string, list<HttpClientConnection>> pool_copy;
        {
            AutoWriteLock al(pool_lock);
            if (pool) {
                pool_copy = pool;
                remove pool;
            }
        }
        foreach string key in (keys pool_copy) {
            foreach HttpClientConnection conn in (pool_copy{key}) {
                try {
                    conn.close();
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::DEBUG, "closeAll: close error for %s: %s: %s",
                        key, ex.err, ex.desc);
                }
            }
        }

    }

    #! Removes idle connections older than the idle timeout
    cleanupIdleConnections() {
        date cutoff = now_us() - opts.idle_timeout;

        list<HttpClientConnection> to_close = ();
        {
            AutoWriteLock al(pool_lock);
            foreach string key in (keys pool) {
                list<HttpClientConnection> conns = pool{key};
                list<HttpClientConnection> keep = ();

                foreach HttpClientConnection conn in (conns) {
                    if (conn.isClosed()) {
                        log(LoggerLevel::DEBUG, "removing closed connection to %s", key);
                        continue;
                    }
                    if (conn.getActiveStreamCount() == 0 && conn.getLastActivity() < cutoff) {
                        log(LoggerLevel::DEBUG, "closing idle connection to %s", key);
                        push to_close, conn;
                        continue;
                    }
                    push keep, conn;
                }

                if (keep.size() > 0) {
                    pool{key} = keep;
                } else {
                    remove pool{key};
                }
            }
        }
        foreach HttpClientConnection conn in (to_close) {
            try {
                conn.close();
            } catch () {
            }
        }
    }

    #! Migrates all ready HTTP/3 connections to new network paths
    /** @return the number of connections successfully migrated
    */
    int migrateAll() {
        list<hash<auto>> to_migrate = ();
        {
            AutoReadLock al(pool_lock);
            foreach string key in (keys pool) {
                foreach HttpClientConnection conn in (pool{key}) {
                    if (conn.isReady()) {
                        push to_migrate, {"key": key, "conn": conn};
                    }
                }
            }
        }

        int migrated = 0;
        foreach hash<auto> item in (to_migrate) {
            try {
                if (item.conn.migrate()) {
                    ++migrated;
                    log(LoggerLevel::DEBUG, "migrated connection %d to %s",
                        item.conn.getConnectionId(), item.key);
                }
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "migration failed for connection %d to %s: %s: %s",
                    item.conn.getConnectionId(), item.key, ex.err, ex.desc);
                try {
                    item.conn.setDraining();
                } catch () {
                }
            }
        }

        log(LoggerLevel::DEBUG, "migrateAll: %d/%d connections migrated", migrated, to_migrate.size());
        return migrated;
    }

    #! Internal: resolves the protocol to use for a given URL
    /** In auto mode: HTTPS starts with HTTP/2 and upgrades to HTTP/3 only when
        an @c Alt-Svc response header advertises H3 support (RFC 7838).  Plain
        HTTP uses HTTP/2 prior knowledge (h2c) first with HTTP/1.1 fallback.
        The protocol cache persists negotiation results to avoid repeated probes.

        @param url_info parsed URL info
        @param key the pool key "host:port"

        @return "h1", "h2", or "h3"
    */
    private string resolveProtocol(hash<auto> url_info, string key) {
        if (opts.protocol == "h1") {
            return "h1";
        }
        if (opts.protocol == "h2") {
            return "h2";
        }
        if (opts.protocol == "h3") {
            if (url_info.protocol != "https") {
                throw "HTTPCLIENT-URL-ERROR",
                    "HTTP/3 requires HTTPS (URL scheme must be https://)";
            }
            if (proxy_info) {
                throw "HTTPCLIENT-PROXY-ERROR",
                    "HTTP/3 (QUIC) cannot tunnel through HTTP proxies; use protocol "
                    "\"auto\" or \"h2\" with a proxy";
            }
            return "h3";
        }

        # Auto mode
        if (url_info.protocol != "https") {
            # For plain HTTP, default to HTTP/1.1 — h2c (HTTP/2 prior knowledge)
            # requires the server to support it, and most HTTP/1.1 servers will
            # reject the HTTP/2 preface.  ALPN negotiation (which reliably detects
            # HTTP/2 support) is only available over TLS.  Users who know their
            # server supports h2c can explicitly set protocol: "h2".
            return "h1";
        }

        # Check cache — "h3" is set by checkAltSvc() when an Alt-Svc response
        # header advertises H3 support; "h2" is set after H3 fallback
        if (protocol_cache.hasKey(key)) {
            string cached = protocol_cache{key};
            if (cached == "h3") {
                # QUIC cannot tunnel through HTTP proxies — downgrade to H2
                if (proxy_info) {
                    return "h2";
                }
                return "h3";
            }
            # Cached as "h2" (after an H3 fallback) — re-probe H3 after
            # H3_REPROBE_INTERVAL if this host previously confirmed H3 support
            if (!proxy_info && h3_probe_times.hasKey(key)) {
                date elapsed = now_us() - h3_probe_times{key};
                if (elapsed >= H3_REPROBE_INTERVAL) {
                    log(LoggerLevel::DEBUG, "re-probing HTTP/3 for %s after %y elapsed", key, elapsed);
                    return "h3";
                }
            }
            return "h2";
        }

        # No cache: start with HTTP/2 and discover H3 capability via Alt-Svc
        return "h2";
    }

    #! Internal: gets or creates a connection for the given URL info
    private HttpClientConnection getConnection(hash<auto> url_info, reference<bool> from_pool) {
        string key = sprintf("%s:%d", url_info.host, url_info.port);

        while (True) {
            # Try to find an existing connection with capacity.
            # Dead connections discovered here are collected and evicted
            # after releasing the read lock — this prevents stale entries
            # from accumulating in the pool.
            {
                list<HttpClientConnection> dead_conns = ();
                *HttpClientConnection live_conn;
                {
                    AutoReadLock al(pool_lock);
                    if (shutdown) {
                        throw "HTTPCLIENT-SHUTDOWN", "connection manager is shutting down";
                    }
                    if (pool.hasKey(key)) {
                        foreach HttpClientConnection conn in (pool{key}) {
                            if (conn.isClosed() || conn.isDraining()) {
                                continue;
                            }
                            if (!isConnectionAlive(conn)) {
                                push dead_conns, conn;
                                continue;
                            }
                            if (!live_conn && conn.tryReserveStream()) {
                                live_conn = conn;
                            }
                        }
                    }
                }
                # Evict dead connections outside the read lock (evictConnections
                # acquires a write lock — doing it under the read lock would deadlock)
                if (dead_conns) {
                    evictConnections(key, dead_conns);
                }
                if (live_conn) {
                    from_pool = True;
                    log(LoggerLevel::DEBUG, "reusing %s connection %d to %s",
                        live_conn.getProtocol(), live_conn.getConnectionId(), key);
                    return live_conn;
                }
            }

            # Serialize connection creation per key to avoid redundant QUIC handshakes.
            # Without this, N threads all finding no reusable connection would each create
            # their own, flooding the server with N handshakes when 1 suffices.
            # The wait is bounded by connect_timeout; on timeout, the waiting thread
            # takes over creation to prevent indefinite blocking when the creator stalls
            # (e.g., stuck in blocking DNS/getaddrinfo).
            {
                AutoLock al(create_mutex);
                if (creating{key}) {
                    int rc = create_cond.wait(create_mutex, opts.connect_timeout);
                    if (rc == ETIMEDOUT) {
                        # Creator stalled — take over creation
                        log(LoggerLevel::DEBUG, "creation wait timed out for %s, taking over", key);
                    } else {
                        # Woken normally — loop back to check the pool
                        continue;
                    }
                }
                creating{key} = True;
            }
            on_exit {
                AutoLock al(create_mutex);
                remove creating{key};
                create_cond.broadcast();
            }

            # We are the designated creator for this key.
            # Resolve protocol and check capacity under the write lock, then release
            # it before creating the connection (constructor may call getaddrinfo()
            # which can block)
            string protocol;
            {
                AutoWriteLock al(pool_lock);
                if (shutdown) {
                    throw "HTTPCLIENT-SHUTDOWN", "connection manager is shutting down";
                }

                # Resolve protocol under WriteLock to ensure thread-safe access to
                # protocol_cache
                protocol = resolveProtocol(url_info, key);

                # Evict dead connections
                if (pool.hasKey(key)) {
                    list<HttpClientConnection> live = ();
                    *HttpClientConnection reusable;
                    foreach HttpClientConnection conn in (pool{key}) {
                        if (conn.isClosed()) {
                            log(LoggerLevel::DEBUG, "evicting closed connection %d to %s",
                                conn.getConnectionId(), key);
                        } else {
                            push live, conn;
                            if (!reusable && !conn.isDraining() && conn.tryReserveStream()) {
                                reusable = conn;
                            }
                        }
                    }
                    if (live.size() != pool{key}.size()) {
                        if (live) {
                            pool{key} = live;
                        } else {
                            remove pool{key};
                        }
                    }
                    if (reusable) {
                        from_pool = True;
                        return reusable;
                    }
                }

                # Check capacity (but don't create connection under lock)
                # Skip cap when max_connections_per_host == 0 (unlimited)
                int current_count = pool.hasKey(key) ? pool{key}.size() : 0;
                if (opts.max_connections_per_host
                        && current_count >= opts.max_connections_per_host) {
                    # Pick connection with fewest streams
                    list<HttpClientConnection> candidates;
                    int min_streams = MAXINT;
                    foreach HttpClientConnection conn in (pool{key}) {
                        int streams = conn.getActiveStreamCount();
                        if (streams < min_streams) {
                            min_streams = streams;
                            candidates = (conn,);
                        } else if (streams == min_streams) {
                            push candidates, conn;
                        }
                    }
                    if (candidates) {
                        HttpClientConnection best = candidates.size() == 1
                            ? candidates[0]
                            : candidates[rand() % candidates.size()];
                        if (best.tryReserveStream()) {
                            from_pool = True;
                            log(LoggerLevel::DEBUG, "using %s connection %d with %d streams "
                                "to %s (at connection limit)", best.getProtocol(),
                                best.getConnectionId(), min_streams, key);
                            return best;
                        }
                    }
                    throw "HTTPCLIENT-CAPACITY-ERROR",
                        sprintf("all %d connections to %s have reached the concurrent "
                            "stream limit", current_count, key);
                }
            }

            # Create connection outside the write lock — constructor may do DNS
            # resolution (getaddrinfo) which can block and would stall all pool
            # operations if done under the lock
            HttpClientConnection conn = createConnection(url_info, key, protocol);

            # For plain HTTP h2c in auto mode: probe BEFORE adding to the pool
            # to prevent other threads from acquiring the H2 connection during
            # the probe window.  The probe connection is submitted to the
            # controller (for I/O driving) but not exposed in the pool.
            bool h2c_probed = False;
            if (protocol == "h2" && url_info.protocol != "https" && opts.protocol == "auto") {
                h2c_probed = True;
                conn.setControllerRef(controller);
                submitConnectionToController(key, conn);

                bool h2c_ok = False;
                try {
                    h2c_ok = conn.waitForReady(H2C_PROBE_TIMEOUT);
                } catch () {
                    # Connection closed during probe (server rejected h2c)
                }
                if (!h2c_ok) {
                    log(LoggerLevel::DEBUG, "h2c probe failed for %s, falling back to HTTP/1.1", key);
                    # Cancel the probe connection from the controller BEFORE closing
                    # the socket to prevent the I/O thread from reading a closed fd
                    try { controller.cancel(conn.getSocket()); } catch () {}
                    try { conn.close(); } catch () {}
                    {
                        AutoWriteLock al(pool_lock);
                        cacheProtocol(key, "h1");
                    }
                    # Create H1 replacement — this one hasn't been submitted yet
                    conn = createConnection(url_info, key, "h1");
                    h2c_probed = False;
                }
            }

            {
                AutoWriteLock al(pool_lock);
                if (shutdown) {
                    try { conn.close(); } catch () {}
                    throw "HTTPCLIENT-SHUTDOWN", "connection manager is shutting down";
                }

                if (!pool.hasKey(key)) {
                    pool{key} = ();
                }
                push pool{key}, conn;

                # Only submit to controller if not already submitted during h2c probe
                if (!h2c_probed) {
                    conn.setControllerRef(controller);
                    submitConnectionToController(key, conn);
                }

                # Reserve a stream slot for the creator thread under the same lock
                # so the pool-scan path (holding only the read lock) cannot grab the
                # sole available slot between pool insertion and reservation
                conn.tryReserveStream();
                return conn;
            }
        }
    }

    #! Internal: creates a connection with protocol fallback
    /** In auto mode, tries H3 first; on failure, falls back to H2 and caches.
    */
    private HttpClientConnection createConnection(hash<auto> url_info, string key, string protocol) {
        if (protocol == "h1") {
            log(LoggerLevel::DEBUG, "creating new HTTP/1.1 connection to %s%s", key,
                proxy_info ? sprintf(" via proxy %s:%d", proxy_info.host, proxy_info.port) : "");
            return new Http1ClientConnection(url_info, opts.connect_timeout, logger,
                1, opts.ssl_verify_mode, opts.accept_all_certs, proxy_info);
        }

        if (protocol == "h3") {
            # Record probe time
            h3_probe_times{key} = now_us();

            try {
                log(LoggerLevel::DEBUG, "creating new HTTP/3 connection to %s", key);
                Http3ClientConnection conn(url_info, opts.connect_timeout, logger,
                    opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
                    opts.client_cert, opts.client_pk);
                cacheProtocol(key, "h3");
                return conn;
            } catch (hash<ExceptionInfo> ex) {
                if (opts.protocol == "h3") {
                    # Forced H3 mode — don't fall back
                    rethrow;
                }
                # Auto mode — fall back to H2
                log(LoggerLevel::DEBUG, "HTTP/3 connection to %s failed (%s: %s), falling back to HTTP/2",
                    key, ex.err, ex.desc);
                cacheProtocol(key, "h2");
            }
        }

        # Create H2 connection
        log(LoggerLevel::DEBUG, "creating new HTTP/2 connection to %s%s", key,
            proxy_info ? sprintf(" via proxy %s:%d", proxy_info.host, proxy_info.port) : "");
        return new Http2ClientConnection(url_info, opts.connect_timeout, logger,
            opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
            proxy_info);
    }

    #! Internal: submits a connection poll operation to the controller
    /** @param key the pool key "host:port"
        @param conn the connection to submit
        @param initial if True, log the initial submission (suppressed on re-submit to reduce noise)
    */
    private submitConnectionToController(string key, HttpClientConnection conn,
            bool initial = True) {
        HttpClientPollOperation op = conn.getPollOperation();
        Socket sock = conn.getSocket();
        string owner = self.uniqueHash();

        if (initial) {
            log(LoggerLevel::DEBUG, "submitting %s connection %d to controller for %s",
                conn.getProtocol(), conn.getConnectionId(), key);
        }

        callback_counter.inc();
        try {
            controller.submit(<SocketPollOperationInfo>{
                "sock": sock,
                "spop": op,
                "owner": owner,
                "to": -1s,
                "callback": sub (hash<SocketPollResultInfo> result) {
                    on_exit callback_counter.dec();
                    handleConnectionResult(key, conn, result);
                },
            });
        } catch () {
            callback_counter.dec();
            rethrow;
        }
    }

    #! Internal: handles connection result from controller callback
    private handleConnectionResult(string key, HttpClientConnection conn,
            hash<SocketPollResultInfo> result) {
        if (result.canceled || shutdown) {
            log(LoggerLevel::DEBUG, "connection %d %s", conn.getConnectionId(),
                result.canceled ? "canceled" : "shutdown");
            return;
        }

        if (result.ex) {
            log(LoggerLevel::ERROR, "connection %d error: %s: %s",
                conn.getConnectionId(), result.ex.err, result.ex.desc);
            # Detect h2c incompatibility on plain HTTP and cache h1 fallback
            if (conn.getProtocol() == "h2" && isH2cIncompatible(result.ex.err)) {
                log(LoggerLevel::DEBUG, "h2c failed for %s (%s), caching h1 fallback",
                    key, result.ex.err);
                AutoWriteLock al(pool_lock);
                cacheProtocol(key, "h1");
            }
            try {
                conn.close();
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "connection %d close after error: %s: %s",
                    conn.getConnectionId(), ex.err, ex.desc);
            }
            removeConnectionFromPool(key, conn);
            return;
        }

        conn.handlePollResult();

        HttpClientPollOperation op = conn.getPollOperation();
        if (op.isClosed() || op.hasError()) {
            # Detect h2c failure and cache h1 fallback
            *hash<auto> op_err = op.getError();
            if (op_err && conn.getProtocol() == "h2" && isH2cIncompatible(op_err.err)) {
                log(LoggerLevel::DEBUG, "h2c failed for %s (%s), caching h1 fallback",
                    key, op_err.err);
                AutoWriteLock al(pool_lock);
                cacheProtocol(key, "h1");
            }
            log(LoggerLevel::DEBUG, "connection %d closed", conn.getConnectionId());
            # Close the connection to notify pending streams — their
            # callbacks fire with an error so threads in f.get() wake up
            # instead of waiting until request_timeout
            try { conn.close(); } catch () {}
            removeConnectionFromPool(key, conn);
            return;
        }

        if (conn.isDraining()) {
            log(LoggerLevel::DEBUG, "connection %d is draining (GOAWAY received)", conn.getConnectionId());
        }

        if (shutdown) {
            try { conn.close(); } catch () {}
            removeConnectionFromPool(key, conn);
            return;
        }

        try {
            op.continueReading();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "connection %d continueReading error: %s: %s",
                conn.getConnectionId(), ex.err, ex.desc);
            try { conn.close(); } catch () {}
            removeConnectionFromPool(key, conn);
            return;
        }
        # Re-submit with initial=False to suppress redundant log line
        submitConnectionToController(key, conn, False);
    }

    #! Internal: removes a connection from the pool
    private removeConnectionFromPool(string key, HttpClientConnection conn) {
        AutoWriteLock al(pool_lock);
        if (!pool.hasKey(key)) {
            return;
        }
        list<HttpClientConnection> conns = pool{key};
        list<HttpClientConnection> keep = ();
        foreach HttpClientConnection c in (conns) {
            if (c.getConnectionId() != conn.getConnectionId()) {
                push keep, c;
            }
        }
        if (keep.size() > 0) {
            pool{key} = keep;
        } else {
            remove pool{key};
        }
    }

    #! Internal: checks if an error indicates a capacity limit
    private bool isCapacityError(string err) {
        return err == "HTTP1-CAPACITY-ERROR"
            || err == "HTTP2-CAPACITY-ERROR"
            || err == "HTTP3-CAPACITY-ERROR"
            || err == "HTTPCLIENT-CAPACITY-ERROR";
    }

    #! Internal: checks if an error indicates a dead connection
    private bool isConnectionError(string err) {
        return isConnectionErrorStatic(err);
    }

    #! Static version of isConnectionError() for use by other classes
    static bool isConnectionErrorStatic(string err) {
        return err == "QUIC-HTTP3-ERROR"
            || err == "QUIC-READ-ERROR"
            || err == "QUIC-RECV-ERROR"
            || err == "HTTP3-CONNECT-ERROR"
            || err == "HTTP3-STATE-ERROR"
            || err == "HTTP3-ABORT"
            || err == "HTTP3-EMPTY-READ"
            || err == "HTTP2-EMPTY-READ"
            || err == "HTTP2-CONNECT-ERROR"
            || err == "HTTP2-ALPN-ERROR"
            || err == "HTTP2-ABORT"
            || err == "HTTP2-ERROR"
            || err == "HTTP1-CONNECT-ERROR"
            || err == "HTTP1-CONNECTION-CLOSED"
            || err == "HTTP1-STATE-ERROR"
            || err == "HTTP1-SSL-ERROR"
            || err == "HTTP1-ABORT"
            || err == "HTTP1-SEND-ERROR"
            || err == "HTTP1-RECV-ERROR"
            || err == "HTTPCLIENT-STATE-ERROR"
            || err == "HTTPCLIENT-CONNECT-ERROR"
            || err == "SOCKET-SEND-ERROR"
            || err == "SOCKET-RECV-ERROR"
            || err == "SOCKET-CLOSED";
    }

    #! Internal: checks if an error indicates an h2c protocol incompatibility
    /** Returns True if the error suggests the server doesn't support HTTP/2
        prior knowledge (h2c) on a plain HTTP connection.
    */
    private bool isH2cIncompatible(string err) {
        return err == "HTTP2-EMPTY-READ"
            || err == "HTTP2-CONNECT-ERROR"
            || err == "HTTP2-ALPN-ERROR"
            || err == "HTTP2-ABORT"
            || err == "HTTP2-ERROR"
            || err == "HTTPCLIENT-CONNECT-ERROR";
    }

    #! Internal: drains pending Alt-Svc discoveries from async callbacks
    /** Called on user threads (from acquireStream) to process Alt-Svc headers
        that were captured by async response callbacks on the I/O thread.
        The I/O thread cannot call checkAltSvc() directly because it acquires
        pool_lock (write), which would block the I/O thread.
    */
    private drainPendingAltSvc() {
        while (True) {
            # Non-blocking get: negative timeout returns NOTHING immediately
            # when the queue is empty (avoids TOCTOU race between size() and get())
            *hash<auto> item;
            try {
                item = pending_alt_svc.get(-1);
            } catch () {
                # QUEUE-TIMEOUT — queue was empty
                break;
            }
            if (!exists item) {
                break;
            }
            checkAltSvc(item.url, {"headers": {"alt-svc": item.alt_svc}});
        }
    }

    #! Internal: checks the @c Alt-Svc response header for HTTP/3 advertisement
    /** When a server response includes an @c alt-svc header with an @c h3= entry,
        this method caches "h3" for the advertised host:port so subsequent connections
        to that endpoint use HTTP/3 (per RFC 7838).

        The advertised host defaults to the origin host when empty; the advertised
        port is parsed from the header value (e.g. @c h3=\":443\").

        @note Acquires pool_lock (write).  Must be called on a user thread only.
        Called from request() (sync path) and drainPendingAltSvc() (async path,
        drains I/O-thread-captured headers on the next user-thread call).

        @param url the origin URL (used for host/port defaults when Alt-Svc omits them)
        @param resp the response hash (checked for @c alt-svc header)
    */
    private checkAltSvc(string url, hash<auto> resp) {
        # Only applicable to HTTPS in auto mode without a proxy
        if (opts.protocol != "auto") {
            return;
        }
        # QUIC cannot tunnel through HTTP proxies — ignore Alt-Svc when proxied
        if (proxy_info) {
            return;
        }
        # HTTP/2 headers are lowercase; HTTP/1.1 headers preserve case — check both.
        # Check for the header before parse_url() to avoid the parse on the common
        # case where no Alt-Svc header is present.
        # NOTE: multiple Alt-Svc headers are collected into a list by Qore's HTTP
        # parser — join them with commas per RFC 7838 §3.
        auto alt_svc_raw = resp.headers."alt-svc" ?? resp.headers."Alt-Svc";
        *string alt_svc = alt_svc_raw.typeCode() == NT_LIST
            ? (foldl $1 + ", " + $2, alt_svc_raw)
            : alt_svc_raw;
        if (!alt_svc) {
            return;
        }
        hash<UrlInfo> url_info = parse_url(url);
        if (url_info.protocol != "https") {
            return;
        }
        if (!url_info.port) {
            url_info.port = 443;
        }
        # Normalize the origin host: parse_url() strips brackets from IPv6
        # (e.g. "[::1]" → "::1"), so the pool key uses the bare form.
        string origin_host = url_info.host;
        # Parse comma-separated Alt-Svc alternatives, e.g.:
        #   h3=":443"; ma=2592000, h3-29=":443"; ma=2592000
        # RFC 7838 §3: alt-authority = [host] ":" port
        # Per RFC 7838 §2.1, the alt-authority host MUST be within the origin's
        # authority scope.  We only honour Alt-Svc when the advertised host is
        # empty (same origin) or matches the origin host — cross-host alternatives
        # are ignored to prevent cache-poisoning and connection-pool confusion.
        foreach string entry in (alt_svc.split(",")) {
            entry = trim(entry);
            # Match h3="[host]:port" — host may be empty, a hostname, an IPv4
            # address, or a bracketed IPv6 address (e.g. h3="[::1]:443")
            *list<*string> m = (entry =~ x/^h3="(\[[^\]]*\]|[^":]*):(\d+)"/);
            if (!m) {
                continue;
            }
            # Empty host means same as origin host (RFC 7838 §2.1)
            # Normalize: strip brackets from IPv6 alt-authority to match
            # parse_url()'s bracket-stripping behavior (e.g. "[::1]" → "::1")
            string alt_host;
            if (!m[0].val()) {
                alt_host = origin_host;
            } else {
                alt_host = m[0];
                if (alt_host.size() > 2 && alt_host[0] == "[") {
                    alt_host = alt_host.substr(1, alt_host.size() - 2);
                }
            }
            int alt_port = int(m[1]);
            # Only honour same-origin Alt-Svc (host match or empty host)
            if (alt_host != origin_host) {
                log(LoggerLevel::DEBUG,
                    "Alt-Svc: ignoring cross-origin h3 alternative %s:%d (origin %s)",
                    alt_host, alt_port, origin_host);
                continue;
            }
            # Use the origin host for the pool key — this is the same format
            # used by acquireStream() → getConnection() for pool lookups
            string key = sprintf("%s:%d", origin_host, alt_port);
            log(LoggerLevel::DEBUG, "Alt-Svc: H3 advertised at %s, caching h3", key);
            AutoWriteLock al(pool_lock);
            cacheProtocol(key, "h3");
            # Drain existing non-H3 connections so subsequent requests use H3.
            # setDraining() prevents new streams while in-flight streams complete.
            if (pool.hasKey(key)) {
                foreach HttpClientConnection conn in (pool{key}) {
                    if (!conn.isClosed() && !conn.isDraining() && conn.getProtocol() != "h3") {
                        log(LoggerLevel::DEBUG,
                            "Alt-Svc: draining %s connection %d to %s to force H3 upgrade",
                            conn.getProtocol(), conn.getConnectionId(), key);
                        try {
                            conn.setDraining();
                        } catch () {
                        }
                    }
                }
            }
            return;
        }
    }

    #! Internal: handles protocol fallback after a connection error
    /** For plain HTTP connections where h2c fails, caches "h1" so subsequent
        connections use HTTP/1.1 directly.  For HTTPS connections, falls back
        from H3 to H2 on any H3-specific error or on a generic connection error
        while H3 is cached (e.g., from Alt-Svc discovery).
    */
    private handleProtocolFallback(string url, string err) {
        hash<UrlInfo> url_info = parse_url(url);
        if (!url_info.port) {
            url_info.port = url_info.protocol == "https" ? 443 : 80;
        }
        string key = sprintf("%s:%d", url_info.host, url_info.port);

        if (url_info.protocol != "https" && isH2cIncompatible(err)) {
            # Plain HTTP: h2c failed → server doesn't support HTTP/2 prior knowledge
            log(LoggerLevel::DEBUG, "h2c failed for %s (%s), falling back to HTTP/1.1", key, err);
            AutoWriteLock al(pool_lock);
            cacheProtocol(key, "h1");
        } else if (url_info.protocol == "https") {
            bool is_h3_err = err == "HTTP3-CONNECT-ERROR" || err == "HTTP3-STATE-ERROR"
                || err == "HTTP3-ABORT" || err == "QUIC-HTTP3-ERROR";
            # HTTPCLIENT-CONNECT-ERROR is raised when the connection closes with an error
            # while waiting for readiness (waitForReady()).  When H3 is currently cached
            # (from a prior Alt-Svc response), this means the QUIC connection failed —
            # fall back to H2.  Use a read lock to check the cache first to avoid the
            # write-lock overhead on the common (H2, non-H3) path.
            bool is_h3_conn_err = False;
            if (!is_h3_err && err == "HTTPCLIENT-CONNECT-ERROR") {
                AutoReadLock al(pool_lock);
                is_h3_conn_err = protocol_cache{key} == "h3";
            }
            if (is_h3_err || is_h3_conn_err) {
                # HTTPS: H3 failed → fall back to H2
                log(LoggerLevel::DEBUG, "H3 failed for %s (%s), falling back to HTTP/2", key, err);
                AutoWriteLock al(pool_lock);
                cacheProtocol(key, "h2");
            }
        }
    }

    #! Internal: checks if a connection is alive at the transport level
    /** Verifies that the underlying socket is open and the poll operation hasn't
        detected any errors. This catches cases where the I/O thread has already
        detected a dead connection (FIN/RST received) but the HttpClientConnection
        state hasn't been updated yet.

        @note Called under pool_lock read lock — must not block or acquire pool_lock
    */
    private bool isConnectionAlive(HttpClientConnection conn) {
        HttpClientPollOperation op = conn.getPollOperation();
        if (op.isClosed() || op.hasError()) {
            log(LoggerLevel::DEBUG, "connection %d poll operation is %s",
                conn.getConnectionId(), op.isClosed() ? "closed" : "errored");
            return False;
        }
        if (!conn.getSocket().isOpen()) {
            log(LoggerLevel::DEBUG, "connection %d socket is closed",
                conn.getConnectionId());
            return False;
        }
        return True;
    }

    #! Internal: evicts specific connections from the pool and closes them
    /** @param key the host:port pool key
        @param conns list of connections to evict
    */
    private evictConnections(string key, list<HttpClientConnection> conns) {
        {
            AutoWriteLock al(pool_lock);
            if (pool.hasKey(key)) {
                # Build a set of connection IDs to evict for O(1) lookup
                hash<string, bool> evict_ids;
                foreach HttpClientConnection c in (conns) {
                    evict_ids{string(c.getConnectionId())} = True;
                }
                list<HttpClientConnection> live = ();
                foreach HttpClientConnection c in (pool{key}) {
                    if (!evict_ids{string(c.getConnectionId())}) {
                        push live, c;
                    }
                }
                if (live) {
                    pool{key} = live;
                } else {
                    remove pool{key};
                }
            }
        }
        foreach HttpClientConnection conn in (conns) {
            log(LoggerLevel::DEBUG, "evicting dead %s connection %d",
                conn.getProtocol(), conn.getConnectionId());
            try { conn.close(); } catch () {}
        }
    }

    #! Internal: evicts dead connections for the given URL
    private evictDeadConnections(string url) {
        hash<UrlInfo> url_info = parse_url(url);
        string key = sprintf("%s:%d", url_info.host, url_info.port);

        list<HttpClientConnection> to_close = ();
        {
            AutoWriteLock al(pool_lock);
            if (!pool.hasKey(key)) {
                return;
            }
            list<HttpClientConnection> live = ();
            foreach HttpClientConnection conn in (pool{key}) {
                # Evict only definitively dead connections — not those that
                # are merely at capacity (which would drop in-flight streams)
                bool dead = conn.isClosed();
                if (!dead) {
                    HttpClientPollOperation op = conn.getPollOperation();
                    dead = op.isClosed() || op.hasError();
                }
                if (dead) {
                    push to_close, conn;
                } else {
                    push live, conn;
                }
            }
            if (live) {
                pool{key} = live;
            } else {
                remove pool{key};
            }
        }
        foreach HttpClientConnection conn in (to_close) {
            log(LoggerLevel::DEBUG, "evicting dead %s connection %d",
                conn.getProtocol(), conn.getConnectionId());
            try { conn.close(); } catch () {}
        }
    }

    #! Internal: caches the protocol for a host:port key, evicting oldest entries if full
    /** @note Called under pool_lock write lock
    */
    private cacheProtocol(string key, string protocol) {
        protocol_cache{key} = protocol;
        if (protocol_cache.size() > MAX_PROTOCOL_CACHE_SIZE) {
            # Evict oldest entries (Qore hashes maintain insertion order);
            # collect only the keys to evict (not the full key list) to avoid
            # allocating a 1000-element list when evicting 1-2 entries
            int to_evict = protocol_cache.size() - MAX_PROTOCOL_CACHE_SIZE;
            list<string> evict_keys;
            HashIterator it(protocol_cache);
            while (it.next() && evict_keys.size() < to_evict) {
                push evict_keys, it.getKey();
            }
            foreach string evict_key in (evict_keys) {
                remove protocol_cache{evict_key};
                remove h3_probe_times{evict_key};
            }
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "HttpClientConnectionManager: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
