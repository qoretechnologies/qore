# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientStreamHandleImpl.qc HTTP/3 client stream handle implementation

/** Http3ClientStreamHandleImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/3 client stream handle with streaming support
/** This class extends HttpClientStreamHandle with HTTP/3-specific streaming
    capabilities for bidirectional streaming over HTTP/3 (e.g., gRPC bidi).

    @par Synchronous Usage
    @code{.py}
Http3ClientStreamHandle stream = cast<Http3ClientStreamHandle>(manager.acquireStream("https://example.com"));
hash<auto> response = stream.request("GET", "/api/users");
printf("Status: %d\n", response.status_code);
    @endcode

    @par Streaming Usage (Bidirectional)
    @code{.py}
Http3ClientStreamHandle stream = cast<Http3ClientStreamHandle>(manager.acquireStream("https://example.com"));

# Start streaming connection (HEADERS without END_STREAM)
stream.startStreaming("POST", "/api/stream", NOTHING, True);

# Send data incrementally
stream.sendData(binary("chunk1"));
stream.sendData(binary("chunk2"));
stream.writesDone();  # close write side

# Read response data
while (True) {
    auto chunk = stream.readData(30s);
    if (!exists chunk) {
        continue;  # timeout, retry
    }
    if (chunk.typeCode() == NT_HASH) {
        if (chunk.end_stream) {
            break;  # stream complete
        }
        if (chunk.error) {
            throw "STREAM-ERROR", sprintf("error: %y", chunk.error);
        }
    }
    printf("Received: %d bytes\n", chunk.size());
}
    @endcode
*/
public class Http3ClientStreamHandle inherits HttpClientStreamHandle {
    private {
        #! Typed reference to the H3 connection (for streaming and datagram methods)
        Http3ClientConnection h3conn;

        #! Data channel for streaming mode (body data and end_stream sentinel)
        *Channel data_channel;

        #! Whether this stream is in streaming mode
        bool streaming = False;
    }

    #! Creates a new stream handle
    constructor(HttpClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *LoggerInterface logger) : HttpClientStreamHandle(conn, request_timeout, logger) {
        h3conn = cast<Http3ClientConnection>(conn);
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientStreamHandle objects cannot be copied";
    }

    #! Protocol-specific async request submission
    int submitAsyncImpl(string method, string path, *hash<auto> headers,
            *data body, HttpResponseCallback callback) {
        return conn.submitRequest(method, path, headers, body,
            sub (hash<auto> resp, *hash<auto> err) {
                try {
                    handleResponse(resp, err);
                } catch (hash<ExceptionInfo> ex) {
                    if (!err) {
                        err = {"err": ex.err, "desc": ex.desc};
                    }
                }
                callback(resp, err);
            });
    }

    #! Protocol-specific async request + datagram submission for HTTP/3
    /** Queues both the HTTP/3 request (with suppress_wake) and a QUIC datagram
        before issuing a single wake, ensuring both are written in the same
        QUIC packet batch.

        @param method HTTP method
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param callback user callback to invoke after handleResponse()
        @param datagram the datagram payload to send

        @return the stream ID assigned to this request

        @see HttpClientStreamHandle::submitAsyncWithDatagram() for the public
        wrapper with state management
    */
    private int submitAsyncWithDatagramImpl(string method, string path, *hash<auto> headers,
            *data body, HttpResponseCallback callback, binary datagram) {
        # Submit the HTTP/3 request without waking the I/O controller
        int sid = conn.submitRequest(method, path, headers, body,
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
            }, NOTHING, True);

        # Queue the datagram (both request and datagram are now pending)
        int qsid = h3conn.getSessionId();
        if (!qsid) {
            # Cancel the request we just submitted
            try { conn.cancelRequest(sid); } catch () {}
            throw "HTTPCLIENT-STATE-ERROR", "QUIC session not yet established";
        }
        try {
            h3conn.getSocket().submitQuicDatagram(qsid, sid, datagram);
        } catch (hash<ExceptionInfo> ex) {
            # Cancel the request we just submitted
            try { conn.cancelRequest(sid); } catch () {}
            rethrow;
        }

