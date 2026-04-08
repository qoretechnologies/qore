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
        @param body_streaming if \c True, enables bidirectional mode; for HTTP/1.1 this
            is used for 101 Switching Protocols (WebSocket upgrade) where the socket
            transitions to raw bidirectional I/O after the handshake

        @return the stream ID

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if connection establishment times out
    */
    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False, *data body) {
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
                data_channel);
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

        if (data_channel.isClosed() && data_channel.empty()) {
            throw "HTTPCLIENT-STREAM-CLOSED",
                sprintf("stream %d has been closed", stream_id);
        }

        try {
            timeout wait_timeout = timeout_ms ?? -1;
            auto rv = data_channel.recv(wait_timeout);
            # The poll operation pushes raw output hashes to the channel.
            # Extract body data and detect end_stream/error for the caller.
            if (rv.typeCode() == NT_HASH) {
                if (rv.error) {
                    {
                        AutoLock al(lock);
                        error_info = rv.error;
                        state = HttpClientStreamState::Error;
                    }
                    return {"error": rv.error};
                }
                # State transitions (monotonically forward)
                {
                    AutoLock al(lock);
                    if (rv.headers && state == HttpClientStreamState::Open) {
                        state = HttpClientStreamState::HeadersReceived;
                        response = rv;
                    }
                    if (rv.body && state != HttpClientStreamState::Complete) {
                        state = HttpClientStreamState::ReceivingBody;
                    }
                    if (rv.end_stream) {
                        state = HttpClientStreamState::Complete;
                    }
                }
                # Return body data extracted from the hash
                if (rv.body) {
                    return rv.body;
                }
                # Return end_stream sentinel
                if (rv.end_stream) {
                    return {"end_stream": True};
                }
                # Headers-only or other metadata — return the hash as-is
                return rv;
            }
            return rv;
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

    #! Not supported for HTTP/1.1 streaming
    /** After 101 Switching Protocols, the socket is extracted from the H1
        connection and used directly with WebSocketClientPollOperationBase.
        This method is not called in that flow.

        @throw HTTP1-STREAMING-ERROR always thrown
    */
    sendData(data data, bool end_stream = False) {
        throw "HTTP1-STREAMING-ERROR",
            "bidirectional body streaming is not supported over HTTP/1.1; "
            "after 101 Upgrade, use the extracted socket with "
            "WebSocketClientPollOperationBase";
    }

    #! Protocol-specific async request submission — returns {stream_id, future}
    hash<auto> submitAsyncImpl(string method, string path, *hash<auto> headers,
            *data body) {
        return conn.submitRequest(method, path, headers, body);
    }

    #! Protocol-specific async submission with EventNotifier signaling
    hash<auto> submitAsyncWithNotifierImpl(string method, string path, *hash<auto> headers,
            *data body, EventNotifier notifier) {
        return conn.submitRequestWithNotifier(method, path, headers, body, notifier);
    }

}

} # namespace HttpClientIo
