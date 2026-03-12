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
        *Logger logger;
    }

    #! Creates a new stream handle
    /** @param conn the connection to use for this stream
        @param request_timeout timeout for requests
        @param logger optional logger
    */
    constructor(HttpClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *Logger logger) {
        self.conn = conn;
        self.request_timeout = request_timeout;
        self.logger = logger;
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
        *data body, code callback);

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
        bool controller_mode = conn.isControllerMode();

        # Wait for connection to become ready
        if (!conn.isReady()) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
            if (controller_mode) {
                if (!conn.waitForReady(remaining)) {
                    throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
                }
            } else {
                if (!conn.driveToReady(remaining)) {
                    throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
                }
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

        if (controller_mode) {
            try {
                date remaining = end_time - now_us();
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
                throw "HTTPCLIENT-REQUEST-ERROR", sprintf("%s: %s", ex.err, ex.desc);
            }
        } else {
            # Non-controller mode: poll-driven I/O
            # Optimization: use larger poll interval for high concurrency to reduce lock
            # acquisitions (isDone() and drivePoll() both acquire the stream lock and
            # connection lock; with many concurrent requests, lock contention is a
            # major bottleneck especially in IR mode where per-call overhead is ~2x)
            timeout poll_interval = conn.getActiveStreamCount() > 1 ? 500ms : 1s;

            while (!f.isDone()) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    cancel();
                    throw "HTTPCLIENT-TIMEOUT",
                        sprintf("request timed out after %d ms", wait_timeout);
                }
                conn.drivePoll(min(remaining, poll_interval));
            }
            try {
                return f.get() ?? {};
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "HTTPCLIENT-CANCELLED") {
                    throw "HTTPCLIENT-CANCELLED", "request was cancelled";
                }
                throw "HTTPCLIENT-REQUEST-ERROR", sprintf("%s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Submits an asynchronous request with a callback
    int submitAsync(string method, string path, *hash<auto> headers, *data body,
            code callback) {
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
            if (conn.isControllerMode()) {
                if (!conn.waitForReady(request_timeout)) {
                    {
                        AutoLock al(lock);
                        if (state == HttpClientStreamState::Open) {
                            state = HttpClientStreamState::Pending;
                        }
                    }
                    throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
                }
            } else {
                if (!conn.driveToReady(request_timeout)) {
                    {
                        AutoLock al(lock);
                        if (state == HttpClientStreamState::Open) {
                            state = HttpClientStreamState::Pending;
                        }
                    }
                    throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
                }
            }
        }

        # Re-check state after readiness wait — cancel() may have run concurrently
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "submitting async request: %s %s", method, path);

        try {
            stream_id = submitAsyncImpl(method, path, headers, body, callback);
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                # Only reset to Pending if still Open — cancel() may have set Cancelled.
                # Note: if submitAsyncImpl() throws, the poll operation's
                # submitRequest() is atomic — the callback was never registered
                # and active_stream_count was never incremented, so no cleanup
                # of the poll operation is needed here.
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            rethrow;
        }

        return stream_id;
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

        if (exists sid) {
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
        }
        if (p) {
            if (err) {
                try { p.setError(err.err, err.desc, err.arg); } catch () {}
            } else {
                try { p.set(resp); } catch () {}
            }
        }
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
