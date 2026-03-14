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
/** This class manages a pool of HTTP/2 and HTTP/3 connections, providing:

    - **Protocol auto-selection**: Try HTTP/3 first, fall back to HTTP/2
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

    @par Shared Static Controller
    By default all managers share a single static AsyncSocketIoController with
    \c autostop=True. The controller's I/O thread starts when the first poll
    operation is submitted and stops when all operations complete. This avoids
    creating competing I/O threads when multiple managers are used.

    @par Custom Controller (optional)
    To use a dedicated controller instead of the shared static one:
    @code{.py}
%requires HttpClientIo
%requires AsyncSocketIo

AsyncSocketIoController controller(False);
controller.start();

HttpClientConnectionManager mgr1();
mgr1.setController(controller);  # replaces static controller

HttpClientConnectionManager mgr2();
mgr2.setController(controller);

# ... use mgr1 and mgr2 ...
mgr1.closeAll();
mgr2.closeAll();
controller.stop();
    @endcode
*/
public class HttpClientConnectionManager {
    public {
        #! Interval before re-probing HTTP/3 for hosts that fell back to HTTP/2
        const H3_REPROBE_INTERVAL = 5m;

        #! Maximum number of entries in the protocol cache before eviction
        const MAX_PROTOCOL_CACHE_SIZE = 1000;
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

        #! True if this instance owns the controller (not using static or external)
        bool owns_controller = False;

        #! Logger
        *LoggerInterface logger;

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
    }

    private:internal {
        #! Static shared controller; lazily initialized via getStaticController()
        static Mutex static_ctrl_lock();
        static *AsyncSocketIo::AsyncSocketIoController static_controller;
    }

