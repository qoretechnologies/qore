# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientStreamHandle.qc Per-request handle for HTTP/2 client streams

/** Http2ClientStreamHandle.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Per-request handle for HTTP/2 client streams
/** This class provides a convenient handle for a single HTTP/2 stream,
    supporting both synchronous and asynchronous request patterns.

    @par Synchronous Usage
    @code{.py}
Http2ClientStreamHandle stream = manager.acquireStream("https://example.com");

# Submit request and block until response
hash<auto> response = stream.request("GET", "/api/users");
printf("Status: %d\n", response.status_code);
printf("Body: %y\n", response.body);
    @endcode

    @par Asynchronous Usage
    @code{.py}
Http2ClientStreamHandle stream = manager.acquireStream("https://example.com");

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

    @par Streaming Usage (WebSocket/SSE)
    @code{.py}
Http2ClientStreamHandle stream = manager.acquireStream("https://example.com");

# Start streaming connection
stream.startStreaming("GET", "/events", {"accept": "text/event-stream"});

# Read streaming data
while (True) {
    auto data = stream.readData(30s);
    if (!exists data) {
        # Timeout - no data received within timeout period
        continue;
    }
    if (data.end_stream) {
        # End of stream sentinel received
        break;
    }
    printf("Event: %y\n", data);
}
    @endcode

*/
public class Http2ClientStreamHandle {
    private {
        #! The connection this stream is on
        Http2ClientConnection conn;

        #! Stream ID (set after request submitted)
        *int stream_id;

        #! Stream state
        Http2ClientStreamState state = Http2ClientStreamState::Pending;

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

        #! Data queue for streaming mode
        *Queue data_queue;

        #! Whether this stream is in streaming mode
        bool streaming = False;
    }

    #! Creates a new stream handle
    /** @param conn the connection to use for this stream
        @param request_timeout timeout for requests
        @param logger optional logger
    */
    constructor(Http2ClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *Logger logger) {
        self.conn = conn;
        self.request_timeout = request_timeout;
        self.logger = logger;
    }

    #! Returns the stream ID (NOTHING if not yet submitted)
    *int getStreamId() {
        return stream_id;
    }

    #! Returns the stream state
    Http2ClientStreamState getState() {
        AutoLock al(lock);
        return state;
    }

