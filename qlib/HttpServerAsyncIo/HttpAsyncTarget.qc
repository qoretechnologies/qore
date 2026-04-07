# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpAsyncTarget.qc Defines the HttpAsyncTarget abstract class

/** HttpAsyncTarget.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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
public namespace HttpServerAsyncIo {

#! Abstract class for receiving async HTTP events
/** This abstract class defines the callbacks that the HttpAsyncSocketIoController
    uses to notify the application about HTTP protocol events.

    Implementations should handle:
    - New HTTP requests (onHttpRequest)
    - Idle connection timeouts (onIdleTimeout)
    - I/O errors (onError)
    - SSE client disconnects (onSseDisconnect)
    - WebSocket frames (onWebSocketFrame)
    - WebSocket connection closes (onWebSocketClose)

    @note Callback signatures use @ref HttpServer::HttpListenerInterface for the
    \c listener parameter.  In practice the runtime object is always an
    \c HttpListener (which inherits both \c Socket and \c HttpListenerInterface),
    but only the \c HttpListenerInterface surface should be used — direct
    socket I/O on the listener is not supported in async mode.
*/
public class HttpAsyncTarget {
    #! Called when an HTTP request is ready (header fully parsed)
    /** This is the main callback for handling HTTP requests. The implementation
        should:
        1. Find the appropriate handler based on URL/content-type
        2. Process the request
        3. Return instructions for connection handling

        @note This method is called from a worker thread in the handler thread pool,
        not from the I/O thread. Implementations must be thread-safe, as multiple
        requests may be processed concurrently across different worker threads.

        @param info Connection and request information (see @ref HttpConnectionInfo)
        @return Instructions for connection handling (see @ref HttpRequestResultInfo):
            - \c close: \c True to close connection after response
            - \c return_to_idle: \c True to return to keep-alive pool
            - \c streaming: \c True for SSE/WebSocket streaming
            - \c stream_type: \c "sse" or \c "websocket"
            - \c stream_operation: The poll operation for streaming
            - \c ttl: Keep-alive timeout
            - \c code: HTTP response code (for HTTP/2 streaming responses)
            - \c hdr: Response headers (for HTTP/2 responses sent through h2op)
            - \c body: Response body (for HTTP/2 responses sent through h2op)
            - \c input_stream: Streaming response body (InputStream for HTTP/2 responses)
            - \c status_message: HTTP status message text (e.g. \c "OK", \c "Not Found")
            - \c chunked: \c True if the response uses chunked transfer encoding (HTTP/1.x)
            - \c dedicated: \c True when a dedicated handler manages the socket lifecycle
            - \c send_callback: Callback for chunked response data
            - \c send_timeout: Timeout for send_callback responses

        @see HttpRequestResultInfo
        @see HttpConnectionInfo
    */
    abstract hash<HttpRequestResultInfo> onHttpRequest(hash<HttpConnectionInfo> info);

    #! Called when an idle connection times out
    /** This is called when a keep-alive connection has been idle for longer
        than the configured TTL. The implementation should close the socket.

        @param sock The socket that timed out
        @param listener The listener that accepted this connection
        @param cx Connection context
    */
    abstract onIdleTimeout(Socket sock, *HttpServer::HttpListenerInterface listener, hash<auto> cx);

    #! Called on I/O errors
    /** This is called when an I/O error occurs during async operations.
        The implementation should log the error and close the socket.

        @param sock The socket where the error occurred
        @param ex Exception information
        @param listener The listener for this connection
        @param cx Connection context (may be empty for errors during accept)
    */
    abstract onError(Socket sock, hash<ExceptionInfo> ex, *HttpServer::HttpListenerInterface listener, *hash<auto> cx);

    #! Called when an SSE client disconnects
    /** This is called when the client closes an SSE connection or the
        connection is lost. The implementation should clean up any
        subscriber registrations.

        @param sock The socket that disconnected
        @param listener The listener for this connection
        @param cx Connection context
    */
    abstract onSseDisconnect(Socket sock, *HttpServer::HttpListenerInterface listener, hash<auto> cx);

    #! Called when a WebSocket frame is received
    /** This is called for text and binary WebSocket frames. Ping frames
        are handled automatically (pong is sent), but the callback is still
        invoked for notification.

        @param sock The WebSocket socket
        @param frame Frame information (see @ref WebSocketFrameInfo)
        @param listener The listener for this connection
        @param cx Connection context

        @see WebSocketFrameInfo
    */
    abstract onWebSocketFrame(Socket sock, hash<WebSocketFrameInfo> frame, *HttpServer::HttpListenerInterface listener, hash<auto> cx);

    #! Called when a WebSocket connection closes
    /** This is called when the WebSocket close handshake completes or
        when the connection is lost unexpectedly.

        @param sock The WebSocket socket
        @param code Close status code (1000 = normal, 1006 = abnormal, etc.)
        @param reason Close reason text
        @param listener The listener for this connection
        @param cx Connection context
    */
    abstract onWebSocketClose(Socket sock, int code, string reason, *HttpServer::HttpListenerInterface listener, hash<auto> cx);

    #! Attempts to handle a request inline in the I/O thread
    /** Returns a result hash if the request was handled inline, or @ref nothing
        if it should be dispatched to the handler thread pool via onHttpRequest().

        This combines the eligibility check and execution into a single call,
        avoiding redundant handler lookups. The default implementation returns
        @ref nothing (all requests go to the thread pool).

        @warning Only handle requests inline that are very fast and non-blocking.
        Long-running handlers will block the I/O callback thread and stall all other
        connections on that I/O callback thread.

        @param info connection and request information
        @return result hash if handled inline, or @ref nothing to dispatch to thread pool

        @since HttpServerAsyncIo 1.0
    */
    *hash<HttpRequestResultInfo> tryInlineRequest(hash<HttpConnectionInfo> info) {
        return NOTHING;
    }

}

} # namespace HttpServerAsyncIo
