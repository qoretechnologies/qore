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
            - \c persistent_request_end: Reports the end of a WebSocket connection or SSE stream that the request
              leaves open to @ref onPersistentRequestEnd(); a target that builds its own result for a request that
              the server handled must keep this field of the server's result

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

    #! Called when the WebSocket connection or SSE stream that a request left open has ended
    /** A request whose @ref onHttpRequest() returned while a WebSocket connection or an SSE stream that it set up
        stays open (the HttpServer's handlers manage such connections after the request returns: see
        @ref HttpServerAsyncIo::HttpPersistentRequestEnd "HttpPersistentRequestEnd") is reported here once that
        connection or stream has ended for any reason: a close by the client or by the server, an error, a reset or
        cancel of the stream, the removal of its listener, or a stop of the server or of the async I/O controller.
        It is called for HTTP/1.1, HTTP/2 and HTTP/3, so a target that keeps state for a request for the life of
        its connection (such as a reservation that a drain closes) can release it then.

        The call is made:
        - exactly once for each such request;
        - never before the request's @ref onHttpRequest() has returned: a connection that ends while the request is
          still being handled is reported after it returns;
        - on a thread of the handler pool, between @ref saveDispatchContext() and @ref restoreDispatchContext() as
          other user callbacks are, with no lock of the server, its handlers or the controller held; if the pool no
          longer takes tasks (it is stopping, or the controller has been deleted), on a thread of its own, without
          the dispatch context, or, if no thread can be started, on the thread that reported the end, which can hold
          locks of its own (such as a thread that closes the stream).

        A request whose @ref onHttpRequest() raised an exception, and a request whose connection is not left open
        when it returns (an ordinary response, or a WebSocket or SSE connection that its handler served while the
        request was handled), is not reported; neither is a connection opened by a request of a persistent HTTP/1.1
        session (see @ref HttpServer::AbstractHttpRequestHandler::isPersistent()), whose own request keeps the
        handler thread until the session ends, nor an HTTP/2 or HTTP/3 SSE stream whose end cannot be detected (its
        handler's setup returned no @ref AbstractSseConnection, or the controller supplied no stream-close hook; the
        server logs an error for such a stream).  An exception raised by this method is logged and ignored.

        The end of an HTTP/2 or HTTP/3 SSE stream that the server closes is reported when the close is started, which
        can be before the stream's last frame has been sent.

        The default implementation does nothing.

        @param info the information of the request as it was given to @ref onHttpRequest(): the same \c sock and
        \c header_info (with \c http2 or \c http3, \c session_id and \c stream_id for a multiplexed stream), so that
        a target can identify the request
    */
    onPersistentRequestEnd(hash<HttpConnectionInfo> info) {
    }

    #! Attempts to handle a request on the async callback dispatcher worker
    /** Returns a result hash if the request was handled inline, or @ref nothing
        if it should be dispatched to the handler thread pool via onHttpRequest().

        This combines the eligibility check and execution into a single call,
        avoiding redundant handler lookups. The default implementation returns
        @ref nothing (all requests go to the thread pool).

        @warning Only handle requests inline that are very fast and non-blocking.
        Long-running handlers will block the async callback dispatcher worker and
        delay other controller callbacks assigned to that worker.

        @param info connection and request information
        @return result hash if handled inline, or @ref nothing to dispatch to thread pool
    */
    *hash<HttpRequestResultInfo> tryInlineRequest(hash<HttpConnectionInfo> info) {
        return NOTHING;
    }

    #! Prepares a request immediately before handler-pool submission
    /** This callback runs on an async callback dispatcher worker after protocol parsing and
        persistent-session routing, but before the request enters the shared handler queue. A target can use it
        for non-blocking admission control or other dispatch reservations. Returning a result rejects or handles
        the request without consuming a handler thread; returning @ref nothing allows normal dispatch.

        A successful preparation that returns @ref nothing is paired with either @ref onHttpRequest() or
        @ref cancelPreparedHttpRequest(). The latter is called when handler-pool submission fails.

        @warning This callback must be fast and non-blocking because it runs on an async callback dispatcher
        worker.

        @param info connection and request information
        @return result hash if the request was handled before dispatch, or @ref nothing to submit it
    */
    *hash<HttpRequestResultInfo> prepareHttpRequest(hash<HttpConnectionInfo> info) {
        return NOTHING;
    }

    #! Cancels request preparation after handler-pool submission fails
    /** The default implementation is a no-op. Targets that reserve capacity in @ref prepareHttpRequest() must
        release it here. This callback is not called when preparation returns a response because no reservation
        should remain in that case.

        @param info connection and request information passed to @ref prepareHttpRequest()
    */
    cancelPreparedHttpRequest(hash<HttpConnectionInfo> info) {
    }

    #! Saves per-thread dispatch state before invoking user code on a handler-pool thread
    /** Called by @ref HttpAsyncSocketIoController on a handler-pool thread immediately
        before invoking any user-code callback (\c onHttpRequest(),
        \c onSseDisconnect(), \c onWebSocketClose(), and the WebSocket frame
        dispatch into \c WebSocketConnection::gotMessage()).  The return value is
        opaque to the controller; whatever this method returns is passed back to
        @ref restoreDispatchContext() after the user code returns or throws, on
        the same handler-pool thread.

        Subclasses can use this hook to capture and overwrite framework-specific
        thread-local data that handler code expects to find populated for the
        duration of the dispatch (for example a server-context or request-context
        global), so that handler-pool thread reuse never leaks state from one
        dispatch into the next.  The default implementation is a no-op and
        returns @ref nothing.

        @return opaque save data to be passed to @ref restoreDispatchContext() at dispatch end
    */
    auto saveDispatchContext() {
        return NOTHING;
    }

    #! Restores per-thread dispatch state after invoking user code on a handler-pool thread
    /** Called by @ref HttpAsyncSocketIoController on the same handler-pool thread
        that invoked the corresponding user-code callback, after the callback
        returns or throws.  Subclasses must restore whatever state
        @ref saveDispatchContext() captured.  The default implementation is a
        no-op.

        @param save_data whatever @ref saveDispatchContext() returned
    */
    restoreDispatchContext(auto save_data) {
    }
}

} # namespace HttpServerAsyncIo