        # Single wake for both request and datagram
        conn.notifyPendingData();

        return sid;
    }

    #! Starts a streaming request (for bidirectional streaming over HTTP/3)
    /** @param method HTTP method (typically POST for bidi streaming)
        @param path request path
        @param headers request headers
        @param body_streaming if True, keep the request body stream open for
        bidirectional communication (e.g., gRPC bidi streaming); the caller must
        send data via sendData() and close the stream explicitly.  If False (default),
        the request is submitted with END_STREAM on the HEADERS frame, which is
        appropriate for response-only streaming (e.g., SSE).

        @return the stream ID

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use
        @throw HTTPCLIENT-TIMEOUT if connection establishment timed out
    */
    bool supportsStreaming() {
        return True;
    }

    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False, *data body) {
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
            if (!conn.waitForReady(request_timeout)) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                    streaming = False;
                    delete data_channel;
                }
                releaseReservationIfHeld();
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Re-check state after readiness wait — cancel() may have run concurrently
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                streaming = False;
                delete data_channel;
                releaseReservationIfHeld();
                throw "HTTPCLIENT-CANCELLED",
                    "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "starting streaming request: %s %s", method, path);

        try {
            stream_id = h3conn.submitRequestStreaming(method, path, headers, body,
                \handleStreamingResponse(), NOTHING, body_streaming);
            @assert(stream_id >= 0);
            # Reservation converted to active stream; release connection reservation
            releaseReservationIfHeld();
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                state = HttpClientStreamState::Pending;
                streaming = False;
                delete data_channel;
            }
            releaseReservationIfHeld();
            rethrow;
        }

        return stream_id;
    }

    #! Sends data on a streaming connection
    /** @param data the data to send
        @param end_stream if True, signals end of stream; if the stream is
        already in a terminal state when \a end_stream is \c True, the call
        is a no-op (half-close is idempotent)

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode, stream is
        closed, or stream is not yet established
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
                if (end_stream && data.empty()) {
                    # Half-close on an already-closed stream is a no-op;
                    # the send side is already effectively closed
                    return;
                }
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("cannot send data in state: %y", state);
            }
            if (!exists stream_id) {
                throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
            }
            sid = stream_id;
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d (end_stream=%y)", sid, end_stream);
        h3conn.sendStreamData(sid, data, end_stream);
    }

    #! Closes the write side of a streaming connection
    /** Sends an empty DATA frame with END_STREAM to signal that no more data
        will be sent. Equivalent to \c sendData(binary(), True).

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode
    */
    writesDone() {
        sendData(binary(), True);
    }

    #! Reads data from a streaming connection
    /** Reads the next chunk of data from this streaming HTTP/3 request.

        The return value follows the same convention as the HTTP/2 streaming
        API for unified handling:
        - \c binary or \c string: body data chunk
        - \c hash with \c end_stream key: stream is complete
        - \c hash with \c error key: stream error
        - \c NOTHING: timeout (no data within \a timeout_ms)

        Blocks deterministically until data arrives from the server via the
        internal channel (same pattern as HTTP/2 streaming).

        @param timeout_ms timeout for waiting for data

        @return data chunk, end_stream sentinel hash, error hash, or NOTHING on timeout

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode
        @throw HTTPCLIENT-STREAM-CLOSED if the stream is in error/cancelled state
        and no more data is available
    */
    auto readData(*timeout timeout_ms) {
        timeout wait_timeout = timeout_ms ?? request_timeout;

        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTPCLIENT-STATE-ERROR", "not in streaming mode";
            }
            if ((state == HttpClientStreamState::Error
                    || state == HttpClientStreamState::Cancelled)
                    && data_channel.empty()) {
                throw "HTTPCLIENT-STREAM-CLOSED", "stream is closed";
            }
            # NOTE: do NOT check Complete state here — handleStreamingResponse()
            # sets state to Complete BEFORE sending body data to the channel.
            # Completion is detected via CHANNEL-CLOSED from the recv() below.
        }

        # Read from the channel — same pattern as H2.
        # The callback puts body data and {"end_stream": True} into the channel.
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

    #! Returns trailers from the completed response
    *hash<string, string> getTrailers() {
        AutoLock al(lock);
        return response.trailers;
    }

    #! Internal: handles streaming response from the poll operation
    /** This callback is invoked by the poll operation when a response is
        available for a streaming request.  Body data and the end_stream
        sentinel are forwarded through the data channel so that readData()
        can consume them (same pattern as HTTP/2 streaming).

        @param resp the response hash from getOutput() (includes headers, status_code, body, end_stream)
        @param err error info if an error occurred
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
            } catch (hash<ExceptionInfo> ex2) {
                # Ignore CHANNEL-CLOSED and any other send errors in error path;
                # propagating here would crash the poll operation callback thread
                log(LoggerLevel::DEBUG, "error channel send failed: %s: %s", ex2.err, ex2.desc);
            }
            data_channel.close();
            return;
        }

        bool is_end = resp.end_stream.toBool();

        # Update non-terminal states immediately (reader doesn't react to these)
        {
            AutoLock al(lock);
            # Guard state transitions to be monotonically forward only
            if (resp.headers && state == HttpClientStreamState::Open) {
                state = HttpClientStreamState::HeadersReceived;
                response = resp;
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
            # Set Complete state BEFORE enqueuing end_stream so that isDone()
            # returns True as soon as the reader consumes end_stream from the
            # channel (avoiding a race where the reader sees end_stream before
            # the state transition completes)
            {
                AutoLock al(lock);
                if (state < HttpClientStreamState::Complete) {
                    state = HttpClientStreamState::Complete;
                    response = resp;
                }
            }
            try {
                data_channel.send({"end_stream": True});
            } catch (hash<ExceptionInfo> ex2) {
                if (ex2.err != "CHANNEL-CLOSED") {
                    rethrow;
                }
            }
            data_channel.close();
        }
    }

    #! Send a QUIC DATAGRAM frame on this stream's datagram channel (RFC 9221/9297)
    sendDatagram(binary data) {
        if (!exists stream_id) {
            throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
        }
        int sid = h3conn.getSessionId();
        if (!sid) {
            throw "HTTPCLIENT-STATE-ERROR", "QUIC session not yet established";
        }
        h3conn.getSocket().submitQuicDatagram(sid, stream_id, data);
        # Wake the I/O controller so the datagram is written promptly
        conn.notifyPendingData();
    }

    #! Read the next incoming QUIC datagram on this stream's channel (RFC 9221/9297)
    *binary readDatagram(timeout timeout_ms = 30s) {
        if (!exists stream_id) {
            throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
        }
        int sid = h3conn.getSessionId();
        if (!sid) {
            throw "HTTPCLIENT-STATE-ERROR", "QUIC session not yet established";
        }
        return h3conn.getSocket().readQuicDatagram(sid, stream_id, timeout_ms);
    }

    #! Check if the remote peer supports QUIC datagrams on this connection
    bool isDatagramSupported() {
        int sid = h3conn.getSessionId();
        if (!sid) {
            return False;
        }
        return h3conn.getSocket().isQuicDatagramSupported(sid);
    }

    #! Get the maximum datagram payload size for this connection
    int getMaxDatagramSize() {
        if (!exists stream_id) {
            return 0;
        }
        int sid = h3conn.getSessionId();
        if (!sid) {
            return 0;
        }
        return h3conn.getSocket().getQuicMaxDatagramSize(sid, stream_id);
    }
}

} # namespace HttpClientIo
