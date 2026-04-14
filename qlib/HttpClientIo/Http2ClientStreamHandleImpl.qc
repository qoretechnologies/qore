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

        #! Deferred end_stream sentinel: set when a channel event carries
        #! body + end_stream together, cleared on next readData() call
        bool pending_end_stream = False;

        #! True when the request write side remains open after startStreaming()
        bool request_body_streaming = False;
    }

    #! Creates a new stream handle
    constructor(Http2ClientConnection conn, timeout request_timeout = DefaultRequestTimeout,
            *LoggerInterface logger) : HttpClientStreamHandle(conn, request_timeout, logger) {
        h2conn = conn;
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http2ClientStreamHandle objects cannot be copied";
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

    #! Returns True; HTTP/2 supports streaming (WebSocket, SSE)
    bool supportsStreaming() {
        return True;
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
        @param body one-shot request body (optional); sent with the request before
        streaming begins

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
            streaming = True;
            request_body_streaming = body_streaming;
            data_channel = new Channel(-1);
        }

        # Wait for connection to become ready if not already
        if (!conn.isReady()) {
            if (!conn.waitForReady(request_timeout)) {
                {
                    AutoLock al(lock);
                    state = HttpClientStreamState::Pending;
                    streaming = False;
                    request_body_streaming = False;
                    delete data_channel;
                }
                throw "HTTPCLIENT-TIMEOUT", "connection establishment timed out";
            }
        }

        log(LoggerLevel::DEBUG, "starting streaming request: %s %s", method, path);

        try {
            stream_id = h2conn.submitRequestStreaming(method, path, headers, body,
                data_channel, body_streaming);
            @assert(stream_id >= 0);
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(lock);
                state = HttpClientStreamState::Pending;
                streaming = False;
                request_body_streaming = False;
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

        {
            AutoLock al(lock);
            if (!streaming) {
                throw "HTTPCLIENT-STATE-ERROR", "not in streaming mode";
            }
            # For Error/Cancelled, state is set before any channel write, so an
            # empty channel means no data will ever arrive
            if ((state == HttpClientStreamState::Error
                    || state == HttpClientStreamState::Cancelled)
                    && data_channel.empty()) {
                throw "HTTPCLIENT-STREAM-CLOSED", "stream is closed";
            }
            # Deliver a deferred end_stream sentinel when the previous call
            # consumed a body+end_stream coalesced event: the body was
            # returned then and end_stream must surface now so streaming
            # readers see a clean completion signal.
            if (pending_end_stream) {
                pending_end_stream = False;
                return {"end_stream": True};
            }
        }
        # For Complete, state is set before end_stream is enqueued; use isClosed()
        # to detect when all data (including end_stream) has been delivered to the
        # channel — avoids a race where isDone() returns True before end_stream
        # is readable, causing premature HTTPCLIENT-STREAM-CLOSED
        if (data_channel.isClosed() && data_channel.empty()) {
            throw "HTTPCLIENT-STREAM-CLOSED", "stream is closed";
        }

        try {
            auto rv = data_channel.recv(wait_timeout);
            # The poll operation pushes raw output hashes to the channel.
            # Extract body data and detect end_stream/error for the caller.
            if (rv.typeCode() == NT_HASH) {
                # ChannelAction::executeError pushes {err, desc} (no nested
                # "error" key), so match on `rv.err` — the same check H3
                # uses in its readData().  Previously the code looked for
                # rv.error which never exists on cancel-pushed errors, so
                # cancel-generated hashes fell through to the
                # "unrecognized metadata" path and state stayed non-Error.
                if (rv.err) {
                    {
                        AutoLock al(lock);
                        error_info = rv;
                        state = HttpClientStreamState::Error;
                    }
                    return {"error": rv};
                }
                # State transitions (monotonically forward)
                {
                    AutoLock al(lock);
                    if (rv.headers && state == HttpClientStreamState::Open) {
                        state = HttpClientStreamState::HeadersReceived;
                        response = rv;
                    }
                    if (rv.body && state < HttpClientStreamState::Complete) {
                        state = HttpClientStreamState::ReceivingBody;
                    }
                    if (rv.end_stream && state < HttpClientStreamState::Complete) {
                        state = HttpClientStreamState::Complete;
                        # Merge rather than overwrite so status_code/headers
                        # captured from an earlier headers event are not lost
                        # when the end_stream event carries no headers — this
                        # is required for extractFinalHeaders() to surface a
                        # non-NOTHING status_code and for the `status_code >=
                        # 300` error-propagation check in doSendDataStream().
                        response += rv;
                        # If body is coalesced with end_stream, return body
                        # now and defer the end_stream sentinel so the
                        # caller doesn't lose the completion signal.
                        if (rv.body) {
                            pending_end_stream = True;
                        }
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
                # (callers handle unknown hashes by skipping them)
                return rv;
            }
            return rv;
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
            if (!stream_id) {
                throw "HTTPCLIENT-STATE-ERROR", "stream not yet established";
            }
            if (!request_body_streaming) {
                if (end_stream && data.empty()) {
                    return;
                }
                throw "HTTPCLIENT-STATE-ERROR",
                    "cannot send data: request write side is closed";
            }
            sid = stream_id;
        }

        # Check connection state (lock-free atomic read) AFTER the stream-level
        # checks.  The underlying H2 connection may have been closed (GOAWAY,
        # TCP reset) while the stream handle's state is still Open.  Catching
        # this here produces a clean HTTPCLIENT-STREAM-CLOSED instead of the
        # opaque HTTP2-STATE-ERROR from sendStreamData().
        if (!h2conn.isReady()) {
            if (end_stream && data.empty()) {
                return;
            }
            throw "HTTPCLIENT-STREAM-CLOSED",
                sprintf("cannot send data: H2 connection closed (state: %y)",
                    h2conn.getConnectionState());
        }

        log(LoggerLevel::DEBUG, "sending data on stream %d (end_stream=%y)", sid, end_stream);
        h2conn.sendStreamData(sid, data, end_stream);
    }

    #! Returns trailers from the completed response
    *hash<string, string> getTrailers() {
        AutoLock al(lock);
        return response.trailers;
    }

}

} # namespace HttpClientIo
