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

        #! Whether this stream uses chunked request-body streaming (sendData allowed)
        bool request_body_streaming = False;
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
        @param body_streaming if \c True, the request body is sent using
            \c "Transfer-Encoding: chunked" and chunks are pushed
            incrementally via @ref sendData() / @ref writesDone().
            Use this for protocols like DataStream that send a chunked
            request body.  When \c True, @a body should be empty.

            HTTP/1.1 chunked transfer is sequential on the wire — the
            request body must be fully sent before the response body
            starts arriving.  At the application layer, callers may
            interleave @ref sendData() and @ref readData() freely; the
            poll op buffers as needed.  For true wire-level concurrent
            bidirectional streaming, use HTTP/2 or HTTP/3.
        @param body request body (only used when \c body_streaming = \c False)
        @param notifier optional EventNotifier signaled when raw response
            items are available to @ref readData()

        @return the stream ID

        @throw HTTPCLIENT-STATE-ERROR if the stream is already in use, or
            if both \c body_streaming = \c True and a non-empty @a body
            were supplied
        @throw HTTPCLIENT-TIMEOUT if connection establishment times out

        @since %Qore 2.3 (the \c body_streaming = \c True path was added in
            this release; previously the method threw \c HTTP1-STREAMING-ERROR
            on \c sendData())
    */
    int startStreaming(string method, string path, *hash<auto> headers,
            bool body_streaming = False, *data body, *EventNotifier notifier,
            bool sse_install_eligible = False) {
        if (body_streaming && body && body.size() > 0) {
            throw "HTTPCLIENT-STATE-ERROR",
                "body_streaming=True is incompatible with a non-empty body; "
                "send body bytes via sendData() instead";
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
        request_body_streaming = body_streaming;

        # Wait for connection to become ready
        if (!conn.isReady()) {
            if (notifier) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                }
                releaseReservationIfHeld();
                streaming = False;
                request_body_streaming = False;
                remove data_channel;
                throw "HTTPCLIENT-NOT-READY",
                    "connection is not ready for async streaming request";
            }
            if (!conn.waitForReady(timeout_ms: request_timeout)) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                }
                releaseReservationIfHeld();
                streaming = False;
                request_body_streaming = False;
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
                request_body_streaming = False;
                remove data_channel;
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "starting streaming request: %s %s (body_streaming=%y)",
            method, path, body_streaming);

        try {
            stream_id = h1conn.submitRequestStreaming(method, path, headers, body,
                data_channel, True, body_streaming, notifier, sse_install_eligible);
            @assert(stream_id >= 0);
            releaseReservationIfHeld();
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            streaming = False;
            request_body_streaming = False;
            remove data_channel;
            rethrow;
        }

        return stream_id;
    }

    #! Starts an SSE streaming request with I/O-thread event parsing
    /** @see HttpClientStreamHandle::startSseStreaming() for full documentation
    */
    Queue startSseStreaming(string method, string path, *hash<auto> headers, *data body,
            *EventNotifier notifier) {
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Pending) {
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("stream already in use: state=%y", state);
            }
            state = HttpClientStreamState::Open;
        }

        Queue queue();
        streaming = True;

        # Wait for connection to become ready
        if (!conn.isReady()) {
            if (!conn.waitForReady(timeout_ms: request_timeout)) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                }
                releaseReservationIfHeld();
                streaming = False;
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        # Re-check state after readiness wait — cancel() may have run concurrently
        {
            AutoLock al(lock);
            if (state != HttpClientStreamState::Open) {
                releaseReservationIfHeld();
                streaming = False;
                throw "HTTPCLIENT-CANCELLED", "stream was cancelled during connection setup";
            }
        }

        log(LoggerLevel::DEBUG, "starting SSE streaming request: %s %s", method, path);

        try {
            stream_id = h1conn.submitRequestSse(method, path, headers, body, queue, notifier);
            @assert(stream_id >= 0);
            releaseReservationIfHeld();
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                if (state == HttpClientStreamState::Open) {
                    state = HttpClientStreamState::Pending;
                }
            }
            releaseReservationIfHeld();
            streaming = False;
            rethrow;
        }

        return queue;
    }

    #! Installs an SSE parser on an existing streaming request
    /** @see HttpClientStreamHandle::installSseState() for full documentation
    */
    installSseState(Queue queue, *EventNotifier notifier, *data initial_body) {
        int sid;
        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTPCLIENT-STATE-ERROR", "not in streaming mode";
            }
            if (!exists stream_id) {
                throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
            }
            sid = stream_id;
        }

        h1conn.installSseState(sid, queue, notifier, initial_body);
    }

    #! Reads data from the streaming connection
    /** @param timeout_ms timeout for waiting for data

        @return data chunk (binary), end-of-stream sentinel hash, or NOTHING on timeout

        @throw HTTPCLIENT-STREAM-CLOSED if the stream has been closed
    */
    auto readData(*timeout timeout_ms) {
        timeout wait_timeout = timeout_ms ?? -1;
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

        # @ref Qore::Thread::Channel::recv() treats timeout_ms <= 0 as infinite
        # wait.  A caller that passes 0 explicitly wants a non-blocking peek
        # (matching the other HTTP reader APIs where 0 means "return NOTHING
        # if no data is pending"); route through tryRecv() in that case so
        # drain loops (e.g. DataStreamClientIo.sendDSv1Stream) don't hang.
        try {
            auto rv;
            if (wait_timeout == 0ms) {
                *hash<auto> tr = data_channel.tryRecv();
                if (!exists tr) {
                    return NOTHING;
                }
                if (!tr.has_value) {
                    throw "HTTPCLIENT-STREAM-CLOSED",
                        sprintf("stream %d has been closed", stream_id);
                }
                rv = tr.value;
            } else {
                rv = data_channel.recv(wait_timeout);
            }
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

    #! Sends a request-body chunk on a streaming HTTP/1.1 request
    /** Requires the stream to have been started with
        @ref startStreaming() and \c body_streaming = \c True (which sends
        the request headers with \c "Transfer-Encoding: chunked" and
        leaves the request body open).  Each call appends one chunk;
        passing \c end_stream = \c True writes the final \c "0\\r\\n"
        terminator (and any trailers set via @ref setTrailers via the
        connection) and transitions the connection to response reading.

        Half-close (\c end_stream = \c True with empty data) on an
        already-closed stream is a no-op.

        @param data the chunk to send (string or binary)
        @param end_stream if \c True, signal end-of-request-body

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode, the
            stream is not in a valid send state, the stream wasn't
            started with \c body_streaming = \c True, or the stream has
            no assigned ID yet
        @throw HTTPCLIENT-STREAM-CLOSED if the underlying H1 connection
            has been closed

        @since %Qore 2.3
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
                    # half-close on already-closed stream is a no-op
                    return;
                }
                throw "HTTPCLIENT-STATE-ERROR",
                    sprintf("cannot send data in state: %y", state);
            }
            if (!stream_id) {
                throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
            }
            if (!request_body_streaming) {
                if (end_stream && data.empty()) {
                    return;
                }
                throw "HTTPCLIENT-STATE-ERROR",
                    "cannot send data: stream was not started with "
                    "body_streaming=True";
            }
            sid = stream_id;
        }

        # Check connection state AFTER stream-level checks — the H1
        # connection may have been closed (TCP reset, FIN) while the
        # stream handle's state is still Open.  Catching it here gives
        # a clean HTTPCLIENT-STREAM-CLOSED instead of an opaque
        # HTTP1-STATE-ERROR from sendStreamData().
        if (!h1conn.isReady()) {
            if (end_stream && data.empty()) {
                return;
            }
            throw "HTTPCLIENT-STREAM-CLOSED",
                sprintf("cannot send data: H1 connection closed (state: %y)",
                    h1conn.getConnectionState());
        }

        log(LoggerLevel::DEBUG, "sending data on H1 stream %d (end_stream=%y)",
            sid, end_stream);
        h1conn.sendStreamData(sid, data, end_stream);
    }

    #! Sets HTTP/1.1 chunked-transfer trailers for a streaming-send request
    /** Trailers are emitted after the final \c "0\\r\\n" terminator when
        @ref sendData() is called with \c end_stream = \c True.  Each call
        replaces any previously-set trailers.

        @param trailers map of trailer header name to value; pass \c NOTHING
            to clear

        @throw HTTPCLIENT-STATE-ERROR if not in streaming mode

        @since %Qore 2.3
    */
    setTrailers(*hash<string, string> trailers) {
        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTPCLIENT-STATE-ERROR", "not in streaming mode";
            }
            if (!request_body_streaming) {
                throw "HTTPCLIENT-STATE-ERROR",
                    "trailers require body_streaming=True";
            }
        }
        h1conn.setTrailers(trailers);
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
