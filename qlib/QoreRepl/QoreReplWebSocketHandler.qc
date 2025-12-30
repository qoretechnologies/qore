# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore QoreReplWebSocketHandler class definition

/** QoreReplWebSocketHandler.qc Copyright 2025 Qore Technologies, s.r.o.

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

%require-types
%enable-all-warnings
%new-style
%strict-args

%requires HttpServerUtil

public namespace QoreRepl {

#! WebSocket handler for REPL connections
/** This handler can be added to an HttpServer to provide WebSocket-based REPL access.

    @par Example: Basic Setup
    @code{.py}
%requires HttpServer
%requires WebSocketHandler
%requires QoreRepl

# Create handler
QoreReplWebSocketHandler handler();

# Add to HTTP server
HttpServer server(hash<HttpServerOptionInfo>{"port": 8080});
server.addHandler("/ws/repl", handler);
server.start();
    @endcode

    @par Example: With Authentication
    @code{.py}
# Use callback authentication
code auth_callback = sub (hash<auto> cx, hash<auto> hdr) returns bool {
    return hdr."x-api-key" == "secret123";
};

QoreReplWebSocketHandler handler({"auth_callback": auth_callback});
    @endcode

    @par Security Considerations
    Running a REPL over WebSocket allows arbitrary code execution. Always:
    - Use TLS (wss://) in production
    - Implement authentication via setAuthCallback() or HttpServer authentication
    - Consider network-level restrictions (firewall, VPN)
    - Use restrictive parse options if appropriate

    @see QoreReplWebSocketConnection
    @see WebSocketHandler::WebSocketHandler
*/
public class QoreReplWebSocketHandler inherits WebSocketHandler::WebSocketHandler {
    public {
        #! Default maximum connections (0 = unlimited)
        const DefaultMaxConnections = 0;

        #! Known handler options
        const HandlerOptions = (
            "auth_callback",
            "repl_options",
            "max_connections",
        ) + WebSocketHandler::Options;
    }

    private {
        #! Optional authentication callback
        *code auth_callback;

        #! Options for creating QoreRepl instances
        *hash<auto> repl_options;

        #! Maximum number of concurrent connections (0 = unlimited)
        int max_connections = DefaultMaxConnections;

        #! Current connection count
        int connection_count = 0;

        #! Lock for connection count
        Mutex count_lock();
    }

    #! Creates a new REPL WebSocket handler
    /** @param auth Optional HTTP authenticator
        @param opts Handler options:
        - \c auth_callback: A callback code(hash cx, hash hdr) returns bool for custom authentication
        - \c repl_options: Options hash to pass to QoreRepl constructor for each connection
        - \c max_connections: Maximum concurrent connections (0 = unlimited, default: 0)
        - Plus all options from WebSocketHandler (heartbeat, heartbeat_msg, logger, send_timeout)

        @throw WEBSOCKETHANDLER-OPTION-ERROR Unknown or invalid option
    */
    constructor(*HttpServer::AbstractAuthenticator auth, *hash<auto> opts)
            : WebSocketHandler(auth, opts - ("auth_callback", "repl_options", "max_connections")) {
        if (opts.auth_callback) {
            if (opts.auth_callback.typeCode() != NT_CLOSURE && opts.auth_callback.typeCode() != NT_CALLREF) {
                throw "WEBSOCKETHANDLER-OPTION-ERROR",
                    sprintf("Option 'auth_callback' has type %y; expecting closure or call reference",
                        opts.auth_callback.fullType());
            }
            auth_callback = opts.auth_callback;
        }

        if (opts.repl_options) {
            if (opts.repl_options.typeCode() != NT_HASH) {
                throw "WEBSOCKETHANDLER-OPTION-ERROR",
                    sprintf("Option 'repl_options' has type %y; expecting hash",
                        opts.repl_options.fullType());
            }
            repl_options = opts.repl_options;
        }

        if (exists opts.max_connections) {
            if (!opts.max_connections.intp() || opts.max_connections < 0) {
                throw "WEBSOCKETHANDLER-OPTION-ERROR",
                    sprintf("Option 'max_connections' value %y is not a non-negative integer",
                        opts.max_connections);
            }
            max_connections = opts.max_connections;
        }
    }

