# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientConnectionManager.qc Connection pool for HTTP/3 multiplexed clients over QUIC

/** Http3ClientConnectionManager.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace Http3ClientIo {

#! Connection pool manager for HTTP/3 multiplexed client connections over QUIC
/** This class manages a pool of QUIC/HTTP/3 connections, providing:

    - Connection pooling: Reuse connections across requests
    - Load balancing: Distribute streams across connections
    - Automatic connection creation: Create new connections as needed
    - Idle timeout handling: Close idle connections

    @par Basic Usage
    @code{.py}
%requires Http3ClientIo

# Create a connection manager with default settings
Http3ClientConnectionManager manager();

# Acquire a stream for a request
Http3ClientStreamHandle stream = manager.acquireStream("https://example.com");
hash<auto> response = stream.request("GET", "/api/users");
    @endcode

    @par Async Mode with AsyncSocketIoController
    @code{.py}
%requires Http3ClientIo
%requires AsyncSocketIo

# Create manager with async controller
Http3ClientConnectionManager manager();
AsyncSocketIoController controller(False);
manager.setController(controller);
controller.start();

# Submit async request with callback
manager.submitRequestAsync("https://example.com", "GET", "/api/resource",
    NOTHING, NOTHING,
    sub (hash<auto> response, *hash<auto> error) {
        if (error) {
            printf("Error: %y\n", error);
        } else {
            printf("Response: %y\n", response);
        }
    });

controller.stop();
manager.closeAll();
    @endcode
*/
public class Http3ClientConnectionManager {
    private {
        #! Options for this manager
        hash<Http3ClientConnectionManagerOptions> opts;

        #! Connection pool: "host:port" -> list of connections
        hash<string, list<Http3ClientConnection>> pool;

        #! Lock for thread-safe access to pool
        RWLock pool_lock();

        #! AsyncSocketIoController for async operations
        *AsyncSocketIo::AsyncSocketIoController controller;

        #! True if using controller for async mode
        bool controllerMode = False;

        #! Logger
        *Logger logger;

        #! Shutdown flag
        bool shutdown = False;
    }

    #! Creates a new connection manager with the given options
    constructor(*hash<Http3ClientConnectionManagerOptions> opts) {
        self.opts = opts ?? <Http3ClientConnectionManagerOptions>{};
        self.logger = self.opts.logger;
        log(LoggerLevel::DEBUG, "connection manager created: max_conn=%d max_streams=%d idle=%d",
            self.opts.max_connections_per_host,
            self.opts.max_streams_per_connection,
            self.opts.idle_timeout / 1s);
    }

    #! Copying is not supported (holds mutable state: connection pool, locks, controller references)
    private copy() {
        throw "COPY-ERROR", "Http3ClientConnectionManager objects cannot be copied";
    }

    #! Destructor - closes all connections
    destructor() {
        try {
            closeAll();
        } catch () {
            # Destructors must not propagate exceptions
        }
    }

    #! Sets the AsyncSocketIoController for async operations
    /** @note Must be called before any requests are submitted; not thread-safe
        with concurrent request submission.
    */
    setController(AsyncSocketIo::AsyncSocketIoController controller) {
        self.controller = controller;
        self.controllerMode = True;
        log(LoggerLevel::DEBUG, "controller set, async mode enabled");
    }

    #! Returns the AsyncSocketIoController (if set)
    *AsyncSocketIo::AsyncSocketIoController getController() {
        return controller;
    }

    #! Returns True if the manager is in async controller mode
    bool isControllerMode() {
        return controllerMode;
    }

    #! Acquires a stream handle for the given URL
    Http3ClientStreamHandle acquireStream(string url) {
        hash<UrlInfo> url_info;
        try {
            url_info = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "HTTP3-URL-ERROR", sprintf("invalid URL %y: %s", url, ex.desc);
        }
        if (!url_info.host) {
            throw "HTTP3-URL-ERROR", sprintf("invalid URL: %y (missing host)", url);
        }
        # HTTP/3 is always HTTPS
        if (!url_info.protocol) {
            url_info.protocol = "https";
        }
        if (!url_info.port) {
            url_info.port = 443;
        }

        Http3ClientConnection conn = getConnection(url_info);
        return new Http3ClientStreamHandle(conn, opts.request_timeout, logger);
    }

    #! Submits an async request and returns immediately
    Http3ClientStreamHandle submitRequestAsync(string url, string method, string path,
            *hash<auto> headers, *data body, code callback) {
        Http3ClientStreamHandle stream = acquireStream(url);
        stream.submitAsync(method, path, headers, body, callback);
        return stream;
    }

    #! Performs a synchronous request
    hash<auto> request(string url, string method, string path, *hash<auto> headers,
            *data body, *timeout timeout_ms) {
        Http3ClientStreamHandle stream = acquireStream(url);
        return stream.request(method, path, headers, body, timeout_ms);
    }