    #! Returns the shared static controller, creating it on first access
    /** All managers that don't override with setController() share a single
        I/O thread via this controller. The controller uses autostop=True so
        the thread starts when the first poll operation is submitted and stops
        when all operations complete.
    */
    private static AsyncSocketIo::AsyncSocketIoController getStaticController() {
        AutoLock al(static_ctrl_lock);
        if (!static_controller) {
            static_controller = new AsyncSocketIo::AsyncSocketIoController(True);
        }
        return static_controller;
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
        # Use the shared static controller by default; all managers share a
        # single I/O thread for efficient async operation
        controller = getStaticController();

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

    #! Sets an external AsyncSocketIoController for async operations
    /** Replaces the default shared controller with a caller-provided controller.
        Existing pool connections are migrated to the new controller.

        @param controller the I/O controller to use

        @note This is optional; by default all managers share a static
        controller with \c autostop=True. Use \c setController() only when
        you need a dedicated controller.
    */
    setController(AsyncSocketIo::AsyncSocketIoController controller) {
        # Cancel this manager's operations on the old controller
        try {
            self.controller.cancelByOwner(self.uniqueHash());
        } catch () {
        }
        # Stop a privately-owned controller
        if (owns_controller) {
            try {
                self.controller.stop();
            } catch () {
            }
        }

        # Assign the new controller and migrate connections under write lock
        # to synchronize with concurrent reads in submitConnectionToController()
        # and handleConnectionResult()
        {
            AutoWriteLock al(pool_lock);
            self.controller = controller;
            owns_controller = False;

            foreach string key in (keys pool) {
                foreach HttpClientConnection conn in (pool{key}) {
                    if (!conn.isClosed()) {
                        conn.setControllerRef(controller);
                        submitConnectionToController(key, conn);
                    }
                }
            }
        }

        log(LoggerLevel::DEBUG, "external controller set, connections migrated");
    }

    #! Returns the AsyncSocketIoController (if set)
    *AsyncSocketIo::AsyncSocketIoController getController() {
        return controller;
    }

    #! Returns True if the manager is in async controller mode
    /** @return always True; async I/O via controller is the only mode

        @since HttpClientIo 1.1
    */
    bool isControllerMode() {
        return True;
    }

    #! Acquires a stream handle for the given URL
    /** @param url the target URL (e.g., "https://example.com")

        @return a stream handle ready for submitting requests

        @throw HTTPCLIENT-URL-ERROR if the URL is invalid
        @throw HTTPCLIENT-CAPACITY-ERROR if no connection can be acquired
    */
    HttpClientStreamHandle acquireStream(string url) {
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

        HttpClientConnection conn = getConnection(url_info);

        # Create protocol-appropriate stream handle
        if (conn instanceof Http2ClientConnection) {
            return new Http2ClientStreamHandle(cast<Http2ClientConnection>(conn),
                opts.request_timeout, logger);
        } else {
            return new Http3ClientStreamHandle(conn, opts.request_timeout, logger);
        }
    }

    #! Submits an async request and returns immediately
    /** @param url the target URL
        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback callback invoked with response or error

        @return the stream handle (can be used to cancel)
    */
    HttpClientStreamHandle submitRequestAsync(string url, string method, string path,
            *hash<auto> headers, *data body, HttpResponseCallback callback) {
        HttpClientStreamHandle stream = acquireStream(url);
        stream.submitAsync(method, path, headers, body, callback);
        return stream;
    }

    #! Performs a synchronous request
    /** @param url the target URL
        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param timeout_ms request timeout (optional)

        @return the response hash
    */
    hash<auto> request(string url, string method, string path, *hash<auto> headers,
            *data body, *timeout timeout_ms) {
        HttpClientStreamHandle stream = acquireStream(url);
        return stream.request(method, path, headers, body, timeout_ms);
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
            foreach HttpClientConnection conn in (conns) {
                host_streams += conn.getActiveStreamCount();
            }
            total_connections += host_connections;
            total_active_streams += host_streams;
            per_host{key} = {
                "connections": host_connections,
                "active_streams": host_streams,
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

        # Stop the auto-created controller after closing connections
        if (owns_controller) {
            try {
                controller.stop();
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "closeAll: controller stop error: %s: %s", ex.err, ex.desc);
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
    /** @param url_info parsed URL info
        @param key the pool key "host:port"

        @return "h2" or "h3"
    */
    private string resolveProtocol(hash<auto> url_info, string key) {
        if (opts.protocol == "h2") {
            return "h2";
        }
        if (opts.protocol == "h3") {
            if (url_info.protocol != "https") {
                throw "HTTPCLIENT-URL-ERROR",
                    "HTTP/3 requires HTTPS (URL scheme must be https://)";
            }
            return "h3";
        }

        # Auto mode
        if (url_info.protocol != "https") {
            return "h2";
        }

        # Check cache
        if (protocol_cache.hasKey(key)) {
            string cached = protocol_cache{key};
            if (cached == "h3") {
                return "h3";
            }
            # Cached as "h2" — check if we should re-probe for H3
            if (h3_probe_times.hasKey(key)) {
                date elapsed = now_us() - h3_probe_times{key};
                if (elapsed < H3_REPROBE_INTERVAL) {
                    return "h2";
                }
            }
            # Time to re-probe
        }

        # Try H3 first
        return "h3";
    }

    #! Internal: gets or creates a connection for the given URL info
    private HttpClientConnection getConnection(hash<auto> url_info) {
        string key = sprintf("%s:%d", url_info.host, url_info.port);

        while (True) {
            # Try to find an existing connection with capacity
            {
                AutoReadLock al(pool_lock);
                if (shutdown) {
                    throw "HTTPCLIENT-SHUTDOWN", "connection manager is shutting down";
                }
                if (pool.hasKey(key)) {
                    foreach HttpClientConnection conn in (pool{key}) {
                        if (!conn.isClosed() && !conn.isDraining() && conn.canAcceptStream()) {
                            log(LoggerLevel::DEBUG, "reusing %s connection %d to %s",
                                conn.getProtocol(), conn.getConnectionId(), key);
                            return conn;
                        }
                    }
                }
            }

            # Serialize connection creation per key to avoid redundant QUIC handshakes.
            # Without this, N threads all finding no reusable connection would each create
            # their own, flooding the server with N handshakes when 1 suffices.
            {
                AutoLock al(create_mutex);
                if (creating{key}) {
                    # Another thread is already creating a connection for this key; wait
                    # and then re-check the pool
                    create_cond.wait(create_mutex);
                    continue;
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
                            if (!reusable && !conn.isDraining() && conn.canAcceptStream()) {
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
                        if (best.canAcceptStream()) {
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

            {
                AutoWriteLock al(pool_lock);
                if (shutdown) {
                    # Manager shut down while we were creating the connection
                    try { conn.close(); } catch () {}
                    throw "HTTPCLIENT-SHUTDOWN", "connection manager is shutting down";
                }

                if (!pool.hasKey(key)) {
                    pool{key} = ();
                }
                push pool{key}, conn;

                conn.setControllerRef(controller);
                submitConnectionToController(key, conn);

                return conn;
            }
        }
    }

    #! Internal: creates a connection with protocol fallback
    /** In auto mode, tries H3 first; on failure, falls back to H2 and caches.
    */
    private HttpClientConnection createConnection(hash<auto> url_info, string key, string protocol) {
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
        log(LoggerLevel::DEBUG, "creating new HTTP/2 connection to %s", key);
        return new Http2ClientConnection(url_info, opts.connect_timeout, logger,
            opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs);
    }

    #! Internal: submits a connection poll operation to the controller
    private submitConnectionToController(string key, HttpClientConnection conn) {
        if (!controller) {
            return;
        }

        HttpClientPollOperation op = conn.getPollOperation();
        Socket sock = conn.getSocket();
        string owner = self.uniqueHash();

        log(LoggerLevel::DEBUG, "submitting %s connection %d to controller for %s",
            conn.getProtocol(), conn.getConnectionId(), key);

        hash<SocketPollOperationInfo> info = <SocketPollOperationInfo>{
            "sock": sock,
            "spop": op,
            "owner": owner,
            "to": -1s,
            "callback": sub (hash<SocketPollResultInfo> result) {
                handleConnectionResult(key, conn, result);
            },
            "dedicated_thread": conn.getProtocol() == "h3",
        };

        controller.submit(info);
    }

    #! Internal: handles connection result from controller callback
    private handleConnectionResult(string key, HttpClientConnection conn,
            hash<SocketPollResultInfo> result) {
        if (result.canceled) {
            log(LoggerLevel::DEBUG, "connection %d canceled", conn.getConnectionId());
            return;
        }

        if (result.ex) {
            log(LoggerLevel::ERROR, "connection %d error: %s: %s",
                conn.getConnectionId(), result.ex.err, result.ex.desc);
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
            log(LoggerLevel::DEBUG, "connection %d closed", conn.getConnectionId());
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
            removeConnectionFromPool(key, conn);
            return;
        }
        submitConnectionToController(key, conn);
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
