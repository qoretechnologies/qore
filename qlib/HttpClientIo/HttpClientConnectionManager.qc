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

    - **Protocol auto-selection**: race HTTP/3 and HTTP/2 with happy eyeballs for HTTPS,
      preferring HTTP/3 when available
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
public class HttpClientConnectionManager inherits HttpClientConnectionManagerBase {
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
        const MaxRequestRetries = DefaultMaxRequestRetries;
    }

    private {
        #! Options for this manager
        hash<HttpClientConnectionManagerOptions> opts;

        #! Connection pool: "host:port" -> list of connections
        hash<string, list<HttpClientConnection>> pool;

        #! Lock for thread-safe access to pool
        /** @anchor pool_lock_pin
            Every public method that might be called concurrently with
            the destructor (e.g. from a background thread retaining a
            reference to the manager) must declare a local variable

            @code
            RWLock pool_lock_pin = pool_lock;
            @endcode

            at its entry.  The local holds a QoreObject reference on the
            RWLock wrapper, so if another thread runs @c delete @c mgr
            concurrently the wrapper's refcount stays >0 and its
            QoreObject destructor is deferred.  Without this pin, member
            cleanup in @c delete derefs the wrapper to 0 → its Qore
            destructor calls @c AbstractSmartLock::destructor, which
            fires @c LOCK-ERROR if any thread still holds the
            underlying lock (lib/QC_RWLock.qpp).

            The pin is a local-only ref: once the method returns, it
            drops out of scope and the wrapper is destroyed cleanly —
            by which time any @c AutoReadLock / @c AutoWriteLock in the
            method body has already released the lock, so the deferred
            @c destructor() call finds the lock free and exits without
            error.
        */
        RWLock pool_lock();

        #! AsyncSocketIoController for async operations
        AsyncSocketIo::AsyncSocketIoController controller;


        #! Logger
        *LoggerInterface logger;

        #! Parsed proxy info (host, port, protocol, ssl)
        *hash<auto> proxy_info;

        #! Shutdown flag
        bool shutdown = False;

        #! Stable AsyncIoController owner id for this manager instance.
        /** Do not use self.uniqueHash() directly for controller ownership:
            object identity strings can be reused after destruction, while the
            controller intentionally retains recent cancel tombstones for a
            short race window.  A monotonic suffix makes owner reuse impossible
            inside the process.
        */
        string owner_id;

        #! Process-local manager id counter used in @ref owner_id.
        static int next_manager_id = 0;

        #! Lock for manager id allocation.
        static Mutex manager_id_lock();

        #! Timestamp of the last idle-connection cleanup, used by @ref maybeCleanupIdle().
        /** Without periodic cleanup, idle connections accumulate in the pool indefinitely;
            each entry keeps a socket / TLS / HTTP2 / HTTP3 session graph alive, causing
            unbounded memory growth under connection churn.  Cleanup is run opportunistically
            from user-facing entry points (e.g. @ref acquireStream()) on the calling thread —
            this avoids both the async-I/O-thread restriction and the ref-cycle that would
            arise from a background thread capturing @c self.
        */
        date last_cleanup_time = now_us();

        #! Mutex + condition to serialize connection creation per pool key
        Mutex create_mutex();
        Condition create_cond();
        hash<string, bool> creating;

        #! Protocol cache: "host:port" -> "h2" or "h3"
        hash<string, string> protocol_cache;

        #! Alt-Svc alt port map: origin "host:port" -> alt_port
        /** Populated by checkAltSvc() when the server advertises an h3
            alternative on a port different from the origin TCP port.
            createConnection() rewrites Http3ClientConnection's url_info.port
            to alt_port so the QUIC handshake targets the advertised UDP
            port, not the origin TCP port.  Absent entry means alt_port
            equals origin port (the common case).
        */
        hash<string, int> alt_svc_port_map;

        #! Last H3 probe times: "host:port" -> timestamp
        hash<string, date> h3_probe_times;

        #! Pending Alt-Svc discoveries from async callbacks
        /** Thread-safe unlimited Queue: the I/O thread pushes {url, alt_svc}
            entries here (non-blocking because the queue has no size limit);
            user threads drain and process them in acquireStream() before pool
            lookup.  This avoids acquiring pool_lock from the I/O thread.
        */
        Queue pending_alt_svc();

        #! Cookie jar for automatic Set-Cookie / Cookie header handling
        /** Created when enable_cookies is True (default).  Shared across all
            pooled connections — cookies set by one request are sent with
            subsequent matching requests regardless of which connection is used.
        */
        *CookieJar cookie_jar;
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
        if (self.opts.max_request_retries < 0) {
            throw "HTTPCLIENT-OPTION-ERROR",
                sprintf("max_request_retries must be >= 0 (0 = disabled), got %d",
                self.opts.max_request_retries);
        }
        {
            AutoLock al(manager_id_lock);
            owner_id = sprintf("HttpClientConnectionManager:%d:%s",
                ++next_manager_id, self.uniqueHash());
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

        # Create cookie jar if enabled (default: True)
        if (self.opts.enable_cookies) {
            cookie_jar = new CookieJar();
        }

        log(LoggerLevel::DEBUG, "connection manager created: protocol=%s max_conn=%d max_streams=%d "
            "max_retries=%d idle=%d cookies=%y",
            self.opts.protocol,
            self.opts.max_connections_per_host,
            self.opts.max_streams_per_connection,
            self.opts.max_request_retries,
            self.opts.idle_timeout / 1s,
            self.opts.enable_cookies);
    }

    #! Internal: opportunistically runs @ref cleanupIdleConnections() if enough time
    #! has passed since the last run.
    /** Called from user-facing entry points (e.g. @ref acquireStream()) so cleanup
        happens naturally on activity — no dedicated cleanup thread or timer is needed.
        Cleanup interval is 1/3 of @c idle_timeout (clamped to [5s, 60s]).

        Runs synchronously on the calling thread — never touches the async-I/O thread.
        Qore code is forbidden on that thread, and a background-thread-based scheduler
        would create a ref cycle through the captured @c self that prevents the
        manager's destructor from running.

        The last-cleanup-time update is best-effort under races: two threads that
        happen to both pass the time check may both call @ref cleanupIdleConnections(),
        which is harmless because it takes @c pool_lock and the second caller finds
        everything already cleaned up.
    */
    private:internal maybeCleanupIdle() {
        if (shutdown) {
            return;
        }
        date interval = milliseconds(opts.idle_timeout / 3);
        if (interval < 5s) {
            interval = 5s;
        } else if (interval > 60s) {
            interval = 60s;
        }
        date now = now_us();
        if ((now - last_cleanup_time) < interval) {
            return;
        }
        # Update the timestamp BEFORE doing the work so concurrent callers skip;
        # worst-case benign race produces two serial cleanups.
        last_cleanup_time = now;
        try {
            cleanupIdleConnections();
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "cleanupIdleConnections error: %s: %s",
                ex.err, ex.desc);
        }
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

    #! Returns the cookie jar (NOTHING if cookies are disabled)
    /** @return the CookieJar instance, or NOTHING if \c enable_cookies is False
    */
    *CookieJar getCookieJar() {
        return cookie_jar;
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
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
        # Opportunistically evict idle connections on request entry so the pool
        # doesn't grow unboundedly under churn.  Throttled to at most once per
        # cleanup-interval window; see maybeCleanupIdle().
        maybeCleanupIdle();

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

    #! Ensures a connection to the given URL is being established, returning an EventNotifier
    /** Acquires a stream handle (initiating TCP/TLS connection if needed) and returns an
        EventNotifier that fires when the connection becomes ready or closes.  The stream
        handle is immediately released after getting the notifier, so it does not hold a
        concurrent stream slot.

        Use this to pre-warm a connection asynchronously while performing other work (e.g.,
        an OAuth2 token refresh).  The returned notifier can be polled in a state-machine
        continuePoll() to detect connection readiness without blocking.

        @param url the target URL (only scheme, host, and port are used)

        @return an EventNotifier to poll for readiness, or NOTHING if the connection is
        already ready

        @since %HttpClientIo 1.2
    */
    *EventNotifier ensureConnectionAsync(string url) {
        HttpClientStreamHandle stream = acquireStream(url);
        *EventNotifier notifier = stream.getConnectionReadyNotifier();
        # stream destructs here, releasing its reservation if not yet submitted
        return notifier;
    }

    #! Acquires a stream and starts streaming with automatic retry on stale connections
    /** If the server closed the connection between acquisition and streaming start
        (a race inherent in pooled HTTP/1.1 connections), this method retries with a
        fresh connection up to \c HttpClientConnectionManagerOptions::max_request_retries times.

        @param url the target URL; only scheme, host, and port are used for connection
        management — the URL's path is \b not prepended to \c path
        @param method HTTP method (e.g., "POST")
        @param path the full request path to send (must include any base path from the URL)
        @param headers request headers (optional)
        @param body_streaming if True, enables bidirectional streaming (HTTP/2 and HTTP/3 only;
        HTTP/1.1 supports response-only streaming)
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
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
        int retries = 0;
        int max_retries = opts.max_request_retries;
        bool timeout_retried = False;
        bool h3_close_retried = False;
        while (True) {
            HttpClientStreamHandle stream = acquireStream(url);
            try {
                stream.startStreaming(method, path, headers, body_streaming, body);
                return stream;
            } catch (hash<ExceptionInfo> ex) {
                # HTTP3-CONNECTION-CLOSED and HTTP3-READ-ERROR retry only ONCE:
                # the Gmail/Google idle-close pattern needs one retry on a fresh
                # connection, but if the fresh connection ALSO fails the server
                # is truly down — burning MaxRequestRetries × connect_timeout
                # turns a single dead-server failure into a 45s+ hang.
                if (isHttp3ConnectionCloseError(ex.err)) {
                    if (!h3_close_retried && retries < max_retries
                            && canRetryRequest(method, stream)) {
                        h3_close_retried = True;
                        ++retries;
                        log(LoggerLevel::DEBUG, "streaming %s, retrying once on fresh connection (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        continue;
                    }
                    rethrow;
                }
                if (isConnectionError(ex.err) && retries < max_retries
                        && canRetryRequest(method, stream)) {
                    ++retries;
                    log(LoggerLevel::DEBUG, "streaming connection error %s, retrying (%d/%d)",
                        ex.err, retries, max_retries);
                    evictDeadConnections(url);
                    continue;
                }
                # Only retry timeout on reused pooled connections (stale connection)
                if (ex.err == "HTTPCLIENT-TIMEOUT"
                        && stream.wasConnectionReused() && !timeout_retried
                        && retries < max_retries && canRetryRequest(method, stream)) {
                    timeout_retried = True;
                    ++retries;
                    HttpClientConnection timed_out_conn = stream.getConnection();
                    log(LoggerLevel::DEBUG, "streaming timed out on pooled %s connection %d, "
                        "evicting stale connection and retrying (%d/%d)",
                        timed_out_conn.getProtocol(), timed_out_conn.getConnectionId(),
                        retries, max_retries);
                    try { timed_out_conn.close(); } catch () {}
                    evictDeadConnections(url);
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Submits an async request and returns a Future for the response
    /** @param url the target URL; only scheme, host, and port are used for connection
        management — the URL's path is \b not prepended to \c path
        @param method HTTP method
        @param path the full request path to send (must include any base path from the URL)
        @param headers request headers (optional)
        @param body request body (optional)

        @return a hash with "stream" (HttpClientStreamHandle, for cancellation) and
        "future" (Future, resolves to the response hash)

        @throw HTTPCLIENT-URL-ERROR if the URL is invalid or missing a host
        @throw HTTPCLIENT-SHUTDOWN if the connection manager is shutting down
        @throw HTTPCLIENT-CAPACITY-ERROR if no connection can be acquired after retries
        @throw HTTPCLIENT-TIMEOUT if the connection establishment times out
        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use

        @since %HttpClientIo 1.2
    */
    hash<auto> submitRequestAsync(string url, string method, string path,
            *hash<auto> headers, *data body) {
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
        # Inject cookies from jar into request headers
        hash<UrlInfo> url_info;
        if (cookie_jar) {
            url_info = parse_url(url);
            *string cookie_hdr = cookie_jar.getCookieHeader(
                url_info.host, path, url_info.protocol == "https");
            if (cookie_hdr) {
                headers = headers ?? {};
                if (headers.cookie) {
                    headers.cookie += "; " + cookie_hdr;
                } else {
                    headers.cookie = cookie_hdr;
                }
            }
        }

        int retries = 0;
        int max_retries = opts.max_request_retries;
        bool h3_close_retried = False;
        while (True) {
            *HttpClientStreamHandle stream;
            try {
                stream = acquireStream(url);
                hash<auto> result = stream.submitAsync(method, path, headers, body);
                # Wrap the Future so Set-Cookie extraction happens when
                # the caller resolves it — no extra threads
                if (cookie_jar) {
                    if (!url_info) {
                        url_info = parse_url(url);
                    }
                    result.future = new CookieExtractingFuture(result.future,
                        cookie_jar, url_info.host, path,
                        url_info.protocol == "https");
                }
                return {
                    "stream": stream,
                    "future": result.future,
                };
            } catch (hash<ExceptionInfo> ex) {
                # NOTE: no manual reservation release needed here — submitAsync()
                # calls releaseReservationIfHeld() on all pre-submit failure paths
                # (waitForReady timeout, cancel, submitAsyncImpl exception), and
                # the destructor is a final safety net
                if (isHttp3ConnectionCloseError(ex.err)) {
                    if (!h3_close_retried && retries < max_retries
                            && canRetryRequest(method, stream)) {
                        h3_close_retried = True;
                        ++retries;
                        log(LoggerLevel::DEBUG, "async %s, retrying once on fresh connection (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        continue;
                    }
                    rethrow;
                }
                if ((isCapacityError(ex.err) || isConnectionError(ex.err))
                        && retries < max_retries && canRetryRequest(method, stream)) {
                    ++retries;
                    if (isConnectionError(ex.err)) {
                        log(LoggerLevel::DEBUG, "connection error %s, evicting and retrying (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        # h2c failure on plain HTTP → fall back to h1 for this host
                        handleProtocolFallback(url, ex.err);
                    } else {
                        log(LoggerLevel::DEBUG, "capacity error %s (retry %d/%d)",
                            ex.err, retries, max_retries);
                    }
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Submits an async request and returns a cancellable Future
    /** Unlike @ref submitRequestAsync(), this method returns an
        @ref HttpCancellableFuture that the caller can cancel to abort the
        in-flight HTTP request at the protocol level.

        @param url the target URL; only scheme, host, and port are used for connection
        management — the URL's path is \b not prepended to \c path
        @param method HTTP method
        @param path the full request path to send (must include any base path from the URL)
        @param headers request headers (optional)
        @param body request body (optional)

        @return a @ref HttpCancellableFuture that resolves to the raw response hash and
        supports cancel()

        @throw HTTPCLIENT-URL-ERROR if the URL is invalid or missing a host
        @throw HTTPCLIENT-SHUTDOWN if the connection manager is shutting down
        @throw HTTPCLIENT-CAPACITY-ERROR if no connection can be acquired after retries
        @throw HTTPCLIENT-TIMEOUT if the connection establishment times out

        @since %HttpClientIo 1.3
    */
    HttpCancellableFuture requestAsync(string url, string method, string path,
            *hash<auto> headers, *data body) {
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
        # Inject cookies from jar into request headers
        hash<UrlInfo> url_info;
        if (cookie_jar) {
            url_info = parse_url(url);
            *string cookie_hdr = cookie_jar.getCookieHeader(
                url_info.host, path, url_info.protocol == "https");
            if (cookie_hdr) {
                headers = headers ?? {};
                if (headers.Cookie) {
                    headers.Cookie += "; " + cookie_hdr;
                } else {
                    headers.Cookie = cookie_hdr;
                }
            }
        }

        int retries = 0;
        int max_retries = opts.max_request_retries;
        bool h3_close_retried = False;
        while (True) {
            *HttpClientStreamHandle stream;
            try {
                stream = acquireStream(url);
                hash<auto> result = stream.submitAsync(method, path, headers, body);
                Future raw_future = result.future;
                # Wrap with cookie extraction if needed
                if (cookie_jar) {
                    if (!url_info) {
                        url_info = parse_url(url);
                    }
                    raw_future = new CookieExtractingFuture(raw_future,
                        cookie_jar, url_info.host, path,
                        url_info.protocol == "https");
                }
                return new HttpCancellableFuture(raw_future, stream);
            } catch (hash<ExceptionInfo> ex) {
                if (isHttp3ConnectionCloseError(ex.err)) {
                    if (!h3_close_retried && retries < max_retries
                            && canRetryRequest(method, stream)) {
                        h3_close_retried = True;
                        ++retries;
                        log(LoggerLevel::DEBUG, "cancellable async %s, retrying once on fresh connection (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        continue;
                    }
                    rethrow;
                }
                if ((isCapacityError(ex.err) || isConnectionError(ex.err))
                        && retries < max_retries && canRetryRequest(method, stream)) {
                    ++retries;
                    if (isConnectionError(ex.err)) {
                        log(LoggerLevel::DEBUG, "connection error %s, evicting and retrying (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        handleProtocolFallback(url, ex.err);
                    } else {
                        log(LoggerLevel::DEBUG, "capacity error %s (retry %d/%d)",
                            ex.err, retries, max_retries);
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

        @note Stale connection detection uses two levels:
        - \b Instant: dead connections (FIN/RST from server) are detected by the async
          I/O thread via epoll/kqueue + \c recv(MSG_PEEK) and marked as closed before
          the pool scan returns them.  No timeout needed — detection is immediate.
        - \b Timeout: for "half-open" connections (server went silent — no FIN/RST),
          reused pooled connections use \c connect_timeout as the request deadline instead
          of the full \c request_timeout.  If the server doesn't respond within
          \c connect_timeout, the connection is evicted and the request is retried once
          on a fresh connection, also with \c connect_timeout as the deadline.  This
          provides fast stale detection (typically 2-5s) instead of waiting the full
          \c request_timeout (30-60s).  Timeouts on freshly created connections are not
          retried — they indicate a genuinely slow or unreachable server.

        @throw HTTPCLIENT-TIMEOUT if the request times out (after retry for pooled connections)
        @throw HTTPCLIENT-REQUEST-ERROR if the request fails due to a protocol or transport error
        @throw HTTPCLIENT-URL-ERROR if the URL is invalid or missing a host
        @throw HTTPCLIENT-SHUTDOWN if the connection manager is shutting down
        @throw HTTPCLIENT-CAPACITY-ERROR if no connection can be acquired after retries
        @throw HTTP-REDIRECT-ERROR if the maximum number of redirects is exceeded
    */
    hash<auto> request(string url, string method, string path, *hash<auto> headers,
            *data body, *timeout timeout_ms) {
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
        int redirect_count = 0;

        while (True) {
            # Inject cookies from jar into request headers
            hash<auto> req_headers = headers ?? {};
            if (cookie_jar) {
                hash<UrlInfo> url_info = parse_url(url);
                bool is_secure = url_info.protocol == "https";
                *string cookie_hdr = cookie_jar.getCookieHeader(
                    url_info.host, path, is_secure);
                if (cookie_hdr) {
                    if (req_headers.cookie) {
                        req_headers.cookie += "; " + cookie_hdr;
                    } else {
                        req_headers.cookie = cookie_hdr;
                    }
                }
            }

            hash<auto> resp = requestIntern(url, method, path, req_headers, body, timeout_ms);

            # Process Set-Cookie before checking redirect
            if (cookie_jar && resp.headers) {
                hash<UrlInfo> url_info = parse_url(url);
                bool is_secure = url_info.protocol == "https";
                cookie_jar.processResponseHeaders(
                    url_info.host, path, is_secure, resp.headers);
            }

            # Follow redirects (301, 302, 303, 307, 308)
            if (!opts.redirect_passthru
                    && resp.status_code >= 301 && resp.status_code <= 308
                    && resp.status_code != 304 && resp.status_code != 305
                    && resp.headers.location) {
                if (++redirect_count > opts.max_redirects) {
                    throw "HTTP-REDIRECT-ERROR",
                        sprintf("maximum redirects (%d) exceeded", opts.max_redirects);
                }
                string location = resp.headers.location;
                log(LoggerLevel::DEBUG, "redirect %d/%d: %d -> %s",
                    redirect_count, opts.max_redirects, resp.status_code, location);

                # Parse redirect URL
                if (location =~ /^https?:\/\//) {
                    # Absolute URL — update base URL and path
                    hash<UrlInfo> loc_info = parse_url(location);
                    url = sprintf("%s://%s", loc_info.protocol, loc_info.host);
                    if (loc_info.port) {
                        url += sprintf(":%d", loc_info.port);
                    }
                    path = loc_info.path ?? "/";
                } else {
                    # Relative path
                    path = location;
                }

                # 303: change method to GET and drop body (RFC 7231 Section 6.4.4)
                if (resp.status_code == 303) {
                    method = "GET";
                    body = NOTHING;
                }
                # 301/302: historically change POST to GET (RFC 7231 Section 6.4.2/6.4.3)
                if ((resp.status_code == 301 || resp.status_code == 302)
                        && method == "POST") {
                    method = "GET";
                    body = NOTHING;
                }
                continue;
            }

            return resp;
        }
    }

    #! Performs a single HTTP request with connection retry (no redirect following)
    private hash<auto> requestIntern(string url, string method, string path,
            *hash<auto> headers, *data body, *timeout timeout_ms) {
        int retries = 0;
        int max_retries = opts.max_request_retries;
        bool timeout_retried = False;
        bool h3_close_retried = False;
        while (True) {
            *HttpClientStreamHandle stream;
            try {
                stream = acquireStream(url);
                # Per-request timeout resolution:
                # 1. Caller-provided timeout always wins (the caller knows best,
                #    e.g. LLM queries with 60s timeout).
                # 2. After a stale-connection retry: use connect_timeout for fast
                #    fail — if the fresh connection also times out, the server is
                #    genuinely unreachable.
                # 3. Otherwise: NOTHING — falls through to the default
                #    request_timeout configured on the connection manager.
                #
                # NOTE: reused pooled connections intentionally use the default
                # request_timeout rather than connect_timeout.  Using
                # connect_timeout for "stale detection" caused false timeouts
                # on slow-response endpoints (AI/LLM, long-running APIs) where
                # the connection is alive but the response takes longer than
                # connect_timeout.  True stale connections are detected at the
                # TCP/protocol layer (RST, GOAWAY, connection-reset errors) and
                # retried via the isConnectionError path below.
                *timeout effective_timeout;
                if (exists timeout_ms) {
                    effective_timeout = timeout_ms;
                } else if (timeout_retried) {
                    effective_timeout = opts.connect_timeout;
                }
                log(LoggerLevel::DEBUG, "requestIntern %s %s: reused=%y timeout_retried=%y "
                    "effective_timeout=%y",
                    method, path, stream.wasConnectionReused(), timeout_retried,
                    effective_timeout);
                hash<auto> resp = stream.request(method, path, headers, body,
                    effective_timeout);
                checkAltSvc(url, resp);
                return resp;
            } catch (hash<ExceptionInfo> ex) {
                # Handle HTTPCLIENT-TIMEOUT before the generic isConnectionError
                # retry.  HTTPCLIENT-TIMEOUT is in isConnectionError() for the
                # benefit of acquireAndStartStreaming / submitRequestAsync where
                # timeouts are connection-establishment errors that legitimately
                # warrant full retry.  But in requestIntern(), the timeout may
                # come from Future.get() (a *request* timeout on an established
                # connection).  Letting isConnectionError catch it here causes
                # up to MaxRequestRetries full-timeout retries against a dead
                # server (e.g. 3 × 10s = 30s+), when the correct behavior is:
                # only retry once on a reused pooled connection (stale detection),
                # never on a fresh connection (genuinely unreachable).
                if (ex.err == "HTTPCLIENT-TIMEOUT") {
                    if (stream && stream.wasConnectionReused() && !timeout_retried
                            && retries < max_retries && canRetryRequest(method, stream)) {
                        timeout_retried = True;
                        ++retries;
                        HttpClientConnection timed_out_conn = stream.getConnection();
                        log(LoggerLevel::DEBUG, "request timed out on pooled %s connection %d, "
                            "evicting stale connection and retrying with connect_timeout=%y (%d/%d)",
                            timed_out_conn.getProtocol(), timed_out_conn.getConnectionId(),
                            opts.connect_timeout, retries, max_retries);
                        try { timed_out_conn.close(); } catch () {}
                        evictDeadConnections(url);
                        continue;
                    }
                    rethrow;
                }
                # HTTP3-CONNECTION-CLOSED / HTTP3-READ-ERROR retry ONCE only —
                # see acquireAndStartStreaming for rationale.
                if (isHttp3ConnectionCloseError(ex.err)) {
                    if (!h3_close_retried && retries < max_retries
                            && canRetryRequest(method, stream)) {
                        h3_close_retried = True;
                        ++retries;
                        log(LoggerLevel::DEBUG, "sync request %s, retrying once on fresh connection (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        continue;
                    }
                    rethrow;
                }
                if ((isCapacityError(ex.err) || isConnectionError(ex.err))
                        && retries < max_retries && canRetryRequest(method, stream)) {
                    ++retries;
                    if (isConnectionError(ex.err)) {
                        log(LoggerLevel::DEBUG, "connection error %s, evicting and retrying (%d/%d)",
                            ex.err, retries, max_retries);
                        evictDeadConnections(url);
                        handleProtocolFallback(url, ex.err);
                    } else {
                        log(LoggerLevel::DEBUG, "capacity error %s (retry %d/%d)",
                            ex.err, retries, max_retries);
                    }
                    continue;
                }
                rethrow;
            }
        }
    }

    #! Returns connection pool statistics
    /** @return a hash with the following keys:
        - \c total_connections (int): total number of pooled connections across all hosts
        - \c total_active_streams (int): total number of active streams across all connections
        - \c hosts (int): number of distinct host:port entries in the pool
        - \c per_host (hash<string, hash<auto>>): per-host breakdown keyed by "host:port", where
          each value is a hash with:
          - \c connections (int): number of connections to that host
          - \c active_streams (int): total active streams on those connections
          - \c draining (int): number of connections in GOAWAY/draining state
        - \c protocol (string): configured protocol selection ("auto", "h1", "h2", or "h3")
    */
    hash<auto> getStats() {
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
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
            controller.cancelByOwner(owner_id);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "closeAll: cancelByOwner error: %s: %s", ex.err, ex.desc);
        }

        # Barrier: wait for every dispatcher worker running a callback tagged
        # with this manager's owner id to exit before returning.
        # handleConnectionResult() — invoked from those callbacks — acquires
        # pool_lock on a worker; without this barrier, a worker triggered by
        # an in-flight stream error (e.g. H1 mid-stream cancel from a
        # stream.cancel() above this destructor) can still be inside the
        # write lock when the destructor destroys it, producing
        # "LOCK-ERROR: TID N deleted RWLock while TID M was holding the lock".
        # Per-owner (not global) so a manager destructor doesn't block on
        # unrelated long-running callbacks on the shared controller.
        controller.flushCallbacksByOwner(owner_id);

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

        # Final callback barrier: conn.close() above synchronously cancels
        # the in-flight poll ops, which queue terminal onComplete callbacks
        # on the AsyncIoController.  Call flushCallbacksByOwner again so
        # those callbacks finish before our destructor returns.  The
        # underlying per-owner accounting counts callbacks at enqueue
        # time (not pop time), so this call waits for every callback
        # queued by the cancellations above, not just those already
        # picked up by a worker.
        controller.flushCallbacksByOwner(owner_id);
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
        # Pin @c pool_lock across this method: see @ref pool_lock_pin.
        RWLock pool_lock_pin = pool_lock;
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
    /** In auto mode: HTTPS with no proxy uses a happy-eyeballs race between
        HTTP/3 and HTTP/2, giving HTTP/3 a head start.  Plain HTTP uses
        HTTP/1.1 unless HTTP/2 is explicitly requested.
        The protocol cache persists negotiation results to avoid repeated probes.

        @param url_info parsed URL info
        @param key the pool key "host:port"

        @return "h1", "h2", "h3", or "happy-eyeballs"
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

        # No cache and no proxy: happy eyeballs — try H3 + H2 in parallel
        if (!proxy_info) {
            return "happy-eyeballs";
        }
        # Proxy: H3 (QUIC) can't tunnel through proxies, use H2
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
            # takes over creation to prevent indefinite blocking when the creator stalls.
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
            # it before creating the connection.
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

            # Create connection outside the write lock so constructor-side setup cannot
            # stall unrelated pool operations.
            HttpClientConnection conn;
            if (protocol == "happy-eyeballs") {
                conn = createConnectionHappyEyeballs(url_info, key);
            } else {
                conn = createConnection(url_info, key, protocol);
            }

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
            HttpClientConnection conn = new Http1ClientConnection(url_info, opts.connect_timeout, logger,
                1, opts.ssl_verify_mode, opts.accept_all_certs, proxy_info,
                opts.client_cert, opts.client_pk);
            applyTcpUserTimeout(conn);
            return conn;
        }

        if (protocol == "h3") {
            # Record probe time
            h3_probe_times{key} = now_us();

            # Apply Alt-Svc port rewrite: when the server advertised h3 on a
            # port different from the origin TCP port, connect QUIC to the
            # advertised alt port instead of the origin port.
            hash<auto> h3_url_info = url_info;
            if (alt_svc_port_map.hasKey(key)) {
                int alt_port = alt_svc_port_map{key};
                h3_url_info.port = alt_port;
                log(LoggerLevel::DEBUG,
                    "Alt-Svc: using h3 alt-port %d for origin %s", alt_port, key);
            }
            try {
                log(LoggerLevel::DEBUG, "creating new HTTP/3 connection to %s:%d",
                    h3_url_info.host, h3_url_info.port);
                Http3ClientConnection conn(h3_url_info, opts.connect_timeout, logger,
                    opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
                    opts.client_cert, opts.client_pk);
                # H3 uses UDP/QUIC — TCP_USER_TIMEOUT does not apply.
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
        HttpClientConnection h2 = new Http2ClientConnection(url_info, opts.connect_timeout, logger,
            opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
            proxy_info, opts.client_cert, opts.client_pk);
        applyTcpUserTimeout(h2);
        return h2;
    }

    #! Internal: apply the configured TCP_USER_TIMEOUT to a connection's socket
    /** Called immediately after construction so the value is in place before
        the I/O thread fires confirmConnected.  No effect on UDP (H3) or
        platforms without \c TCP_USER_TIMEOUT.
    */
    private applyTcpUserTimeout(HttpClientConnection conn) {
        # Qore's timeout type is an int representing milliseconds; pass it
        # directly to setUserTimeout(int).
        if (opts.tcp_user_timeout > 0) {
            try {
                conn.getSocket().setUserTimeout(opts.tcp_user_timeout);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG,
                    "TCP_USER_TIMEOUT could not be applied: %s: %s",
                    ex.err, ex.desc);
            }
        }
    }

    #! Internal: happy eyeballs parallel H3 + H2 connection race
    /** Tries H3 (QUIC/UDP) and H2 (TCP/TLS) in parallel. H3 starts first;
        if it doesn't connect within the stagger delay, H2 starts alongside.
        The first connection to become ready wins; the loser is cancelled.

        @param url_info parsed URL info
        @param key the host:port pool key
        @return the winning connection (H3 or H2)

        @since HttpClientIo 1.3
    */
    private HttpClientConnection createConnectionHappyEyeballs(hash<auto> url_info, string key) {
        # Stagger delay: H3 gets a head start before H2 begins
        int HappyEyeballsStaggerMs = 100;

        log(LoggerLevel::DEBUG, "happy eyeballs: starting H3 + H2 race for %s", key);

        # Record H3 probe time
        h3_probe_times{key} = now_us();

        # Create H3 connection (non-blocking — constructor sets up QUIC poll op)
        *Http3ClientConnection h3conn;
        try {
            h3conn = new Http3ClientConnection(url_info, opts.connect_timeout, logger,
                opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
                opts.client_cert, opts.client_pk);
            h3conn.setControllerRef(controller);
            h3conn.setManagerContext(self, key);
            submitConnectionToController(key, h3conn);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "happy eyeballs: H3 creation failed (%s: %s), using H2", key, ex.err, ex.desc);
            # H3 not available — just return H2
            {
                AutoWriteLock al(pool_lock);
                cacheProtocol(key, "h2");
            }
            return createConnection(url_info, key, "h2");
        }

        # Wait for H3 with stagger timeout using poll on the connection's notifier
        EventNotifier h3_notifier();
        bool h3_queued = h3conn.registerReadyNotifier(h3_notifier);
        if (!h3_queued) {
            # Already decided (ready or failed)
            if (h3conn.isReady()) {
                log(LoggerLevel::DEBUG, "happy eyeballs: H3 connected immediately for %s", key);
                {
                    AutoWriteLock al(pool_lock);
                    cacheProtocol(key, "h3");
                }
                return h3conn;
            }
            # H3 failed immediately — fall through to start H2
        } else {
            # Drive the stagger wait via the I/O controller — no handler-thread
            # Socket::poll() syscall.  The NotifierPollOperation submits a
            # SOCK_POLLIN watch on the notifier's fd; the I/O thread wakes us via
            # the returned Queue on signal or timeout.  waitForNotifier returns
            # False on timeout; in either case we still acknowledge() to drain
            # the fd (matches the former Socket::poll pattern) and then check
            # isReady() for the authoritative state.
            controller.waitForNotifier(h3_notifier, HappyEyeballsStaggerMs);
            h3_notifier.acknowledge();
            if (h3conn.isReady()) {
                log(LoggerLevel::DEBUG, "happy eyeballs: H3 won within stagger for %s", key);
                {
                    AutoWriteLock al(pool_lock);
                    cacheProtocol(key, "h3");
                }
                return h3conn;
            }
        }

        # H3 didn't connect yet — start H2 in parallel
        log(LoggerLevel::DEBUG, "happy eyeballs: starting H2 for %s (H3 still pending)", key);
        Http2ClientConnection h2conn(url_info, opts.connect_timeout, logger,
            opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
            proxy_info, opts.client_cert, opts.client_pk);
        applyTcpUserTimeout(h2conn);
        h2conn.setControllerRef(controller);
        h2conn.setManagerContext(self, key);
        submitConnectionToController(key, h2conn);

        # Race: register notifier and check if either already connected
        EventNotifier race_notifier();
        bool h3_pending = h3conn.registerReadyNotifier(race_notifier);
        bool h2_pending = h2conn.registerReadyNotifier(race_notifier);

        # If either already decided (ready or failed), check immediately
        if (!h3_pending || !h2_pending) {
            if (h3conn.isReady()) {
                log(LoggerLevel::DEBUG, "happy eyeballs: H3 already ready for %s", key);
                try { h2conn.close(); } catch () {}
                {
                    AutoWriteLock al(pool_lock);
                    cacheProtocol(key, "h3");
                }
                return h3conn;
            }
            if (h2conn.isReady()) {
                log(LoggerLevel::DEBUG, "happy eyeballs: H2 already ready for %s", key);
                try { h3conn.close(); } catch () {}
                {
                    AutoWriteLock al(pool_lock);
                    cacheProtocol(key, "h2");
                }
                return h2conn;
            }
            # Race: H2 setClosed() can fire between submit and registerReadyNotifier,
            # so race_notifier gets queued only on H3.  If H2 failed with
            # SOCKET-CONNECT-ERROR (TCP refused), the port is definitively closed
            # — H3 over UDP won't recover.  Skip the poll (which would wait up to
            # connect_timeout for H3's own timeout to expire) and fall straight
            # through to error handling so asyncModeConnectionRefusedTest
            # fast-fails in ~100ms instead of 10s.
            if (!h2_pending && !h2conn.isReady()) {
                *hash<auto> h2_err_early = h2conn.getPollOperation().getError();
                if (h2_err_early.err == "SOCKET-CONNECT-ERROR") {
                    log(LoggerLevel::DEBUG,
                        "happy eyeballs: H2 failed fast with SOCKET-CONNECT-ERROR for %s,"
                        " aborting H3", key);
                    try { h3conn.close(); } catch () {}
                    try { h2conn.close(); } catch () {}
                    string desc_fast = sprintf("could not connect to %s: %s", key,
                        h2_err_early.desc);
                    throw "SOCKET-CONNECT-ERROR", desc_fast;
                }
            }
        }

        # If BOTH H3 and H2 already failed (neither pending, neither ready),
        # skip the poll entirely — race_notifier will never fire, and blocking
        # up to connect_timeout here would be wasted time.
        bool both_already_closed = !h3_pending && !h2_pending
            && !h3conn.isReady() && !h2conn.isReady();
        if (!both_already_closed) {
            timeout remaining = opts.connect_timeout - HappyEyeballsStaggerMs;
            if (remaining <= 0ms) {
                remaining = opts.connect_timeout;
            }
            # Async wait via the I/O controller (replaces a handler-thread
            # Socket::poll syscall).  See the h3_notifier path above.
            controller.waitForNotifier(race_notifier, remaining);
            race_notifier.acknowledge();
        } else {
            log(LoggerLevel::DEBUG,
                "happy eyeballs: both H3 and H2 already closed for %s, skipping poll", key);
        }

        # Determine winner
        if (h3conn.isReady()) {
            log(LoggerLevel::DEBUG, "happy eyeballs: H3 won for %s", key);
            try { h2conn.close(); } catch () {}
            {
                AutoWriteLock al(pool_lock);
                cacheProtocol(key, "h3");
            }
            return h3conn;
        }
        if (h2conn.isReady()) {
            log(LoggerLevel::DEBUG, "happy eyeballs: H2 won for %s", key);
            try { h3conn.close(); } catch () {}
            {
                AutoWriteLock al(pool_lock);
                cacheProtocol(key, "h2");
            }
            return h2conn;
        }

        # Neither connected — collect error details and clean up both
        *hash<auto> h3_err = h3conn.getPollOperation().getError();
        *hash<auto> h2_err = h2conn.getPollOperation().getError();
        try { h3conn.close(); } catch () {}
        try { h2conn.close(); } catch () {}

        # Build a user-friendly error message with the underlying failure reasons
        string desc = sprintf("could not connect to %s", key);
        # Use the most informative underlying error (prefer H2 as it's TCP-based)
        *string detail = h2_err.desc ?? h3_err.desc;
        if (detail) {
            desc += sprintf(": %s", detail);
        }

        # Fail-fast path: if the H2 (TCP) attempt reported SOCKET-CONNECT-ERROR,
        # the endpoint is definitively refused at the kernel level (ECONNREFUSED
        # or similar).  H3 over UDP can't recover this because the remote host
        # has rejected the port — retrying via the outer isConnectionError loop
        # just wastes another connect_timeout per attempt (seen in
        # asyncModeConnectionRefusedTest: 2×10s = 20s, 5s over the 15s budget).
        # Propagate the underlying SOCKET-CONNECT-ERROR, which is NOT in
        # isConnectionError(), so the retry loop fails fast.
        if (h2_err.err == "SOCKET-CONNECT-ERROR") {
            throw h2_err.err, desc;
        }

        throw "HTTPCLIENT-CONNECT-ERROR", desc;
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
        string controller_key = sprintf("%s:conn:%d", owner_id, conn.getConnectionId());

        if (initial) {
            log(LoggerLevel::DEBUG, "submitting %s connection %d to controller for %s",
                conn.getProtocol(), conn.getConnectionId(), key);
            # Set manager context once — connection_ref is already set in the
            # connection impl constructor via poll_op.setConnection(self)
            conn.setManagerContext(self, key);

            # Install the proactive idle timeout for H1/H2 connections (nginx
            # upstream keepalive_timeout model).  H3 relies on ngtcp2's built-in
            # idle timer negotiated via QUIC max_idle_timeout transport
            # parameter, so skip it there.
            # NOTE: Qore's "timeout" type is an int representing milliseconds;
            # pass it directly to the poll op's setIdleTimeout(int) method.
            if (op instanceof Http1ClientPollOperation) {
                cast<Http1ClientPollOperation>(op).setIdleTimeout(opts.idle_timeout);
            } else if (op instanceof Http2ClientPollOperation) {
                cast<Http2ClientPollOperation>(op).setIdleTimeout(opts.idle_timeout);
                # Set keepalive ping at half the idle timeout so dead connections
                # are detected before the idle timer fires and evicts them.
                cast<Http2ClientPollOperation>(op).setPingInterval(opts.idle_timeout / 2);
            }
        }

        # Use no timeout (-1) for the controller operation.  The H1/H2/H3
        # poll operations are long-lived (they handle multiple request/response
        # cycles on a single connection).  The connect timeout is handled
        # internally by the poll operation's connect state, and request timeouts
        # are handled by FutureImpl::get() in HttpClientStreamHandle::request().
        # Using connect_timeout here caused the controller to kill the operation
        # after connect_timeout seconds, even though it was in the middle of a
        # request/response cycle with a longer timeout.
        #
        # has_on_complete=True: HttpClientPollOperation inherits onComplete()
        # from the Qore-level base class, but the C++/Qore multiple-inheritance
        # resolution in AsyncIoController::submit() (lib/AsyncIoControllerPriv.cpp
        # lines 710-727) may return the AbstractPollOperation no-op from the
        # wrong parent.  Force dispatch explicitly so HttpClientPollOperation::
        # onComplete() fires and handleConnectionResult() removes idle-closed,
        # errored, and proactively-timed-out connections from the pool.
        controller.submit(<SocketPollOperationInfo>{
            "sock": sock,
            "spop": op,
            "key": controller_key,
            "owner": owner_id,
            "to": -1,
            "has_on_complete": True,
        });
    }

    #! Internal: handles connection result from poll operation completion
    handleConnectionResult(string key, HttpClientConnection conn,
            hash<SocketPollResultInfo> result) {
        if (result.canceled) {
            log(LoggerLevel::DEBUG, "connection %d canceled", conn.getConnectionId());
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
            # Promises are rejected so threads in f.get() wake up
            # instead of waiting until request_timeout
            try { conn.close(); } catch () {}
            removeConnectionFromPool(key, conn);
            return;
        }

        if (conn.isDraining()) {
            log(LoggerLevel::DEBUG, "connection %d is draining (GOAWAY received)", conn.getConnectionId());
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

    #! Internal: checks if a method can be retried after request submission
    private bool isIdempotentMethod(string method) {
        string m = toupper(method);
        return m == "GET"
            || m == "HEAD"
            || m == "PUT"
            || m == "DELETE"
            || m == "OPTIONS"
            || m == "TRACE";
    }

    #! Internal: checks if the failure happened before request submission
    /** Before stream submission, the failure is connection-only: no request
        has been put on the wire, so retrying is safe for any HTTP method.
    */
    private bool isConnectionOnlyFailure(*HttpClientStreamHandle stream) {
        return !(stream && stream.isSubmitted());
    }

    #! Internal: checks if a failed request may be retried safely
    /** Connection-only failures can be retried for any method.  After stream
        submission, only idempotent methods are retried automatically.
    */
    private bool canRetryRequest(string method, *HttpClientStreamHandle stream) {
        return isConnectionOnlyFailure(stream) || isIdempotentMethod(method);
    }

    #! Internal: checks if an error indicates a dead connection
    private bool isConnectionError(string err) {
        return isConnectionErrorStatic(err);
    }

    #! Internal: checks if an HTTP/3 peer-close/read error should use the one-shot retry path
    private bool isHttp3ConnectionCloseError(string err) {
        return err == "HTTP3-CONNECTION-CLOSED"
            || err == "HTTP3-READ-ERROR";
    }

    #! Static version of isConnectionError() for use by other classes
    static bool isConnectionErrorStatic(string err) {
        return err == "QUIC-HTTP3-ERROR"
            || err == "QUIC-READ-ERROR"
            || err == "QUIC-RECV-ERROR"
            || err == "QUIC-SEND-ERROR"
            || err == "HTTP3-CONNECT-ERROR"
            || err == "HTTP3-CONNECTION-CLOSED"
            || err == "HTTP3-READ-ERROR"
            || err == "HTTP3-STATE-ERROR"
            || err == "HTTP3-ABORT"
            || err == "HTTP3-EMPTY-READ"
            || err == "HTTP2-EMPTY-READ"
            || err == "HTTP2-CONNECTION-CLOSED"
            || err == "HTTP2-CONNECT-ERROR"
            || err == "HTTP2-ALPN-ERROR"
            || err == "HTTP2-ABORT"
            || err == "HTTP2-STATE-ERROR"
            || err == "HTTP2-ERROR"
            || err == "HTTP1-CONNECT-ERROR"
            || err == "HTTP1-CONNECTION-CLOSED"
            || err == "HTTP1-IDLE-TIMEOUT"
            || err == "HTTP1-STATE-ERROR"
            || err == "HTTP1-SSL-ERROR"
            || err == "HTTP1-ABORT"
            || err == "HTTP1-SEND-ERROR"
            || err == "HTTP1-RECV-ERROR"
            || err == "HTTP2-IDLE-TIMEOUT"
            || err == "HTTPCLIENT-STATE-ERROR"
            || err == "HTTPCLIENT-CONNECT-ERROR"
            || err == "SOCKET-SEND-ERROR"
            || err == "SOCKET-RECV-ERROR"
            || err == "SOCKET-CLOSED"
            # Timeout on a reused keep-alive connection: the server may have
            # closed the connection between requests (TCP FIN race).  Retry on
            # a fresh connection — same as curl's Curl_retry_request() logic.
            || err == "HTTPCLIENT-TIMEOUT";
    }

    #! Internal: checks if an error indicates an h2c protocol incompatibility
    /** Returns True if the error suggests the server doesn't support HTTP/2
        prior knowledge (h2c) on a plain HTTP connection.
    */
    private bool isH2cIncompatible(string err) {
        return err == "HTTP2-EMPTY-READ"
            || err == "HTTP2-CONNECTION-CLOSED"
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
        # Skip during shutdown — see evictDeadConnections() for rationale.
        if (shutdown) {
            return;
        }
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
            # Cache key MUST be the origin (host:origin_port), not the Alt-Svc
            # alternative port.  getConnection() / getProtocol() look up by
            # (url_info.host, url_info.port) = origin port; a cache keyed by
            # alt_port would never match a subsequent request to the origin.
            # When alt_port != origin_port we also record alt_port separately
            # so createConnection() can rewrite the H3 target.
            string key = sprintf("%s:%d", origin_host, url_info.port);
            log(LoggerLevel::DEBUG,
                "Alt-Svc: H3 advertised for origin %s via alt-port %d, caching h3",
                key, alt_port);
            AutoWriteLock al(pool_lock);
            cacheProtocol(key, "h3");
            if (alt_port != url_info.port) {
                alt_svc_port_map{key} = alt_port;
            } else {
                # Alt-Svc port equals origin port — no rewrite needed;
                # remove any stale entry in case it was set by a prior
                # Alt-Svc response pointing elsewhere.
                remove alt_svc_port_map{key};
            }
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
        # Skip during shutdown — see evictDeadConnections() for rationale.
        if (shutdown) {
            return;
        }
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
                || err == "HTTP3-ABORT" || err == "QUIC-HTTP3-ERROR"
                || err == "QUIC-SEND-ERROR" || err == "QUIC-READ-ERROR"
                || err == "QUIC-RECV-ERROR";
            # HTTPCLIENT-CONNECT-ERROR is raised when the connection closes with an
            # error while waiting for readiness (waitForReady()).
            # HTTPCLIENT-TIMEOUT is raised when the QUIC handshake starts but
            # never completes (waitForReady() expires).
            # When H3 is currently cached (from a prior Alt-Svc response), either
            # error means the QUIC connection failed — fall back to H2.
            # Use a read lock to check the cache first to avoid the write-lock
            # overhead on the common (H2, non-H3) path.
            bool is_h3_conn_err = False;
            if (!is_h3_err
                    && (err == "HTTPCLIENT-CONNECT-ERROR" || err == "HTTPCLIENT-TIMEOUT")) {
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
    /** Verifies that the poll operation hasn't detected any errors, the
        underlying socket is open once the connection has left CONNECTING, and
        the idle window has not expired.

        Liveness detection is owned by the async I/O thread.  When a server
        sends FIN/RST, the I/O thread sees the POLLIN/POLLHUP via its
        epoll/kqueue loop and the H1 poll op's \c handleIdle issues a
        \c recv(MSG_PEEK) to confirm; on EOF, the op is marked closed and
        \c op.isClosed() returns True here.  This method does NOT do its own
        \c poll(0) on the fd: that would reach past the async I/O ownership
        model and race the I/O thread for the same kernel state without
        material benefit (the I/O thread learns of the same event microseconds
        later, and the conn-mgr's first-byte retry covers the race window —
        the same trade-off curl makes via \c Curl_retry_request).

        @note Called under pool_lock read lock — must not block or acquire pool_lock
    */
    private bool isConnectionAlive(HttpClientConnection conn) {
        HttpClientPollOperation op = conn.getPollOperation();
        if (op.isClosed() || op.hasError()) {
            log(LoggerLevel::DEBUG, "connection %d poll operation is %s",
                conn.getConnectionId(), op.isClosed() ? "closed" : "errored");
            return False;
        }
        if (conn.getConnectionState() == HttpClientConnectionState::Connecting) {
            return True;
        }
        if (!conn.getSocket().isOpen()) {
            log(LoggerLevel::DEBUG, "connection %d socket is closed",
                conn.getConnectionId());
            return False;
        }

        # Stream-state-sensitive eviction checks (idle-age + max-age).
        # Both gated on getActiveStreamCount() == 0:
        #  - last_activity is not updated continuously mid-stream, so a
        #    long-running stream can have a stale last_activity and would
        #    be falsely judged idle.
        #  - max-age eviction would interrupt in-flight streams; never
        #    evict a connection that's actively serving requests — the
        #    next checkout after the stream completes will catch it.
        # Pure stored-state reads — no syscall, no I/O thread coupling.
        # Use the same cutoff pattern as @ref cleanupIdleConnections (compare
        # two dates) — `(now_us() - timestamp) >= opts.timeout` does not
        # behave reliably because the LHS is a date duration and the RHS
        # is a millisecond int.
        if (conn.getActiveStreamCount() == 0) {
            date now = now_us();
            # Strict idle-age check.  maybeCleanupIdle() runs only every
            # idle_timeout/3 (clamped), so a connection slightly past
            # idle_timeout can slip through.  Reject it here at checkout
            # so reuse decisions are race-free against the cleanup tick.
            if (opts.idle_timeout > 0) {
                date idle_cutoff = now - opts.idle_timeout;
                if (conn.getLastActivity() < idle_cutoff) {
                    # WARN: pool churn driven by idle-age — operationally
                    # relevant signal for "why is the manager opening so
                    # many fresh connections?"  Tune idle_timeout up if
                    # this fires more often than expected.
                    log(LoggerLevel::WARN,
                        "evicting connection %d at checkout: exceeded "
                        "idle_timeout (%y since last activity)",
                        conn.getConnectionId(),
                        now - conn.getLastActivity());
                    return False;
                }
            }
            # Born-at TTL (curl CURLOPT_MAXAGE_CONN).  Bounds the
            # wall-clock age of a pooled connection so middlebox-expired
            # sessions are refreshed proactively.
            if (opts.max_age > 0) {
                date age_cutoff = now - opts.max_age;
                if (conn.getCreationTime() < age_cutoff) {
                    # WARN: pool churn driven by max_age (born-at TTL).
                    # See above — operationally relevant signal.
                    log(LoggerLevel::WARN,
                        "evicting connection %d at checkout: exceeded "
                        "max_age (%y since creation)",
                        conn.getConnectionId(),
                        now - conn.getCreationTime());
                    return False;
                }
            }
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
        # Fast-fail during shutdown — the caller's public-method
        # pool_lock_pin keeps pool_lock alive, but the pool itself has
        # already been removed in closeAll(), so there is no useful
        # work to do.
        if (shutdown) {
            return;
        }
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

    #! Sets the logger for this connection manager
    setLogger(*LoggerInterface logger) {
        self.logger = logger;
    }

    #! Sets the default connection timeout for NEW connections
    /** Existing pooled connections are not affected; only future connect attempts
        (including H3 setup after a stale-connection close) observe the new value.

        @param ct the new connect timeout (must be positive)

        @since HttpClientIo 1.1
    */
    setConnectTimeout(timeout ct) {
        opts.connect_timeout = ct;
    }

    #! Returns the default connection timeout in milliseconds
    /** @since HttpClientIo 1.1 */
    timeout getConnectTimeout() {
        return opts.connect_timeout;
    }

    #! Sets the default request timeout applied when the caller does not pass one
    /** Per-request overrides passed to @ref request() still take precedence.

        @param rt the new default request timeout (must be positive)

        @since HttpClientIo 1.1
    */
    setRequestTimeout(timeout rt) {
        opts.request_timeout = rt;
    }

    #! Returns the default request timeout in milliseconds
    /** @since HttpClientIo 1.1 */
    timeout getRequestTimeout() {
        return opts.request_timeout;
    }

    #! Returns the configured protocol mode (@c "auto", @c "h1", @c "h2", @c "h3")
    /** @since HttpClientIo 1.1 */
    string getProtocolMode() {
        return opts.protocol;
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "HttpClientConnectionManager: " + fmt) + argv);
        }
    }
}

#! Future subclass that extracts Set-Cookie headers when the caller resolves it
/** Overrides \c get() to process cookies from the response headers before
    returning.  No extra threads — cookie extraction happens synchronously
    on the caller's thread when they call \c get().
*/
class CookieExtractingFuture inherits Future {
    private {
        Future real_future;
        CookieJar jar;
        string host;
        string path;
        bool is_secure;
    }

    constructor(Future real_future, CookieJar jar, string host, string path,
            bool is_secure) {
        self.real_future = real_future;
        self.jar = jar;
        self.host = host;
        self.path = path;
        self.is_secure = is_secure;
    }

    #! Blocks until the response is available, then extracts cookies and returns it
    auto get(timeout timeout_ms = 0) {
        auto rv = real_future.get(timeout_ms);
        if (rv.typeCode() == NT_HASH && rv.headers) {
            jar.processResponseHeaders(host, path, is_secure, rv.headers);
        }
        return rv;
    }

    bool isDone() {
        return real_future.isDone();
    }

    bool isError() {
        return real_future.isError();
    }

    bool cancel() {
        return real_future.cancel();
    }
}

} # namespace HttpClientIo
