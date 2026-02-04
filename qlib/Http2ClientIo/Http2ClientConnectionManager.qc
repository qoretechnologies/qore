# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientConnectionManager.qc Connection pool for HTTP/2 multiplexed clients

/** Http2ClientConnectionManager.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace Http2ClientIo {

#! Connection pool manager for HTTP/2 multiplexed client connections
/** This class manages a pool of HTTP/2 connections, providing:

    - Connection pooling: Reuse connections across requests
    - Load balancing: Distribute streams across connections
    - Automatic connection creation: Create new connections as needed
    - Idle timeout handling: Close idle connections
    - GOAWAY handling: Graceful connection shutdown

    @par Basic Usage
    @code{.py}
%requires Http2ClientIo

# Create a connection manager with default settings
Http2ClientConnectionManager manager();

# Acquire a stream for a request
Http2ClientStreamHandle stream = manager.acquireStream("https://example.com");
hash<auto> response = stream.request("GET", "/api/users");
    @endcode

    @par With Custom Options
    @code{.py}
%requires Http2ClientIo

# Create manager with custom settings
Http2ClientConnectionManager manager(<Http2ClientConnectionManagerOptions>{
    "max_connections_per_host": 4,
    "max_streams_per_connection": 50,
    "idle_timeout": 30s,
});

# Acquire a stream and make a synchronous request
Http2ClientStreamHandle stream = manager.acquireStream("https://example.com");
hash<auto> response = stream.request("GET", "/api/item/1");
printf("Response: %y\n", response.body);

# Or use async with callbacks
stream = manager.acquireStream("https://example.com");
stream.submitAsync("GET", "/api/item/2", NOTHING, NOTHING,
    sub (hash<auto> response, *hash<auto> error) {
        if (error) {
            printf("Error: %y\n", error);
        } else {
            printf("Response: %y\n", response.body);
        }
    });
    @endcode

    @note AsyncSocketIoController integration for event-driven I/O is planned
    for a future release. Currently, connections operate in synchronous mode.

    @par Connection Sharing
    When multiple clients use the same manager, they automatically share
    connections to the same host:port, enabling true HTTP/2 multiplexing
    across the application.

*/
public class Http2ClientConnectionManager {
    private {
        #! Options for this manager
        hash<Http2ClientConnectionManagerOptions> opts;

        #! Connection pool: "host:port" -> list of connections
        hash<string, list<Http2ClientConnection>> pool;

        #! Lock for thread-safe access to pool
        RWLock pool_lock();

        #! AsyncSocketIoController for async operations
        *object controller;

        #! Logger
        *Logger logger;

        #! Shutdown flag
        bool shutdown = False;
    }

    #! Creates a new connection manager with the given options
    /** @param opts configuration options
    */
    constructor(*hash<Http2ClientConnectionManagerOptions> opts) {
        self.opts = opts ?? <Http2ClientConnectionManagerOptions>{};
        self.logger = self.opts.logger;
        log(LoggerLevel::DEBUG, "connection manager created: max_conn=%d max_streams=%d idle=%d",
            self.opts.max_connections_per_host,
            self.opts.max_streams_per_connection,
            self.opts.idle_timeout / 1s);
    }

    #! Destructor - closes all connections
    destructor() {
        closeAll();
    }

    #! Sets the AsyncSocketIoController for async operations
    /** @param controller the I/O controller to use
    */
    setController(object controller) {
        self.controller = controller;
    }

    #! Returns the AsyncSocketIoController (if set)
    *object getController() {
        return controller;
    }

    #! Acquires a stream handle for the given URL
    /** @param url the target URL (e.g., "https://example.com")

        @return a stream handle ready for submitting requests

        @throw HTTP2-CONNECTION-ERROR if unable to acquire a connection
    */
    Http2ClientStreamHandle acquireStream(string url) {
        hash<UrlInfo> url_info;
        try {
            url_info = parse_url(url);
        } catch (hash<ExceptionInfo> ex) {
            throw "HTTP2-URL-ERROR", sprintf("invalid URL %y: %s", url, ex.desc);
        }
        if (!url_info.host) {
            throw "HTTP2-URL-ERROR", sprintf("invalid URL: %y (missing host)", url);
        }
        if (!url_info.port) {
            url_info.port = url_info.protocol == "https" ? 443 : 80;
        }
        if (!url_info.protocol) {
            url_info.protocol = "https";
        }

        # Get or create a connection
        Http2ClientConnection conn = getConnection(url_info);

        # Create and return a stream handle
        return new Http2ClientStreamHandle(conn, opts.request_timeout, logger);
    }

