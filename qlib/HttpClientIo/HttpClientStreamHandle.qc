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

    @par Asynchronous Usage
    @code{.py}
HttpClientStreamHandle stream = manager.acquireStream("https://example.com");
stream.submitAsync("GET", "/api/users", NOTHING, NOTHING,
    sub (hash<auto> response, *hash<auto> error) {
        printf("Response: %y\n", response);
    });
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

        #! Promise for synchronous request() -- set/cleared per call
        *Promise request_promise;

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
    /** @par Contract
        Implementations MUST call handleResponse() with the response/error before
        invoking the user \c callback.  The synchronous request() API creates a
        Promise that is resolved inside handleResponse(); failing to call it will
        cause request() to hang indefinitely.  Exceptions from handleResponse()
        should be caught, logged, and converted to an error passed to \c callback.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback user callback to invoke after handleResponse()

        @return the stream ID assigned to this request
    */
    abstract int submitAsyncImpl(string method, string path, *hash<auto> headers,
        *data body, HttpResponseCallback callback);

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

        # Create Promise/Future for this request
        Promise p();
        {
            AutoLock al(lock);
            request_promise = p;
        }
        on_exit {
            AutoLock al(lock);
            request_promise = NOTHING;
        }
        Future f = p.getFuture();

        submitAsync(method, path, headers, body, \handleAsyncResponse());
        @assert(exists stream_id);

        try {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                cancel();
                throw "HTTPCLIENT-TIMEOUT",
                    sprintf("request timed out after %d ms", wait_timeout);
            }
            return f.get(remaining) ?? {};
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
        # Wait for connection to become ready
        if (!conn.isReady()) {
            if (!conn.waitForReady(request_timeout)) {
                releaseReservationIfHeld();
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Create Promise/Future — store Promise as member to keep it alive
        # through the submitAsyncImpl closure chain (which captures self)
        Promise p();
        {
            AutoLock al(lock);
            request_promise = p;
        }
        Future f = p.getFuture();

        try {
            submitAsync(method, path, headers, body, \handleAsyncResponse());
        } catch () {
            # Clear request_promise on failure so the Promise is not leaked;
            # handleResponse() will never be called for a failed submission
            {
                AutoLock al(lock);
                remove request_promise;
            }
            rethrow;
        }

        return f;
    }

    #! Submits an asynchronous request with a callback
    int submitAsync(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback) {
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

        try {
            stream_id = submitAsyncImpl(method, path, headers, body, callback);
            @assert(stream_id >= 0);
            # Reservation converted to active stream
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

        return stream_id;
    }

    #! Submits an asynchronous request without blocking on connection establishment
    /** If the connection is already ready, the request is submitted immediately.
        Otherwise, submission is deferred until the connection becomes ready via
        @ref HttpClientConnection::callWhenReady().

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback user callback invoked when the response is complete

        @note This method never blocks. Safe to call from event loop threads.

        @since %HttpClientIo 1.1
    */
    submitAsyncNonBlocking(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        if (conn.isReady()) {
            doSubmitAsyncImpl(method, path, headers, body, callback);
        } else {
            log(LoggerLevel::DEBUG, "deferring async request until connection ready: %s %s", method, path);
            conn.callWhenReady(sub () {
                try {
                    {
                        AutoLock al(lock);
                        if (state != HttpClientStreamState::Open) {
                            releaseReservationIfHeld();
                            callback(NOTHING, {
                                "err": "HTTPCLIENT-CANCELLED",
                                "desc": "stream was cancelled during connection setup",
                            });
                            return;
                        }
                    }
                    doSubmitAsyncImpl(method, path, headers, body, callback);
                } catch (hash<ExceptionInfo> ex) {
                    callback(NOTHING, ex);
                }
            });
        }
    }

    #! Internal: performs the actual async submission
    private doSubmitAsyncImpl(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback) {
        log(LoggerLevel::DEBUG, "submitting async request: %s %s", method, path);
        try {
            stream_id = submitAsyncImpl(method, path, headers, body, callback);
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
        @param callback user callback invoked when the response is complete
        @param datagram the datagram payload to send

        @return the stream ID assigned to this request

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if connection establishment timed out
        @throw HTTPCLIENT-CANCELLED if the stream was cancelled during connection setup
        @throw QUIC-DATAGRAM-NOT-SUPPORTED if the protocol does not support datagrams
    */
    int submitAsyncWithDatagram(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback, binary datagram) {
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

        try {
            stream_id = submitAsyncWithDatagramImpl(method, path, headers, body,
                callback, datagram);
            @assert(stream_id >= 0);
            # Reservation converted to active stream
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

        return stream_id;
    }

    #! Protocol-specific async request + datagram submission (subclasses override)
    /** Default implementation throws — only HTTP/3 supports QUIC datagrams.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback user callback to invoke after handleResponse()
        @param datagram the datagram payload to send

        @return the stream ID assigned to this request

        @throw QUIC-DATAGRAM-NOT-SUPPORTED if the protocol does not support datagrams

        @see submitAsyncWithDatagram() for the public wrapper with state management
    */
    private int submitAsyncWithDatagramImpl(string method, string path, *hash<auto> headers,
            *data body, HttpResponseCallback callback, binary datagram) {
        throw "QUIC-DATAGRAM-NOT-SUPPORTED",
            "QUIC datagrams are only supported over HTTP/3; this stream uses "
            + conn.getProtocol();
    }

    #! Cancels the request
    cancel() {
        *int sid;
        *Promise p;
        {
            AutoLock al(lock);
            if (state == HttpClientStreamState::Complete
                    || state == HttpClientStreamState::Cancelled
                    || state == HttpClientStreamState::Error) {
                return;
            }
            state = HttpClientStreamState::Cancelled;
            sid = stream_id;
            p = request_promise;
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

        # Note: cancelRequest() above may have already resolved the Promise via
        # the poll operation's callback → handleResponse().  The try/catch
        # silently handles the double-resolve race (Promise.setError throws if
        # already resolved).  Either resolution carries a cancellation error.
        if (p) {
            try { p.setError("HTTPCLIENT-CANCELLED", "stream cancelled"); } catch () {}
        }
    }

    #! Internal: placeholder callback for the sync request() path
    /** This is intentionally a no-op.  The actual response handling is done by
        handleResponse(), which concrete submitAsyncImpl() implementations call
        before invoking this callback.  The Promise is already resolved by the
        time this method runs.
    */
    private handleAsyncResponse(hash<auto> resp, *hash<auto> err) {
    }

    #! Internal: handles response (called from protocol-specific submit callback)
    private handleResponse(hash<auto> resp, *hash<auto> err) {
        if (!err) {
            resp = normalizeResponseBody(resp);
        }
        *Promise p;
        {
            AutoLock al(lock);
            if (err) {
                error_info = err;
                state = HttpClientStreamState::Error;
            } else {
                response = resp;
                state = HttpClientStreamState::Complete;
            }
            p = request_promise;
            # Clear the member reference so the Promise is not leaked in the
            # async path (the sync request() path has its own on_exit cleanup)
            remove request_promise;
        }
        if (p) {
            if (err) {
                try { p.setError(err.err, err.desc, err.arg); } catch () {}
            } else {
                try { p.set(resp); } catch () {}
            }
        }
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
