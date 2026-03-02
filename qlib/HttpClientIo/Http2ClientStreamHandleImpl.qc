# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientStreamHandleImpl.qc HTTP/2 client stream handle implementation

/** Http2ClientStreamHandleImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/2 client stream handle with streaming support
/** This class extends HttpClientStreamHandle with HTTP/2-specific streaming
    capabilities for WebSocket and SSE over HTTP/2.

    @par Streaming Usage (WebSocket/SSE)
    @code{.py}
Http2ClientStreamHandle stream = cast<Http2ClientStreamHandle>(manager.acquireStream("https://example.com"));

# Start streaming connection
stream.startStreaming("GET", "/events", {"accept": "text/event-stream"});

# Read streaming data
while (True) {
    auto data = stream.readData(30s);
    if (!exists data) {
        continue;
    }
    if (data.end_stream) {
        break;
    }
    printf("Event: %y\n", data);
}
    @endcode
*/
public class Http2ClientStreamHandle inherits HttpClientStreamHandle {
    private {
        #! Data channel for streaming mode
        *Channel data_channel;

        #! Whether this stream is in streaming mode
        bool streaming = False;

        #! Typed reference to the H2 connection (for streaming methods)
        Http2ClientConnection h2conn;
    }

    #! Creates a new stream handle
    constructor(Http2ClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *Logger logger) : HttpClientStreamHandle(conn, request_timeout, logger) {
        h2conn = conn;
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http2ClientStreamHandle objects cannot be copied";
    }

    #! Protocol-specific async request submission
    /** @note The implementation MUST call handleResponse() to resolve the
        Promise used by the synchronous request() API.
    */
    int submitAsyncImpl(string method, string path, *hash<auto> headers,
            *data body, code callback) {
        return conn.submitRequest(method, path, headers, body,
            sub (hash<auto> resp, *hash<auto> err) {
                try {
                    handleResponse(resp, err);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "handleResponse error: %s: %s", ex.err, ex.desc);
                    if (!err) {
                        err = {"err": ex.err, "desc": ex.desc};
                    }
                }
                callback(resp, err);
            });
    }

    #! Starts a streaming request (for WebSocket or SSE over HTTP/2)
    /** @param method HTTP method (typically GET for SSE, or CONNECT for WebSocket)
        @param path request path
        @param headers request headers
        @param body_streaming if True, keep the request body stream open for
        bidirectional communication (e.g., WebSocket over HTTP/2); the caller must
        send data via sendData() and close the stream explicitly.  If False (default),
        the request is submitted with END_STREAM on the HEADERS frame, which is
        appropriate for response-only streaming (e.g., SSE).

        @return the stream ID

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
    */
    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
            streaming = True;
            data_channel = new Channel(-1);
        }

        # Wait for connection to become ready if not already
        if (!conn.isReady()) {
            bool ready = conn.isControllerMode()
                ? conn.waitForReady(request_timeout)
                : conn.driveToReady(request_timeout);
            if (!ready) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                    streaming = False;
                    delete data_channel;
                }
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        log(LoggerLevel::DEBUG, "starting streaming request: %s %s", method, path);

        try {
            stream_id = h2conn.submitRequestStreaming(method, path, headers, NOTHING,
                \handleStreamingResponse(), NOTHING, body_streaming);
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                state = HttpClientStreamState::Pending;
                streaming = False;
                delete data_channel;
            }
            rethrow;
        }

        return stream_id;
    }

    #! Reads data from a streaming connection
    /** @param timeout_ms timeout for waiting for data

        @return the data received, NOTHING on timeout, or a hash with key "end_stream"
        set to True when the stream is complete

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode
        @throw HTTPCLIENT-STREAM-CLOSED if the stream is closed and no data is queued
    */
    auto readData(*timeout timeout_ms) {
        timeout wait_timeout = timeout_ms ?? request_timeout;
        bool controller_mode = conn.isControllerMode();

        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTPCLIENT-STATE-ERROR", "not in streaming mode";
            }
            if (state == HttpClientStreamState::Complete
                    || state == HttpClientStreamState::Error
                    || state == HttpClientStreamState::Cancelled) {
                if (data_channel.empty()) {
                    throw "HTTPCLIENT-STREAM-CLOSED", "stream is closed";
                }
            }
        }

        if (controller_mode) {
            try {
                return data_channel.recv(wait_timeout);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "CHANNEL-TIMEOUT") {
                    return NOTHING;
                }
                if (ex.err == "CHANNEL-CLOSED") {
                    throw "HTTPCLIENT-STREAM-CLOSED", "stream is closed";
                }
                rethrow;
            }
        }

        # Non-controller mode: drive the poll while waiting for data
        date end_time = now_us() + wait_timeout;
        while (True) {
            auto result = data_channel.tryRecv();
            if (exists result) {
                return result.value;
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
        @param end_stream if True, signals end of stream

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode or stream is closed
    */
    sendData(data data, bool end_stream = False) {
        int sid;
        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTPCLIENT-STATE-ERROR", "not in streaming mode";
            }
            if (state != HttpClientStreamState::Open
                    && state != HttpClientStreamState::HeadersReceived
                    && state != HttpClientStreamState::ReceivingBody) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("cannot send data in state: %y", state);
            }
            if (!stream_id) {
                throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
            }
            sid = stream_id;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d (end_stream=%y)", sid, end_stream);
        h2conn.sendStreamData(sid, data, end_stream);

        if (!conn.isControllerMode()) {
            conn.drivePoll(1s);
        }
    }

    #! Returns trailers from the completed response
    *hash<string, string> getTrailers() {
        AutoLock al(lock);
        return response.trailers;
    }

    #! Internal: handles streaming response
    private handleStreamingResponse(hash<auto> resp, *hash<auto> err) {
        if (err) {
            {
                AutoLock al(lock);
                error_info = err;
                state = HttpClientStreamState::Error;
            }
            try {
                data_channel.send({"error": err});
            } catch (hash<ExceptionInfo> ex2) {
                # Ignore CHANNEL-CLOSED and any other send errors in error path;
                # propagating here would crash the poll operation callback thread
                log(LoggerLevel::DEBUG, "error channel send failed: %s: %s", ex2.err, ex2.desc);
            }
            data_channel.close();
            return;
        }

        bool is_end = resp.end_stream.toBool() || exists resp.trailers;

        # Update non-terminal states immediately (reader doesn't react to these)
        {
            AutoLock al(lock);
            # Guard state transitions to be monotonically forward only
            if (resp.headers && state == HttpClientStreamState::Open) {
                state = HttpClientStreamState::HeadersReceived;
            }
            if (resp.body && state < HttpClientStreamState::Complete) {
                state = HttpClientStreamState::ReceivingBody;
            }
        }

        # Send data to channel BEFORE setting Complete state, so readers
        # cannot see Complete + empty channel and throw HTTPCLIENT-STREAM-CLOSED
        if (resp.body) {
            try {
                data_channel.send(resp.body);
            } catch (hash<ExceptionInfo> ex2) {
                if (ex2.err != "CHANNEL-CLOSED") {
                    rethrow;
                }
            }
        }

        if (is_end) {
            try {
                data_channel.send({"end_stream": True});
            } catch (hash<ExceptionInfo> ex2) {
                if (ex2.err != "CHANNEL-CLOSED") {
                    rethrow;
                }
            }
            # Set Complete state AFTER data is enqueued
            {
                AutoLock al(lock);
                if (state < HttpClientStreamState::Complete) {
                    state = HttpClientStreamState::Complete;
                    response = resp;
                }
            }
            data_channel.close();
        }
    }
}

} # namespace HttpClientIo