    #! Submits an async request and returns immediately
    /** This is a convenience method that acquires a stream, submits the request,
        and invokes the callback when the response is ready.

        @param url the target URL
        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback callback invoked with response or error

        @return the stream handle (can be used to cancel)
    */
    Http2ClientStreamHandle submitRequestAsync(string url, string method, string path,
            *hash<auto> headers, *data body, code callback) {
        Http2ClientStreamHandle stream = acquireStream(url);
        stream.submitAsync(method, path, headers, body, callback);
        return stream;
    }

    #! Performs a synchronous request
    /** This is a convenience method that acquires a stream, submits the request,
        and waits for the response.

        @param url the target URL
        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param timeout_ms request timeout (optional)

        @return the response hash
    */
    hash<auto> request(string url, string method, string path, *hash<auto> headers,
            *data body, *timeout timeout_ms) {
        Http2ClientStreamHandle stream = acquireStream(url);
        return stream.request(method, path, headers, body, timeout_ms);
    }

    #! Returns connection pool statistics
    hash<auto> getStats() {
        AutoReadLock al(pool_lock);

        int total_connections = 0;
        int total_active_streams = 0;
        hash<auto> per_host = {};

        foreach string key in (keys pool) {
            list<Http2ClientConnection> conns = pool{key};
            int host_connections = conns.size();
            int host_streams = 0;
            foreach Http2ClientConnection conn in (conns) {
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
        AutoWriteLock al(pool_lock);
        shutdown = True;

        foreach string key in (keys pool) {
            foreach Http2ClientConnection conn in (pool{key}) {
                try {
                    conn.close();
                } catch () {
                    # Ignore close errors
                }
            }
        }
        remove pool;
    }

    #! Removes idle connections older than the idle timeout
    /** Call this periodically to clean up idle connections.
    */
    cleanupIdleConnections() {
        date cutoff = now_us() - opts.idle_timeout;

        AutoWriteLock al(pool_lock);
        foreach string key in (keys pool) {
            list<Http2ClientConnection> conns = pool{key};
            list<Http2ClientConnection> keep = ();

            foreach Http2ClientConnection conn in (conns) {
                if (conn.isClosed()) {
                    log(LoggerLevel::DEBUG, "removing closed connection to %s", key);
                    continue;
                }
                if (conn.getActiveStreamCount() == 0 && conn.getLastActivity() < cutoff) {
                    log(LoggerLevel::DEBUG, "closing idle connection to %s", key);
                    conn.close();
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

    #! Internal: gets or creates a connection for the given URL info
    private Http2ClientConnection getConnection(hash<auto> url_info) {
        string key = sprintf("%s:%d", url_info.host, url_info.port);

        # Try to find an existing connection with capacity
        {
            AutoReadLock al(pool_lock);
            if (shutdown) {
                throw "HTTP2-SHUTDOWN", "connection manager is shutting down";
            }
            if (pool.hasKey(key)) {
                foreach Http2ClientConnection conn in (pool{key}) {
                    if (conn.canAcceptStream()) {
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
                throw "HTTP2-SHUTDOWN", "connection manager is shutting down";
            }

            # Check again in case another thread created one
            if (pool.hasKey(key)) {
                foreach Http2ClientConnection conn in (pool{key}) {
                    if (conn.canAcceptStream()) {
                        return conn;
                    }
                }
            }

            # Check if we can create a new connection
            int current_count = pool.hasKey(key) ? pool{key}.size() : 0;
            if (current_count >= opts.max_connections_per_host) {
                # All connections are at capacity, pick the one with fewest streams
                Http2ClientConnection best;
                int min_streams = 2147483647;
                foreach Http2ClientConnection conn in (pool{key}) {
                    int streams = conn.getActiveStreamCount();
                    if (streams < min_streams) {
                        min_streams = streams;
                        best = conn;
                    }
                }
                if (best) {
                    log(LoggerLevel::DEBUG, "using connection %d with %d streams to %s (all at capacity)",
                        best.getConnectionId(), min_streams, key);
                    return best;
                }
                throw "HTTP2-CONNECTION-ERROR",
                    sprintf("all %d connections to %s are full", current_count, key);
            }

            # Create new connection
            log(LoggerLevel::DEBUG, "creating new connection to %s", key);
            Http2ClientConnection conn(url_info, opts.connect_timeout, logger);

            # Add to pool
            if (!pool.hasKey(key)) {
                pool{key} = ();
            }
            push pool{key}, conn;

            # TODO: Controller integration for HTTP/2 client multiplexing requires
            # a different approach than one-shot operations. The multiplex poll operation
            # is long-running and needs continuous driving. For now, connections work
            # in synchronous mode. Future work: implement a dedicated HTTP/2 client
            # event loop or extend AsyncSocketIoController for persistent connections.

            return conn;
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            logger.log(level, "Http2ClientConnectionManager: " + fmt, argv);
        }
    }
}

} # namespace Http2ClientIo
