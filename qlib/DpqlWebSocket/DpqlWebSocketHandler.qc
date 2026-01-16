# -*- mode: qore; indent-tabs-mode: nil -*-
#! DPQL WebSocket handler class definition

/*  DpqlWebSocketHandler.qc Copyright 2026 Qore Technologies, s.r.o.

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

%modern

#! Contains all public definitions in the DataProvider namespace
public namespace DataProvider {

#! WebSocket handler for DPQL language server connections
/** This handler exposes DPQL tokenization, parsing, validation, and completion
    services over WebSocket.

    @par Example
    @code{.py}
%requires HttpServer
%requires DpqlWebSocket

DataProvider::DpqlWebSocketHandler handler();
HttpServer server({"port": 8080});
server.addHandler("/ws/dpql", handler);
server.start();
    @endcode

    @see DpqlWebSocketConnection
*/
public class DpqlWebSocketHandler inherits WebSocketHandler::WebSocketHandler {
    public {
        #! Default maximum connections (0 = unlimited)
        const DefaultMaxConnections = 0;

        #! Known handler options
        const HandlerOptions = (
            "auth_callback",
            "max_connections",
        ) + WebSocketHandler::Options;
    }

    private {
        #! Optional authentication callback
        *code auth_callback;

        #! Maximum number of concurrent connections (0 = unlimited)
        int max_connections = DefaultMaxConnections;

        #! Current connection count
        int connection_count = 0;

        #! Lock for connection count
        Mutex count_lock();
    }

    #! Creates a new DPQL WebSocket handler
    /** @param auth Optional HTTP authenticator
        @param opts Handler options:
        - \c auth_callback: A callback code(hash cx, hash hdr) returns bool for custom authentication
        - \c max_connections: Maximum concurrent connections (0 = unlimited, default: 0)
        - Plus all options from WebSocketHandler (heartbeat, heartbeat_msg, logger, send_timeout)

        @throw WEBSOCKETHANDLER-OPTION-ERROR Unknown or invalid option
    */
    constructor(*HttpServer::AbstractAuthenticator auth, *hash<auto> opts)
            : WebSocketHandler(auth, opts - ("auth_callback", "max_connections")) {
        if (opts.auth_callback) {
            if (opts.auth_callback.typeCode() != NT_CLOSURE && opts.auth_callback.typeCode() != NT_CALLREF) {
                throw "WEBSOCKETHANDLER-OPTION-ERROR",
                    sprintf("Option 'auth_callback' has type %y; expecting closure or call reference",
                        opts.auth_callback.fullType());
            }
            auth_callback = opts.auth_callback;
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
    hash<auto> handleRequest(hash<auto> cx, hash<auto> hdr, *data b) {
        {
            count_lock.lock();
            on_exit count_lock.unlock();

            if (max_connections > 0 && connection_count >= max_connections) {
                return {
                    "code": 503,
                    "body": "Maximum connections reached",
                };
            }
            connection_count++;
        }

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

        hash<auto> result = WebSocketHandler::handleRequest(cx, hdr, b);
        if (result.code != 101) {
            decrementConnectionCount();
        }
        return result;
    }

    #! Creates a new connection instance
    WebSocketHandler::WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        logDebug("New DPQL connection: %s from %s", cid, cx."peer-info".hostname_desc);
        return new DpqlWebSocketConnection(self);
    }

    #! Called when a connection terminates
    deregisterConnectionImpl(WebSocketHandler::WebSocketConnection wsc) {
        {
            count_lock.lock();
            on_exit count_lock.unlock();
            connection_count--;
        }
    }

    #! Sets the authentication callback
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

    #! Decrements the connection count (thread-safe)
    private decrementConnectionCount() {
        count_lock.lock();
        on_exit count_lock.unlock();
        connection_count--;
    }
}

} # namespace DataProvider
