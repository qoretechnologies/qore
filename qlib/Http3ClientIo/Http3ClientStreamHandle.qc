# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientStreamHandle.qc Per-request handle for HTTP/3 client streams

/** Http3ClientStreamHandle.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Per-request handle for HTTP/3 client streams
/** This class provides a convenient handle for a single HTTP/3 stream,
    supporting both synchronous and asynchronous request patterns.

    @par Synchronous Usage
    @code{.py}
Http3ClientStreamHandle stream = manager.acquireStream("https://example.com");

# Submit request and block until response
hash<auto> response = stream.request("GET", "/api/users");
printf("Status: %d\n", response.status_code);
printf("Body: %y\n", response.body);
    @endcode

    @par Asynchronous Usage
    @code{.py}
Http3ClientStreamHandle stream = manager.acquireStream("https://example.com");

# Submit request with callback
stream.submitAsync("GET", "/api/users", NOTHING, NOTHING,
    sub (hash<auto> response, *hash<auto> error) {
        if (error) {
            printf("Error: %y\n", error);
        } else {
            printf("Response: %y\n", response);
        }
    });
    @endcode
*/
public class Http3ClientStreamHandle {
    private {
        #! The connection this stream is on
        Http3ClientConnection conn;

        #! Stream ID (set after request submitted)
        *int stream_id;

        #! Stream state
        Http3ClientStreamState state = Http3ClientStreamState::Pending;

        #! Lock for thread safety
        Mutex lock();

        #! Condition for waiting on response
        Condition response_cond();

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
    constructor(Http3ClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *Logger logger) {
        self.conn = conn;
        self.request_timeout = request_timeout;
        self.logger = logger;
    }

    #! Prevent copying — Mutex/Condition members are not safely copyable
    private copy() {
        throw "COPY-ERROR", "Http3ClientStreamHandle objects cannot be copied";
    }

    #! Returns the stream ID (NOTHING if not yet submitted)
    *int getStreamId() {
        return stream_id;
    }

    #! Returns the stream state
    Http3ClientStreamState getState() {
        AutoLock al(lock);
        return state;
    }

    #! Returns True if the stream is complete or closed
    bool isDone() {
        AutoLock al(lock);
        return state == Http3ClientStreamState::Complete
            || state == Http3ClientStreamState::Cancelled
            || state == Http3ClientStreamState::Error;
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
    Http3ClientConnection getConnection() {
        return conn;
    }

    #! Performs a synchronous request and returns the response
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param timeout_ms request timeout (uses default if not specified)

        @return the response hash with status_code, headers, body

        @throw HTTP3-REQUEST-ERROR if the request fails
        @throw HTTP3-TIMEOUT if the request times out
    */
    hash<auto> request(string method, string path, *hash<auto> headers, *data body,
            *timeout timeout_ms) {
        timeout wait_timeout = timeout_ms ?? request_timeout;
        date end_time = now_us() + wait_timeout;
        bool controller_mode = conn.isControllerMode();

        # Wait for connection to become ready (uses remaining budget from end_time,
        # so the overall timeout is not doubled)
        if (!conn.isReady()) {
            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                throw "HTTP3-TIMEOUT", "connection establishment timed out";
            }
            if (controller_mode) {
                if (!conn.waitForReady(remaining)) {
                    throw "HTTP3-TIMEOUT", "connection establishment timed out";
                }
            } else {
                if (!conn.driveToReady(remaining)) {
                    throw "HTTP3-TIMEOUT", "connection establishment timed out";
                }
            }
        }

        # Submit async and wait
        submitAsync(method, path, headers, body, \handleAsyncResponse());

        # Wait for response
        {
            AutoLock al(lock);
            while (state != Http3ClientStreamState::Complete
                    && state != Http3ClientStreamState::Error
                    && state != Http3ClientStreamState::Cancelled) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    delete al;
                    cancel();
                    throw "HTTP3-TIMEOUT",
                        sprintf("request timed out after %d ms", int(wait_timeout));
                }

                if (controller_mode) {
                    response_cond.wait(lock, remaining);
                } else {
                    # Adaptive timeout: short poll for multi-stream (other
                    # streams need prompt service), longer for single-stream
                    # to reduce wakeups — matches HTTP/2 pattern
                    timeout poll_to = min(remaining,
                        conn.getActiveStreamCount() > 1 ? 100ms : 1s);
                    delete al;
                    conn.drivePoll(poll_to);
                    al = new AutoLock(lock);
                }
            }

            if (error_info) {
                throw "HTTP3-REQUEST-ERROR",
                    sprintf("%s: %s", error_info.err, error_info.desc);
            }

            if (state == Http3ClientStreamState::Cancelled) {
                throw "HTTP3-REQUEST-CANCELLED", "request was cancelled";
            }

            return response ?? {};
        }
    }

    #! Submits an asynchronous request with a callback
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback callback invoked when response is ready

        @return the stream ID assigned to this request

        @throw HTTP3-STATE-ERROR if the stream is already in use
    */
    int submitAsync(string method, string path, *hash<auto> headers, *data body,
            code<auto(hash<auto>, *hash<auto>)> callback) {
        {
            AutoLock al(lock);
            if (state != Http3ClientStreamState::Pending) {
                throw "HTTP3-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = Http3ClientStreamState::Open;
        }

        # Wait for connection to become ready.
        # NOTE: uses request_timeout (not a caller-provided remaining time) because
        # submitAsync() is a public standalone entry point.  When called from
        # request(), the connection is already ready so this block is skipped.
        if (!conn.isReady()) {
            if (conn.isControllerMode()) {
                if (!conn.waitForReady(request_timeout)) {
                    {
                        AutoLock al(lock);
                        state = Http3ClientStreamState::Pending;
                    }
                    throw "HTTP3-TIMEOUT", "connection establishment timed out";
                }
            } else {
                if (!conn.driveToReady(request_timeout)) {
                    {
                        AutoLock al(lock);
                        state = Http3ClientStreamState::Pending;
                    }
                    throw "HTTP3-TIMEOUT", "connection establishment timed out";
                }
            }
        }

        log(LoggerLevel::DEBUG, "submitting async request: %s %s", method, path);

        try {
            stream_id = conn.submitRequest(method, path, headers, body,
                sub (hash<auto> resp, *hash<auto> err) {
                    try {
                        handleResponse(resp, err);
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "handleResponse error: %s: %s", ex.err, ex.desc);
                        # Ensure user callback is always invoked even if internal
                        # state update fails; clear resp to maintain the contract
                        # that callback receives either resp OR err, not both
                        delete resp;
                        if (!err) {
                            err = {"err": ex.err, "desc": ex.desc};
                        }
                    }
                    callback(resp, err);
                });
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                state = Http3ClientStreamState::Pending;
            }
            rethrow;
        }

        return stream_id;
    }

    #! Cancels the request
    /** @note After cancel(), the async callback may still fire and call
        handleResponse() which overwrites state to Error. Sync request()
        callers never observe the transient Cancelled state. This matches
        HTTP/2 behavior by design.
    */
    cancel() {
        *int sid;
        {
            AutoLock al(lock);
            if (state == Http3ClientStreamState::Complete
                    || state == Http3ClientStreamState::Cancelled
                    || state == Http3ClientStreamState::Error) {
                return;
            }
            state = Http3ClientStreamState::Cancelled;
            sid = stream_id;
        }

        if (exists sid) {
            log(LoggerLevel::DEBUG, "cancelling stream %d", sid);
            try {
                conn.cancelRequest(sid);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "cancel stream %d: %s: %s", sid, ex.err, ex.desc);
            }
        }

        {
            AutoLock al(lock);
            response_cond.broadcast();
        }
    }

    #! Internal: handles async response for sync requests
    /** Response state is already updated by submitAsync closure's handleResponse() call;
        this callback only needs to exist as the callback parameter for submitAsync().
    */
    private handleAsyncResponse(hash<auto> resp, *hash<auto> err) {
    }

    #! Internal: handles response
    private handleResponse(hash<auto> resp, *hash<auto> err) {
        AutoLock al(lock);
        # Guarantee broadcast even if state/response assignment throws
        on_exit response_cond.broadcast();
        if (err) {
            error_info = err;
            state = Http3ClientStreamState::Error;
        } else {
            response = resp;
            state = Http3ClientStreamState::Complete;
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt, ...) {
        if (logger) {
            string prefix = exists stream_id
                ? sprintf("Http3ClientStreamHandle[%d]: ", stream_id)
                : "Http3ClientStreamHandle: ";
            call_function_args(\logger.log(), (level, prefix + fmt) + argv);
        }
    }
}

} # namespace Http3ClientIo
