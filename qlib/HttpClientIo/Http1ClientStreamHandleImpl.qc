# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http1ClientStreamHandleImpl.qc HTTP/1.1 client stream handle implementation

/** Http1ClientStreamHandleImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/1.1 client stream handle
/** This class extends HttpClientStreamHandle for HTTP/1.1 connections.

    HTTP/1.1 is a serial protocol — each stream handle represents a single
    request/response exchange on the connection. The connection is exclusive
    during streaming (no multiplexing).

    @par Request/Response Usage
    @code{.py}
Http1ClientStreamHandle stream = cast<Http1ClientStreamHandle>(manager.acquireStream("http://example.com"));
hash<auto> response = stream.request("GET", "/api/data");
printf("Status: %d\n", response.status_code);
    @endcode

    @par Streaming Usage (SSE)
    @code{.py}
HttpClientStreamHandle stream = manager.acquireStream("http://example.com");
stream.startStreaming("GET", "/events", {"Accept": "text/event-stream"});
while (True) {
    auto data = stream.readData(30s);
    if (!exists data) {
        continue;
    }
    if (data.typeCode() == NT_HASH && data.end_stream) {
        break;
    }
    printf("Chunk: %y\n", data);
}
    @endcode
*/
public class Http1ClientStreamHandle inherits HttpClientStreamHandle {
    private {
        #! Typed reference to the H1 connection
        Http1ClientConnection h1conn;

        #! Data channel for streaming mode
        *Channel data_channel;

        #! Whether this stream is in streaming mode
        bool streaming = False;
    }

    #! Creates a new stream handle
    constructor(Http1ClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *LoggerInterface logger) : HttpClientStreamHandle(conn, request_timeout, logger) {
        h1conn = conn;
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http1ClientStreamHandle objects cannot be copied";
    }

    #! Returns True — HTTP/1.1 supports streaming via chunked transfer encoding
    bool supportsStreaming() {
        return True;
    }

    #! Starts a streaming request on this HTTP/1.1 connection
    /** The connection is exclusive during streaming — no other requests can be submitted
        until the stream completes.

        @param method HTTP method (e.g., "GET", "POST")
        @param path request path
        @param headers request headers (optional)
        @param body_streaming not supported for HTTP/1.1; must be \c False

        @return the stream ID

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if connection establishment times out
        @throw HTTP1-STREAMING-ERROR if \c body_streaming is \c True (not supported)
    */
    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False, *data body) {
        if (body_streaming) {
            throw "HTTP1-STREAMING-ERROR",
                "bidirectional body streaming is not supported over HTTP/1.1";
        }
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        # Create unbounded channel for data delivery
        data_channel = new Channel(-1);
        streaming = True;

        # Wait for connection to become ready
        if (!conn.isReady()) {
            if (!conn.waitForReady(request_timeout)) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                }
                releaseReservationIfHeld();
                streaming = False;
                remove data_channel;
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Re-check state after readiness wait — cancel() may have run concurrently
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                releaseReservationIfHeld();
                streaming = False;
                remove data_channel;
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "starting streaming request: %s %s", method, path);

        try {
            stream_id = h1conn.submitRequestStreaming(method, path, headers, body,
                \handleStreamingResponse());
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
            streaming = False;
            remove data_channel;
            rethrow;
        }

        return stream_id;
    }

    #! Reads data from the streaming connection
    /** @param timeout_ms timeout for waiting for data

        @return data chunk (binary), end-of-stream sentinel hash, or NOTHING on timeout

        @throw HTTPCLIENT-STREAM-CLOSED if the stream has been closed
    */
    auto readData(*timeout timeout_ms) {
        if (!streaming || !data_channel) {
            throw "HTTPCLIENT-STATE-ERROR", "stream is not in streaming mode";
        }

        # Check for terminal state with empty channel
        {
            AutoLock al(lock);
            if ((state == HttpClientStreamState::Error
                    || state == HttpClientStreamState::Cancelled)
                    && data_channel.empty()) {
                throw "HTTPCLIENT-STREAM-CLOSED",
                    sprintf("stream %d is in terminal state: %y", stream_id, state);
            }
        }

        try {
            timeout wait_timeout = timeout_ms ?? -1;
            return data_channel.recv(wait_timeout);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "CHANNEL-TIMEOUT") {
                return NOTHING;
            }
            if (ex.err == "CHANNEL-CLOSED") {
                throw "HTTPCLIENT-STREAM-CLOSED",
                    sprintf("stream %d has been closed", stream_id);
            }
            rethrow;
        }
    }

    #! Not supported for HTTP/1.1 response-only streaming
    /** @throw HTTP1-STREAMING-ERROR always thrown — HTTP/1.1 does not support bidirectional streaming
    */
    sendData(data data, bool end_stream = False) {
        throw "HTTP1-STREAMING-ERROR",
            "bidirectional body streaming is not supported over HTTP/1.1";
    }

    #! Protocol-specific async request submission
    /** @note The implementation calls handleResponse() to resolve the
        Promise used by the synchronous request() API.
    */
    int submitAsyncImpl(string method, string path, *hash<auto> headers,
            *data body, HttpResponseCallback callback) {
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

    #! Internal: handles incremental streaming response callbacks from the poll operation
    /** Called by the poll operation for each phase of the streaming response:
        - Headers: \c resp.status_code and \c resp.headers set
        - Body data: \c resp.body set (binary chunk)
        - End of stream: \c resp.end_stream set to \c True
    */
    private handleStreamingResponse(hash<auto> resp, *hash<auto> err) {
        if (err) {
            {
                AutoLock al(lock);
                error_info = err;
                state = HttpClientStreamState::Error;
            }
            try {
                data_channel.send({"error": err});
            } catch () {}
            try {
                data_channel.close();
            } catch () {}
            return;
        }

        bool end = resp.end_stream.toBool();

        # State transitions (monotonically forward)
        {
            AutoLock al(lock);
            if (resp.headers && state == HttpClientStreamState::Open) {
                response = resp;
                state = HttpClientStreamState::HeadersReceived;
            }
            if (resp.body && state != HttpClientStreamState::Complete) {
                state = HttpClientStreamState::ReceivingBody;
            }
        }

        # Queue body data
        if (resp.body) {
            try {
                data_channel.send(resp.body);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "CHANNEL-CLOSED") {
                    return;
                }
                rethrow;
            }
        }

        # Handle completion
        if (end) {
            {
                AutoLock al(lock);
                state = HttpClientStreamState::Complete;
            }
            try {
                data_channel.send({"end_stream": True});
            } catch () {}
            try {
                data_channel.close();
            } catch () {}
        }
    }
}

} # namespace HttpClientIo
