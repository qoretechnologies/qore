# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpClientStreamHandle.qc Abstract base for HTTP client stream handles

/** HttpClientStreamHandle.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Abstract base class for HTTP client stream handles
/** This class provides a convenient handle for a single HTTP stream,
    supporting both synchronous and asynchronous request patterns.

    @par Synchronous Usage
    @code{.py}
HttpClientStreamHandle stream = manager.acquireStream("https://example.com");
hash<auto> response = stream.request("GET", "/api/users");
printf("Status: %d\n", response.status_code);
    @endcode

    @par Asynchronous Usage (Future API)
    @code{.py}
HttpClientStreamHandle stream = manager.acquireStream("https://example.com");
hash<auto> result = stream.submitAsync("GET", "/api/users");
hash<auto> response = result.future.get(30s);
printf("Status: %d\n", response.status_code);
    @endcode

    @par Streaming Usage (SSE, gRPC, etc.)
    @code{.py}
HttpClientStreamHandle stream = manager.acquireStream("https://example.com");
if (stream.supportsStreaming()) {
    string body = make_json({"jsonrpc": "2.0", "method": "message/stream", "id": 1,
        "params": {"message": {"role": "user", "parts": ({"type": "text", "text": "Hello"},)}}});
    stream.startStreaming("POST", "/events",
        {"Accept": "text/event-stream", "Content-Type": "application/json"}, False, body);
    while (True) {
        auto data = stream.readData(30s);
        if (!exists data) {
            continue;  # timeout, retry
        }
        if (data.typeCode() == NT_HASH) {
            if (data.end_stream) {
                break;  # stream complete
            }
            if (data.error) {
                throw "STREAM-ERROR", sprintf("%y", data.error);
            }
            continue;
        }
        # data is binary or string — process it
        printf("Received: %y\n", data);
    }
}
    @endcode
*/
public class HttpClientStreamHandle {
    private {
        #! The connection this stream is on
        HttpClientConnection conn;

        #! Stream ID (set after request submitted)
        *int stream_id;

        #! Stream state
        HttpClientStreamState state = HttpClientStreamState::Pending;

        #! Lock for thread safety
        Mutex lock();

        #! The response when complete
        *hash<auto> response;

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Request timeout
        timeout request_timeout;

        #! Logger
        *LoggerInterface logger;

        #! True if a pending stream reservation is held on the connection
        bool reservation_held = False;
    }

    #! Creates a new stream handle
    /** @param conn the connection to use for this stream
        @param request_timeout timeout for requests
        @param logger optional logger
    */
    constructor(HttpClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *LoggerInterface logger) {
        self.conn = conn;
        self.request_timeout = request_timeout;
        self.logger = logger;
    }

    #! Releases any unreleased stream reservation
    destructor() {
        releaseReservationIfHeld();
    }

    #! Protocol-specific async request submission (subclasses must implement)
    /** Submits the request and returns a hash with the stream ID and a Future
        for the response. The Future is resolved by a C++ completion action on
        the I/O thread (PromiseAction for request/response, ChannelAction for
        streaming) — no user code runs on the I/O thread.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)

        @return hash with "stream_id" (int) and "future" (Future)
    */
    abstract hash<auto> submitAsyncImpl(string method, string path, *hash<auto> headers,
        *data body);

    #! Protocol-specific async request submission with EventNotifier signaling
    /** Override in subclasses to use submitRequestWithNotifier (H1/H2) or
        registerStreamWithPromiseAndNotifier (H3).

        @return hash with "stream_id" (int) and "future" (Future)
    */
    abstract hash<auto> submitAsyncWithNotifierImpl(string method, string path,
        *hash<auto> headers, *data body, EventNotifier notifier);

    #! Returns the stream ID (NOTHING if not yet submitted)
    *int getStreamId() {
        return stream_id;
    }

    #! Returns the stream state
    HttpClientStreamState getState() {
        AutoLock al(lock);
        return state;
    }

    #! Returns True if the stream is complete or closed
    bool isDone() {
        AutoLock al(lock);
        return state == HttpClientStreamState::Complete
            || state == HttpClientStreamState::Cancelled
            || state == HttpClientStreamState::Error;
    }

    #! Returns True if the stream is in error state
    bool hasError() {
        AutoLock al(lock);
        return exists error_info;
    }

    #! Returns error info if an error occurred
    *hash<auto> getError() {
        AutoLock al(lock);
        return error_info;
    }

    #! Returns True if a request has been submitted on this stream
    bool isSubmitted() {
        return exists stream_id;
    }