    #! Handle incoming HTTP request for WebSocket upgrade
    /** Validates the request and optionally authenticates via callback.

        @param cx Call context hash
        @param hdr HTTP headers
        @param b Request body

        @return Response hash with code 101 to accept, or error code to reject
    */
    hash<auto> handleRequest(hash<auto> cx, hash<auto> hdr, *data b) {
        # Check max connections with lock held for the entire check-and-increment
        # to prevent race conditions where multiple requests see the same count
        {
            count_lock.lock();
            on_exit count_lock.unlock();

            if (max_connections > 0 && connection_count >= max_connections) {
                return {
                    "code": 503,
                    "body": "Maximum connections reached",
                };
            }
            # Pre-increment connection count here while holding the lock
            # to reserve our slot before the WebSocket upgrade
            connection_count++;
        }

        # Custom authentication callback
        if (auth_callback) {
            try {
                if (!auth_callback(cx, hdr)) {
                    decrementConnectionCount();
                    return {
                        "code": 401,
                        "body": "Unauthorized",
                    };
                }
            } catch (hash<ExceptionInfo> ex) {
                decrementConnectionCount();
                logError("Auth callback error: %s: %s", ex.err, ex.desc);
                return {
                    "code": 500,
                    "body": "Authentication error",
                };
            }
        }

        # Delegate to parent for WebSocket upgrade
        hash<auto> result = WebSocketHandler::handleRequest(cx, hdr, b);
        # If the upgrade fails, decrement the connection count
        if (result.code != 101) {
            decrementConnectionCount();
        }
        return result;
    }

    #! Decrements the connection count (thread-safe)
    private decrementConnectionCount() {
        count_lock.lock();
        on_exit count_lock.unlock();
        connection_count--;
    }

    #! Creates a new connection instance
    /** @param cx Call context hash
        @param hdr HTTP headers
        @param cid Connection ID

        @return A new QoreReplWebSocketConnection instance
    */
    WebSocketHandler::WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        # Note: connection_count was already incremented in handleRequest() to prevent
        # race conditions between the max_connections check and the WebSocket upgrade
        logDebug("New REPL connection: %s from %s", cid, cx."peer-info".hostname_desc);
        return new QoreReplWebSocketConnection(self, repl_options);
    }

    #! Called when a connection terminates
    /** @param wsc The connection being closed
    */
    deregisterConnectionImpl(WebSocketHandler::WebSocketConnection wsc) {
        # Decrement connection count
        {
            count_lock.lock();
            on_exit count_lock.unlock();
            connection_count--;
        }

        if (wsc instanceof QoreReplWebSocketConnection) {
            QoreReplWebSocketConnection repl_conn = cast<QoreReplWebSocketConnection>(wsc);
            logDebug("REPL connection closed: %s", repl_conn.getSessionId());
        }
    }

    #! Sets the authentication callback
    /** @param callback A callback code(hash cx, hash hdr) returns bool

        The callback receives the HTTP context and headers, and should return
        True to allow the connection or False to reject it.

        @par Example
        @code{.py}
handler.setAuthCallback(sub (hash<auto> cx, hash<auto> hdr) returns bool {
    return hdr."x-api-key" == "secret123";
});
        @endcode
    */
    setAuthCallback(code callback) {
        auth_callback = callback;
    }

    #! Clears the authentication callback
    clearAuthCallback() {
        delete auth_callback;
    }

    #! Returns the current connection count
    int getConnectionCount() {
        count_lock.lock();
        on_exit count_lock.unlock();
        return connection_count;
    }

    #! Returns the maximum connections setting
    int getMaxConnections() {
        return max_connections;
    }

    #! Sets the maximum connections
    /** @param max Maximum connections (0 = unlimited)
    */
    setMaxConnections(int max) {
        if (max < 0) {
            throw "INVALID-ARGUMENT", sprintf("'max' value %d is not a non-negative integer", max);
        }
        count_lock.lock();
        on_exit count_lock.unlock();
        max_connections = max;
    }

    #! Returns the REPL options used for new connections
    *hash<auto> getReplOptions() {
        return repl_options;
    }

    #! Sets the REPL options for new connections
    /** @param opts Options hash to pass to QoreRepl constructor
    */
    setReplOptions(*hash<auto> opts) {
        repl_options = opts;
    }
}

} # namespace QoreRepl