    #! Returns connection pool statistics
    hash<auto> getStats() {
        AutoReadLock al(pool_lock);

        int total_connections = 0;
        int total_active_streams = 0;
        hash<auto> per_host = {};

        foreach string key in (keys pool) {
            list<Http3ClientConnection> conns = pool{key};
            int host_connections = conns.size();
            int host_streams = 0;
            foreach Http3ClientConnection conn in (conns) {
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
        };
    }

    #! Closes all connections
    closeAll() {
        # Set shutdown flag first to prevent new connections from being created
        # between cancelByOwner() and pool draining
        {
            AutoWriteLock al(pool_lock);
            shutdown = True;
        }

        if (controllerMode && controller) {
            try {
                controller.cancelByOwner(self.uniqueHash());
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "closeAll: cancelByOwner error: %s: %s", ex.err, ex.desc);
            }
        }

        # Copy the pool and release the lock before closing connections,
        # since close() may trigger callbacks that acquire locks
        hash<string, list<Http3ClientConnection>> pool_copy;
        {
            AutoWriteLock al(pool_lock);
            if (pool) {
                pool_copy = pool;
                remove pool;
            }
        }
        foreach string key in (keys pool_copy) {
            foreach Http3ClientConnection conn in (pool_copy{key}) {
                try {
                    conn.close();
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::DEBUG, "closeAll: close error for %s: %s: %s",
                        key, ex.err, ex.desc);
                }
            }
        }
    }

    #! Migrates all ready connections to new network paths
    /** Iterates all connections in the pool and calls migrate() on each connection
        that is in the Ready state. Connections that fail to migrate are marked as
        draining (a new connection will be created on the next request).

        @return the number of connections successfully migrated

        @since %Qore 2.3
    */
    int migrateAll() {
        # Collect ready connections under read lock
        list<hash<auto>> to_migrate = ();
        {
            AutoReadLock al(pool_lock);
            foreach string key in (keys pool) {
                foreach Http3ClientConnection conn in (pool{key}) {
                    if (conn.isReady()) {
                        push to_migrate, {"key": key, "conn": conn};
                    }
                }
            }
        }

        # Exception isolation: each connection is migrated independently in a
        # try/catch so that one failure does not prevent migration of the rest.
        # Failed connections are marked as draining (not removed) so they can
        # complete any in-flight requests before being replaced.  The collect-
        # then-iterate pattern avoids holding pool_lock during migration I/O;
        # Qore reference counting keeps connection objects alive even if another
        # thread removes them from the pool between collect and migrate.
        int migrated = 0;
        foreach hash<auto> item in (to_migrate) {
            try {
                item.conn.migrate();
                ++migrated;
                log(LoggerLevel::DEBUG, "migrated connection %d to %s",
                    item.conn.getConnectionId(), item.key);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "migration failed for connection %d to %s: %s: %s",
                    item.conn.getConnectionId(), item.key, ex.err, ex.desc);
                # Mark as draining — a new connection will be created on the next request
                try {
                    item.conn.setDraining();
                } catch () {
                    # Ignore errors during setDraining
                }
            }
        }

        log(LoggerLevel::DEBUG, "migrateAll: %d/%d connections migrated", migrated, to_migrate.size());
        return migrated;
    }

    #! Removes idle connections older than the idle timeout
    cleanupIdleConnections() {
        date cutoff = now_us() - opts.idle_timeout;

        # Collect connections to close under the lock, then close outside the lock
        # to avoid deadlock (close() may trigger callbacks that acquire locks)
        list<Http3ClientConnection> to_close = ();
        {
            AutoWriteLock al(pool_lock);
            foreach string key in (keys pool) {
                list<Http3ClientConnection> conns = pool{key};
                list<Http3ClientConnection> keep = ();

                foreach Http3ClientConnection conn in (conns) {
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
        # Close connections outside the lock
        foreach Http3ClientConnection conn in (to_close) {
            try {
                conn.close();
            } catch () {
                # Ignore close errors
            }
        }
    }

    #! Internal: gets or creates a connection for the given URL info
    /** @note Connection pooling uses host:port as the key, matching the HTTP/2
        multiplexing pattern (Http2ClientIo).  QUIC multiplexes multiple streams
        over a single connection, so one connection per host:port is sufficient;
        additional connections are only created when the stream limit is reached.
    */
    private Http3ClientConnection getConnection(hash<auto> url_info) {
        string key = sprintf("%s:%d", url_info.host, url_info.port);

        # Try to find an existing connection with capacity
        {
            AutoReadLock al(pool_lock);
            if (shutdown) {
                throw "HTTP3-SHUTDOWN", "connection manager is shutting down";
            }
            if (pool.hasKey(key)) {
                foreach Http3ClientConnection conn in (pool{key}) {
                    if (!conn.isClosed() && !conn.isDraining() && conn.canAcceptStream()) {
                        log(LoggerLevel::DEBUG, "reusing connection %d to %s",
                            conn.getConnectionId(), key);
                        return conn;
                    }
                }
            }
        }

        # Need to create a new connection
        {
            AutoWriteLock al(pool_lock);
            if (shutdown) {
                throw "HTTP3-SHUTDOWN", "connection manager is shutting down";
            }

            # Evict dead connections before checking capacity or selecting a candidate
            if (pool.hasKey(key)) {
                list<Http3ClientConnection> live = ();
                *Http3ClientConnection reusable;
                foreach Http3ClientConnection conn in (pool{key}) {
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

            # Check capacity
            int current_count = pool.hasKey(key) ? pool{key}.size() : 0;
            if (current_count >= opts.max_connections_per_host) {
                # Pick connection with fewest streams
                list<Http3ClientConnection> candidates;
                int min_streams = MAXINT;
                foreach Http3ClientConnection conn in (pool{key}) {
                    int streams = conn.getActiveStreamCount();
                    if (streams < min_streams) {
                        min_streams = streams;
                        candidates = (conn,);
                    } else if (streams == min_streams) {
                        push candidates, conn;
                    }
                }
                if (candidates) {
                    Http3ClientConnection best = candidates.size() == 1
                        ? candidates[0]
                        : candidates[rand() % candidates.size()];
                    # Only return the connection if it can actually accept a new stream;
                    # otherwise throw a clear pool-level error rather than letting the
                    # caller get a confusing HTTP3-CAPACITY-ERROR from submitRequest()
                    if (best.canAcceptStream()) {
                        log(LoggerLevel::DEBUG, "using connection %d with %d streams to %s "
                            "(at connection limit)", best.getConnectionId(), min_streams, key);
                        return best;
                    }
                }
                throw "HTTP3-POOL-EXHAUSTED",
                    sprintf("all %d connections to %s have reached the concurrent stream limit "
                        "(min active streams per connection: %d); retry later or increase "
                        "max_connections_per_host",
                        current_count, key,
                        candidates ? min_streams : 0);
            }

            # Create new QUIC connection
            log(LoggerLevel::DEBUG, "creating new QUIC connection to %s", key);
            Http3ClientConnection conn(url_info, opts.connect_timeout, logger,
                opts.max_streams_per_connection, opts.ssl_verify_mode, opts.accept_all_certs,
                opts.client_cert, opts.client_pk);

            if (!pool.hasKey(key)) {
                pool{key} = ();
            }
            push pool{key}, conn;

            if (controllerMode && controller) {
                conn.setControllerRef(controller);
                submitConnectionToController(key, conn);
            }

            return conn;
        }
    }

    #! Internal: submits a connection poll operation to the controller
    private submitConnectionToController(string key, Http3ClientConnection conn) {
        if (!controller) {
            return;
        }

        Http3ClientPollOperation op = conn.getPollOperation();
        Socket sock = conn.getSocket();
        string owner = self.uniqueHash();

        log(LoggerLevel::DEBUG, "submitting connection %d to controller for %s",
            conn.getConnectionId(), key);

        hash<SocketPollOperationInfo> info = <SocketPollOperationInfo>{
            "sock": sock,
            "spop": op,
            "owner": owner,
            "to": -1s,
            "callback": sub (hash<SocketPollResultInfo> result) {
                handleConnectionResult(key, conn, result);
            },
        };

        controller.submit(info);
    }

    #! Internal: handles connection result from controller callback
    private handleConnectionResult(string key, Http3ClientConnection conn,
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

        Http3ClientPollOperation op = conn.getPollOperation();
        if (op.isClosed() || op.hasError()) {
            log(LoggerLevel::DEBUG, "connection %d closed", conn.getConnectionId());
            removeConnectionFromPool(key, conn);
            return;
        }

        # If GOAWAY received, the connection is draining — don't remove it yet
        # (in-flight streams will still complete), but log it
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
    private removeConnectionFromPool(string key, Http3ClientConnection conn) {
        AutoWriteLock al(pool_lock);
        if (!pool.hasKey(key)) {
            return;
        }
        list<Http3ClientConnection> conns = pool{key};
        list<Http3ClientConnection> keep = ();
        foreach Http3ClientConnection c in (conns) {
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

    #! Internal: logging helper
    private log(int level, string fmt, ...) {
        if (logger) {
            call_function_args(\logger.log(), (level, "Http3ClientConnectionManager: " + fmt) + argv);
        }
    }
}

} # namespace Http3ClientIo