    #! Marks that a pending stream reservation is held on the connection
    /** Called by @ref HttpClientConnectionManager::acquireStream() after
        @ref HttpClientConnection::tryReserveStream() succeeds.  The
        reservation is released automatically when the stream is submitted,
        when a pre-submit failure occurs (timeout, cancel), or when this
        handle is destroyed without submitting.
    */
    markReservationHeld() {
        reservation_held = True;
    }

    #! Clears the reservation-held flag (caller already released manually)
    markReservationReleased() {
        reservation_held = False;
    }

    #! Releases the stream reservation if one is held
    /** Centralizes reservation cleanup for all pre-submit failure paths
        (timeout, cancel, error) so \c pending_stream_count is freed promptly
        even if the handle is retained by the caller.
    */
    private releaseReservationIfHeld() {
        if (reservation_held) {
            reservation_held = False;
            conn.releaseStreamReservation();
        }
    }

    #! Returns the underlying connection
    HttpClientConnection getConnection() {
        return conn;
    }

    #! Performs a synchronous request and returns the response
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param timeout_ms request timeout (uses default if not specified)

        @return the response hash with status_code, headers, body

        @throw HTTPCLIENT-REQUEST-ERROR if the request fails
        @throw HTTPCLIENT-TIMEOUT if the request times out
    */
    hash<auto> request(string method, string path, *hash<auto> headers, *data body,
            *timeout timeout_ms) {
        timeout wait_timeout = timeout_ms ?? request_timeout;
        date end_time = now_us() + wait_timeout;

        # Wait for connection to become ready
        if (!conn.isReady()) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
            if (!conn.waitForReady(remaining)) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Check stream state — cancel may have fired during connection wait
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending
                    && state != HttpClientStreamState::Open) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("cannot submit request in state: %y", state);
            }
        }

        # Submit request — returns {stream_id, future}
        # The Future is resolved by C++ PromiseAction on the I/O thread
        # (pure C++, no user code on I/O thread)
        hash<auto> submit_result = conn.submitRequest(method, path, headers, body);
        stream_id = submit_result.stream_id;
        Future f = submit_result.future;
        reservation_held = False;

        try {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                cancel();
                throw "HTTPCLIENT-TIMEOUT",
                    sprintf("request timed out after %d ms", wait_timeout);
            }
            auto rv = f.get(remaining);
            {
                AutoLock al(lock);
                state = HttpClientStreamState::Complete;
                response = rv;
            }
            return normalizeResponseBody(rv ?? {});
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "FUTURE-TIMEOUT") {
                cancel();
                throw "HTTPCLIENT-TIMEOUT",
                    sprintf("request timed out after %d ms", wait_timeout);
            }
            if (ex.err == "HTTPCLIENT-CANCELLED") {
                throw "HTTPCLIENT-CANCELLED", "request was cancelled";
            }
            # Let connection errors propagate unwrapped so the connection
            # manager can detect, evict dead connections, and retry
            if (HttpClientIo::HttpClientConnectionManager::isConnectionErrorStatic(ex.err)) {
                rethrow;
            }
            throw "HTTPCLIENT-REQUEST-ERROR", sprintf("%s: %s", ex.err, ex.desc);
        }
    }

    #! Submits a request and returns a Future for the raw response
    /** Like \c request() but non-blocking: the Future resolves when the response
        arrives. The I/O is driven by the controller's event loop.

        @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)

        @return a Future that resolves to the raw response hash

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if the connection establishment times out

        @note The caller must keep the returned Future alive (or call \c get())
        to receive the response. The stream handle is kept alive internally
        until the response is received.
    */
    Future requestAsync(string method, string path, *hash<auto> headers, *data body) {
        hash<auto> result = submitAsync(method, path, headers, body);
        return result.future;
    }

    #! Submits an async request and returns a Future for the response
    /** @return hash with "stream_id" (int) and "future" (Future)
    */
    hash<auto> submitAsync(string method, string path, *hash<auto> headers, *data body) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        # Wait for connection to become ready if not already
        if (!conn.isReady()) {
            if (!conn.waitForReady(request_timeout)) {
                {
                    AutoLock al(lock);
                    if (state == HttpClientStreamState::Open) {
                        state = HttpClientStreamState::Pending;
                    }
                }
                releaseReservationIfHeld();
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Re-check state after readiness wait — cancel() may have run concurrently
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "submitting async request: %s %s", method, path);

        hash<auto> result;
        try {
            result = submitAsyncImpl(method, path, headers, body);
            stream_id = result.stream_id;
            @assert(stream_id >= 0);
            reservation_held = False;
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            rethrow;
        }

        return result;
    }

    #! Submits an async request and signals an EventNotifier on completion
    /** Like submitAsync() but uses a CompositeAction (PromiseAction + EventNotifierAction)
        so the I/O thread signals @a notifier when the response arrives.  The caller's
        continuePoll() can then call Future::get() non-blockingly — no background thread.

        @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param notifier EventNotifier to signal on response completion

        @return hash with "stream_id" (int) and "future" (Future)

        @since %HttpClientIo 1.2
    */
    hash<auto> submitAsyncWithNotifier(string method, string path, *hash<auto> headers,
            *data body, EventNotifier notifier) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        # The caller must ensure the connection is ready before calling this method.
        # submitAsyncWithNotifier() must never block (waitForReady() would deadlock
        # if called from the async I/O thread, since the I/O thread drives the
        # connection to READY).  If the connection is not ready, throw immediately.
        if (!conn.isReady()) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            throw "HTTPCLIENT-NOT-READY",
                "connection is not ready for async request; "
                "caller must ensure readiness via SPS_CONNECTING before calling submitAsyncWithNotifier()";
        }

        # Re-check state
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "submitting async request with notifier: %s %s", method, path);

        hash<auto> result;
        try {
            result = submitAsyncWithNotifierImpl(method, path, headers, body, notifier);
            stream_id = result.stream_id;
            @assert(stream_id >= 0);
            reservation_held = False;
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            rethrow;
        }

        return result;
    }

    #! Submits an asynchronous request without blocking on connection establishment
    /** If the connection is not ready, throws HTTPCLIENT-STATE-ERROR.
        Use waitForReady() first if needed, or use submitAsync() which blocks.

        @return hash with "stream_id" (int) and "future" (Future)

        @note This method never blocks. The Future is resolved by a C++ PromiseAction
        on the I/O thread (pure C++, no user code on I/O thread).

        @since %HttpClientIo 1.1
    */
    hash<auto> submitAsyncNonBlocking(string method, string path, *hash<auto> headers, *data body) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        if (!conn.isReady()) {
            {
                AutoLock al(lock);
                state = HttpClientStreamState::Pending;
            }
            releaseReservationIfHeld();
            throw "HTTPCLIENT-STATE-ERROR", "connection not ready; use submitAsync() or waitForReady()";
        }

        log(LoggerLevel::DEBUG, "submitting async request (non-blocking): %s %s", method, path);
        hash<auto> result;
        try {
            result = submitAsyncImpl(method, path, headers, body);
            stream_id = result.stream_id;
            @assert(stream_id >= 0);
            reservation_held = False;
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            rethrow;
        }
        return result;
    }

    #! Submits an asynchronous request with a QUIC datagram atomically
    /** Queues both the HTTP request and a QUIC datagram before waking the
        I/O controller, ensuring both are sent in the same write cycle.

        This avoids a race condition where the I/O controller processes the
        request HEADERS before the datagram is queued, causing the server to
        time out waiting for the datagram.

        @param method HTTP method (typically "POST")
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param datagram the datagram payload to send

        @return the stream ID assigned to this request

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if connection establishment timed out
        @throw HTTPCLIENT-CANCELLED if the stream was cancelled during connection setup
        @throw QUIC-DATAGRAM-NOT-SUPPORTED if the protocol does not support datagrams
    */
    hash<auto> submitAsyncWithDatagram(string method, string path, *hash<auto> headers, *data body,
            binary datagram) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        # Wait for connection to become ready if not already
        if (!conn.isReady()) {
            if (!conn.waitForReady(request_timeout)) {
                {
                    AutoLock al(lock);
                    if (state == HttpClientStreamState::Open) {
                        state = HttpClientStreamState::Pending;
                    }
                }
                releaseReservationIfHeld();
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Re-check state after readiness wait — cancel() may have run concurrently
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "submitting async request with datagram: %s %s", method, path);

        hash<auto> result;
        try {
            result = submitAsyncWithDatagramImpl(method, path, headers, body, datagram);
            stream_id = result.stream_id;
            @assert(stream_id >= 0);
            reservation_held = False;
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            rethrow;
        }

        return result;
    }

    #! Protocol-specific async request + datagram submission (subclasses override)
    /** Default implementation throws — only HTTP/3 supports QUIC datagrams.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param datagram the datagram payload to send

        @return the stream ID assigned to this request

        @throw QUIC-DATAGRAM-NOT-SUPPORTED if the protocol does not support datagrams

        @see submitAsyncWithDatagram() for the public wrapper with state management
    */
    private hash<auto> submitAsyncWithDatagramImpl(string method, string path, *hash<auto> headers,
            *data body, binary datagram) {
        throw "QUIC-DATAGRAM-NOT-SUPPORTED",
            "QUIC datagrams are only supported over HTTP/3; this stream uses "
            + conn.getProtocol();
    }

    #! Cancels the request
    cancel() {
        *int sid;
        {
            AutoLock al(lock);
            if (state == HttpClientStreamState::Complete
                    || state == HttpClientStreamState::Cancelled
                    || state == HttpClientStreamState::Error) {
                return;
            }
            state = HttpClientStreamState::Cancelled;
            sid = stream_id;
        }

        # Release reservation if the stream was never submitted
        if (!exists sid) {
            releaseReservationIfHeld();
        } else {
            log(LoggerLevel::DEBUG, "cancelling stream %d", sid);
            try {
                conn.cancelRequest(sid);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "cancel stream %d: %s: %s", sid, ex.err, ex.desc);
            }
        }
        # Note: cancelRequest() above rejects the Promise in the poll operation,
        # which resolves the Future with a cancellation error.
    }

    #! Returns an EventNotifier that fires when the connection becomes ready (or closes)
    /** If the underlying connection is already ready (or in a terminal state), returns
        \c NOTHING — the caller can proceed immediately without polling.

        Otherwise, registers an EventNotifier with the connection's C++ priv via
        \c AbstractHttpPollConnection::registerReadyNotifier().  When the I/O thread
        transitions the connection to READY or CLOSED, it signals the notifier.
        The caller should poll the notifier FD for \c SOCK_POLLIN and check
        \c conn.isReady() when it fires to determine whether to proceed or raise an error.

        @return an EventNotifier to poll, or NOTHING if already decided

        @since %HttpClientIo 1.2
    */
    *EventNotifier getConnectionReadyNotifier() {
        if (conn.isReady()) {
            return NOTHING;
        }
        EventNotifier notifier();
        # registerReadyNotifier returns True if queued (CONNECTING), False if already decided
        if (!conn.registerReadyNotifier(notifier)) {
            # State transitioned (READY/DRAINING/CLOSED) between isReady() and registration
            return NOTHING;
        }
        return notifier;
    }

    #! Internal: converts binary body to string for text-based content types
    /** Ensures consistent body types across all HTTP protocol versions (H1, H2, H3).
        The H1 and H2 layers already perform this conversion internally, so this is
        effectively a no-op for those protocols (body is already a string). For H3
        (QUIC), the C++ layer returns bodies as binary, so this normalizes the output.

        The charset parameter from the Content-Type header is used for encoding
        conversion when present; JSON and YAML content types always use UTF-8
        per RFC 8259 / YAML spec regardless of any charset parameter.
    */
    private static hash<auto> normalizeResponseBody(hash<auto> resp) {
        if (resp.body && resp.body.typeCode() == NT_BINARY) {
            *string ct = resp.headers."content-type";
            if (ct && (ct =~ /^text\// || ct =~ /\/(json|xml|yaml|html|javascript)/
                    || ct =~ /\+(json|xml)/ || ct =~ /charset=/
                    || ct =~ /x-www-form-urlencoded/)) {
                # JSON and YAML are always UTF-8 per their respective specs
                bool force_utf8 = ct =~ /\/(json|x-yaml|yaml)/ || ct =~ /\+json/;
                *string charset;
                if (!force_utf8) {
                    *list<*string> m = (ct =~ x/charset=["']?([^\s;,"']+)/i);
                    if (m) {
                        charset = m[0];
                    }
                }
                resp.body = resp.body.toString(charset ?? "UTF-8");
            }
        }
        return resp;
    }

    #! Starts a streaming request on this stream handle
    /** Opens an HTTP stream for incremental data delivery. For response-only streaming
        (e.g., SSE), set \c body_streaming to \c False (default); the request is sent with
        END_STREAM and the server streams back data. For bidirectional streaming (e.g., gRPC,
        WebSocket), set \c body_streaming to \c True.

        @param method HTTP method (e.g., "GET", "POST", "CONNECT")
        @param path request path
        @param headers request headers (optional)
        @param body_streaming if \c True, keep the request body stream open for bidirectional
        communication; if \c False (default), submit with END_STREAM for response-only streaming
        @param body one-shot request body (optional); sent with the request before streaming
        begins — use for patterns like POST with JSON body + streaming SSE response

        @return the stream ID

        @throw HTTPCLIENT-STREAMING-NOT-SUPPORTED if the protocol does not support streaming
        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if connection establishment times out


        @see readData(), sendData(), writesDone()
    */
    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False, *data body) {
        throw "HTTPCLIENT-STREAMING-NOT-SUPPORTED",
            "streaming is not supported by " + conn.getProtocol();
    }

    #! Reads data from a streaming connection
    /** Returns the next chunk of data from the stream. The return value depends on the stream
        state:
        - \c binary or \c string: actual body data
        - \c hash with \c "end_stream": \c True: signals that the stream is complete
        - \c hash with \c "error": stream error information
        - \c NOTHING: timeout (no data within \c timeout_ms)

        @param timeout_ms timeout for waiting for data; if not specified, blocks indefinitely

        @return data chunk, end-of-stream sentinel, error hash, or NOTHING on timeout

        @throw HTTPCLIENT-STREAMING-NOT-SUPPORTED if the protocol does not support streaming
        @throw HTTPCLIENT-STREAM-CLOSED if the stream has already been closed


        @see startStreaming()
    */
    auto readData(*timeout timeout_ms) {
        throw "HTTPCLIENT-STREAMING-NOT-SUPPORTED",
            "streaming is not supported by " + conn.getProtocol();
    }

    #! Sends data on a streaming connection
    /** Sends data on the write side of a bidirectional stream. Only meaningful when
        \c startStreaming() was called with \c body_streaming = \c True.

        @param data the data to send (string or binary)
        @param end_stream if \c True, signals end of the write side (half-close)

        @throw HTTPCLIENT-STREAMING-NOT-SUPPORTED if the protocol does not support streaming
        @throw HTTPCLIENT-STATE-ERROR if the stream is not in streaming mode


        @see startStreaming(), writesDone()
    */
    sendData(data data, bool end_stream = False) {
        throw "HTTPCLIENT-STREAMING-NOT-SUPPORTED",
            "streaming is not supported by " + conn.getProtocol();
    }

    #! Closes the write side of a streaming connection
    /** Convenience method that sends an empty payload with \c end_stream = \c True.
        Override if the protocol has a dedicated half-close mechanism.


        @see sendData()
    */
    writesDone() {
        sendData(binary(), True);
    }

    #! Returns trailers from the completed response
    /** @return trailers hash, or NOTHING if no trailers or not yet complete

        @since HttpClientIo 1.3
    */
    *hash<string, string> getTrailers() {
        return NOTHING;
    }

    #! Returns True if this stream handle supports streaming
    /** @return \c True if \c startStreaming(), \c readData(), and \c sendData() are available

        @since HttpClientIo 1.3
    */
    bool supportsStreaming() {
        return False;
    }

    #! Send a QUIC DATAGRAM frame on this stream's datagram channel (RFC 9221/9297)
    /** @param data the datagram payload to send

        @throw QUIC-DATAGRAM-NOT-SUPPORTED if the protocol does not support QUIC datagrams
        (e.g., HTTP/2)
    */
    sendDatagram(binary data) {
        throw "QUIC-DATAGRAM-NOT-SUPPORTED",
            "QUIC datagrams are only supported over HTTP/3; this stream uses "
            + conn.getProtocol();
    }

    #! Read the next incoming QUIC datagram on this stream's channel (RFC 9221/9297)
    /** @param timeout_ms read timeout

        @return the datagram payload, or NOTHING on timeout

        @throw QUIC-DATAGRAM-NOT-SUPPORTED if the protocol does not support QUIC datagrams
    */
    *binary readDatagram(timeout timeout_ms = 30s) {
        throw "QUIC-DATAGRAM-NOT-SUPPORTED",
            "QUIC datagrams are only supported over HTTP/3; this stream uses "
            + conn.getProtocol();
    }

    #! Check if the remote peer supports QUIC datagrams on this connection
    /** @return True if datagrams are supported, False otherwise
    */
    bool isDatagramSupported() {
        return False;
    }

    #! Get the maximum datagram payload size for this connection
    /** @return maximum payload size in bytes, or 0 if datagrams not supported
    */
    int getMaxDatagramSize() {
        return 0;
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            string prefix = exists stream_id
                ? sprintf("HttpClientStreamHandle[%d/%s]: ", stream_id, conn.getProtocol())
                : sprintf("HttpClientStreamHandle[%s]: ", conn.getProtocol());
            call_function_args(\logger.log(), (level, prefix + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