    #! Returns True if the stream is complete or closed
    bool isDone() {
        AutoLock al(lock);
        return state == Http2ClientStreamState::Complete
            || state == Http2ClientStreamState::Cancelled
            || state == Http2ClientStreamState::Error;
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
    Http2ClientConnection getConnection() {
        return conn;
    }

    #! Performs a synchronous request and returns the response
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param timeout_ms request timeout (uses default if not specified)

        @return the response hash with status_code, headers, body

        @throw HTTP2-REQUEST-ERROR if the request fails
        @throw HTTP2-TIMEOUT if the request times out
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
                throw "HTTP2-TIMEOUT", "connection establishment timed out";
            }
            if (controller_mode) {
                # In controller mode, wait for the controller to drive the connection
                if (!conn.waitForReady(remaining)) {
                    throw "HTTP2-TIMEOUT", "connection establishment timed out";
                }
            } else {
                # In sync mode, drive the connection directly
                if (!conn.driveToReady(remaining)) {
                    throw "HTTP2-TIMEOUT", "connection establishment timed out";
                }
            }
        }

        # Submit async and wait
        submitAsync(method, path, headers, body, \handleAsyncResponse());

        # Wait for response
        {
            AutoLock al(lock);
            while (state != Http2ClientStreamState::Complete
                    && state != Http2ClientStreamState::Error
                    && state != Http2ClientStreamState::Cancelled) {
                date remaining = end_time - now_us();
                if (remaining <= 0us) {
                    # Release lock before calling cancel() to avoid deadlock
                    delete al;
                    cancel();
                    throw "HTTP2-TIMEOUT",
                        sprintf("request timed out after %d ms", int(wait_timeout));
                }

                if (controller_mode) {
                    # In controller mode, wait on condition - controller drives I/O
                    # The callback will broadcast response_cond when response arrives,
                    # or when an error/abort occurs via notifyPendingStreams()
                    response_cond.wait(lock, remaining);
                } else {
                    # In sync mode, release lock and drive poll directly
                    # Cap poll timeout to 1s to avoid blocking the single driver
                    # thread for extended periods, which would starve other threads
                    # waiting for their responses to be dispatched
                    delete al;
                    conn.drivePoll(min(remaining, 1s));
                    al = new AutoLock(lock);
                }
            }

            if (error_info) {
                throw "HTTP2-REQUEST-ERROR",
                    sprintf("%s: %s", error_info.err, error_info.desc);
            }

            return response ?? {};
        }
    }

    #! Submits an asynchronous request with a callback
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback callback invoked when response is ready:
               sub (hash\<auto> response, *hash\<auto> error)

        @return the stream ID assigned to this request

        @throw HTTP2-STATE-ERROR if the stream is already in use
    */
    int submitAsync(string method, string path, *hash<auto> headers, *data body,
            code callback) {
        {
            AutoLock al(lock);
            if (state != Http2ClientStreamState::Pending) {
                throw "HTTP2-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = Http2ClientStreamState::Open;
        }

        # Wait for connection to become ready if not already
        if (!conn.isReady()) {
            if (conn.isControllerMode()) {
                # In controller mode, wait for the controller to drive the connection
                if (!conn.waitForReady(request_timeout)) {
                    {
                        AutoLock al(lock);
                        state = Http2ClientStreamState::Pending;
                    }
                    throw "HTTP2-TIMEOUT", "connection establishment timed out";
                }
            } else {
                # In sync mode, drive the connection directly
                if (!conn.driveToReady(request_timeout)) {
                    {
                        AutoLock al(lock);
                        state = Http2ClientStreamState::Pending;
                    }
                    throw "HTTP2-TIMEOUT", "connection establishment timed out";
                }
            }
        }

        log(LoggerLevel::DEBUG, "submitting async request: %s %s", method, path);

        try {
            stream_id = conn.submitRequest(method, path, headers, body,
                sub (hash<auto> resp, *hash<auto> err) {
                    handleResponse(resp, err);
                    callback(resp, err);
                });
        } catch (hash<ExceptionInfo> ex) {
            # Rollback state on error
            {
                AutoLock al(lock);
                state = Http2ClientStreamState::Pending;
            }
            rethrow;
        }

        return stream_id;
    }

    #! Starts a streaming request (for WebSocket or SSE over HTTP/2)
    /** @param method HTTP method (typically GET for SSE, or CONNECT for WebSocket)
        @param path request path
        @param headers request headers

        @return the stream ID

        @throw HTTP2-STATE-ERROR if the stream is already in use
    */
    int startStreaming(string method, string path, *hash<auto> headers) {
        {
            AutoLock al(lock);
            if (state != Http2ClientStreamState::Pending) {
                throw "HTTP2-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = Http2ClientStreamState::Open;
            streaming = True;
            data_queue = new Queue();
        }

        # Wait for connection to become ready if not already
        if (!conn.isReady()) {
            bool ready = conn.isControllerMode()
                ? conn.waitForReady(request_timeout)
                : conn.driveToReady(request_timeout);
            if (!ready) {
                {
                    AutoLock al(lock);
                    state = Http2ClientStreamState::Pending;
                    streaming = False;
                    delete data_queue;
                }
                throw "HTTP2-TIMEOUT", "connection establishment timed out";
            }
        }

        log(LoggerLevel::DEBUG, "starting streaming request: %s %s", method, path);

        try {
            stream_id = conn.submitRequest(method, path, headers, NOTHING,
                \handleStreamingResponse(), NOTHING, True);
        } catch (hash<ExceptionInfo> ex) {
            # Rollback state on error
            {
                AutoLock al(lock);
                state = Http2ClientStreamState::Pending;
                streaming = False;
                delete data_queue;
            }
            rethrow;
        }

        return stream_id;
    }

    #! Reads data from a streaming connection
    /** @param timeout_ms timeout for waiting for data

        @return the data received, NOTHING on timeout, or a hash with key "end_stream"
        set to True when the stream is complete (to distinguish from timeout)

        @throw HTTP2-STATE-ERROR if not in streaming mode
        @throw HTTP2-STREAM-CLOSED if the stream is closed and no data is queued
    */
    auto readData(*timeout timeout_ms) {
        timeout wait_timeout = timeout_ms ?? request_timeout;
        bool controller_mode = conn.isControllerMode();

        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTP2-STATE-ERROR", "not in streaming mode";
            }
            if (state == Http2ClientStreamState::Complete
                    || state == Http2ClientStreamState::Error
                    || state == Http2ClientStreamState::Cancelled) {
                if (data_queue.empty()) {
                    throw "HTTP2-STREAM-CLOSED", "stream is closed";
                }
            }
        }

        if (controller_mode) {
            # Controller drives I/O; just wait on the queue
            try {
                return data_queue.get(wait_timeout);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "QUEUE-TIMEOUT") {
                    return NOTHING;
                }
                rethrow;
            }
        }

        # Non-controller mode: drive the poll while waiting for data
        date end_time = now_us() + wait_timeout;
        while (True) {
            # Check queue with short timeout
            try {
                return data_queue.get(0);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "QUEUE-TIMEOUT") {
                    rethrow;
                }
                # Queue empty — drive poll
            }

            date remaining = end_time - now_us();
            if (remaining <= 0us) {
                return NOTHING;
            }

            conn.drivePoll(min(remaining, 1s));
        }
    }

    #! Sends data on a streaming connection
    /** @param data the data to send
        @param end_stream if True, signals end of stream (no more data will be sent)

        @throw HTTP2-STATE-ERROR if not in streaming mode or stream is closed
    */
    sendData(data data, bool end_stream = False) {
        int sid;
        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTP2-STATE-ERROR", "not in streaming mode";
            }
            if (state != Http2ClientStreamState::Open
                    && state != Http2ClientStreamState::HeadersReceived
                    && state != Http2ClientStreamState::ReceivingBody) {
                throw "HTTP2-STATE-ERROR",
                    sprintf("cannot send data in state: %y", state);
            }
            if (!stream_id) {
                throw "HTTP2-STATE-ERROR", "stream not yet established";
            }
            sid = stream_id;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d (end_stream=%y)", sid, end_stream);
        conn.sendStreamData(sid, data, end_stream);

        # In non-controller mode, drive the poll to flush send data
        if (!conn.isControllerMode()) {
            conn.drivePoll(1s);
        }
    }

    #! Returns trailers from the completed response
    /** Returns HTTP/2 trailer headers from the response, if any were received.
        Trailers are available after the stream is complete.

        This is used by protocols like gRPC that communicate status via HTTP/2 trailers
        (e.g., \c "grpc-status", \c "grpc-message").

        @return trailer headers as a hash, or @ref nothing if no trailers were received
        or the stream is not yet complete
    */
    *hash<string, string> getTrailers() {
        AutoLock al(lock);
        return response.trailers;
    }

    #! Cancels the request
    cancel() {
        {
            AutoLock al(lock);
            if (state == Http2ClientStreamState::Complete
                    || state == Http2ClientStreamState::Cancelled
                    || state == Http2ClientStreamState::Error) {
                return;
            }
            state = Http2ClientStreamState::Cancelled;
        }

        if (stream_id) {
            log(LoggerLevel::DEBUG, "cancelling stream %d", stream_id);
            conn.cancelRequest(stream_id);
        }

        {
            AutoLock al(lock);
            response_cond.broadcast();
        }
    }

    #! Internal: handles async response for sync requests
    private handleAsyncResponse(hash<auto> resp, *hash<auto> err) {
        handleResponse(resp, err);
    }

    #! Internal: handles response
    private handleResponse(hash<auto> resp, *hash<auto> err) {
        AutoLock al(lock);
        if (err) {
            error_info = err;
            state = Http2ClientStreamState::Error;
        } else {
            response = resp;
            state = Http2ClientStreamState::Complete;
        }
        response_cond.broadcast();
    }

    #! Internal: handles streaming response
    private handleStreamingResponse(hash<auto> resp, *hash<auto> err) {
        if (err) {
            {
                AutoLock al(lock);
                error_info = err;
                state = Http2ClientStreamState::Error;
            }
            # Push error to queue to unblock readers
            data_queue.push({"error": err});
            return;
        }

        # The callback is invoked once with the complete response when the
        # stream finishes (headers + body + trailers). Detect stream completion
        # from the presence of trailers or the end_stream flag.
        bool is_end = resp.end_stream.toBool() || exists resp.trailers;

        {
            AutoLock al(lock);
            if (resp.headers && state == Http2ClientStreamState::Open) {
                state = Http2ClientStreamState::HeadersReceived;
            }
            if (resp.body) {
                state = Http2ClientStreamState::ReceivingBody;
            }
            if (is_end) {
                state = Http2ClientStreamState::Complete;
                # Store response for getTrailers() access
                response = resp;
            }
        }

        if (resp.body) {
            data_queue.push(resp.body);
        }

        if (is_end) {
            # Use explicit sentinel to distinguish from timeout (which returns NOTHING)
            data_queue.push({"end_stream": True});
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            string prefix = stream_id
                ? sprintf("Http2ClientStreamHandle[%d]: ", stream_id)
                : "Http2ClientStreamHandle: ";
            call_function_args(\logger.log(), (level, prefix + fmt) + argv);
        }
    }
}

} # namespace Http2ClientIo
