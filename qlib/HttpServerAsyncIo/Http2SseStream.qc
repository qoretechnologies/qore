# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2SseStream.qc SSE streaming over HTTP/2

/** Http2SseStream.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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
public namespace HttpServerAsyncIo {

#! SSE connection over HTTP/2 stream
/** Pushes Server-Sent Events over an HTTP/2 stream as DATA frames.  The
    stream is kept open (no END_STREAM flag) until explicitly closed,
    allowing the server to push events incrementally.

    Unlike @ref HttpSseStreamPollOperation (which keeps an explicit event
    queue for an I/O-thread poll loop to drain), this class hands each frame
    to @ref Qore::Socket::sendHttp2StreamData().  That public Socket API
    delegates HTTP/2 DATA enqueue to the async I/O controller and wakes the
    owning poll operation to flush nghttp2 pending data.

    Queueing never waits for the client: the data buffered for the stream beyond the client's flow-control window
    is bounded (see @ref setMaxBufferSize(), applied with @ref Qore::Socket::setHttp2StreamSendLimit()), and an
    event that would exceed the bound drops the client: the stream is reset with \c RST_STREAM(CANCEL), its end is
    reported, and the call that queued the event raises \c SSE-CLIENT-OVERFLOW (see
    @ref httpserverasyncio_sse_buffer).
*/
public class Http2SseStream inherits AbstractSseConnection {
    public {
        #! Formerly the timeout of the wait for the stream to drain; no longer used, as queueing never waits
        const DefaultDrainTimeout = 5s;
    }

    private {
        #! Socket the HTTP/2 session is running on
        Socket sock;

        #! The HTTP/2 stream ID for this SSE stream
        int stream_id;

        #! Optional callback to wake the async I/O controller after each write
        /** Called after @ref queueRawString() and @ref queueRawEvent() submit
            data.  The Socket API already wakes the owning async I/O
            controller after a successful enqueue; this callback is retained
            for callers that want an explicit wake hook around the higher-level
            SSE operation.

            @see @ref AsyncSocketIo::AsyncSocketIoController::wakeSocket()
        */
        *code<nothing()> wake_callback;

        #! True while the stream is open
        bool stream_open = True;

        #! True once response headers have been submitted
        bool headers_sent = False;

        #! Mutex protecting heartbeat timer state
        Mutex heartbeat_mutex();

        #! Heartbeat interval in milliseconds; 0 disables the heartbeat
        int heartbeat_ms = DefaultSseHeartbeatIntervalMs;

        #! Heartbeat payload sent when the interval expires with no outbound traffic
        string heartbeat_msg = DefaultSseHeartbeatMsg;

        #! Async I/O controller timer ID for the next heartbeat
        *int heartbeat_timer_id;

        #! Generation value used to ignore canceled or superseded timer callbacks
        int heartbeat_generation = 0;

        #! Monotonic-clock microseconds of the last successful SSE DATA write
        int last_send_us = clock_get_monotonic_us();
    }

    #! Creates an SSE stream over HTTP/2 using an existing poll operation
    /** Legacy constructor; kept for compatibility with callers that already
        hold the @ref Http2PollOperation.  Prefer the socket-based constructor
        for new code.

        @param poll_op the HTTP/2 poll operation for the connection
        @param stream_id the HTTP/2 stream ID to use for SSE
    */
    constructor(Http2PollOperation poll_op, int stream_id) {
        self.sock = poll_op.getSocket();
        self.stream_id = stream_id;
    }

    #! Creates an SSE stream over HTTP/2 directly on a socket
    /** @param sock the socket the HTTP/2 session is running on
        @param stream_id the HTTP/2 stream ID to use for SSE
        @param wake_callback optional callback to wake the async I/O controller
            after each write
        @param headers_already_sent set to True if the caller already submitted
            the response headers (via @ref Qore::Socket::submitHttp2StreamingResponseHeaders());
            in that case @ref sendHeaders() must not be called again
    */
    constructor(Socket sock, int stream_id, *code<nothing()> wake_callback,
            bool headers_already_sent = False) {
        self.sock = sock;
        self.stream_id = stream_id;
        self.wake_callback = wake_callback;
        self.headers_sent = headers_already_sent;
        if (headers_sent) {
            rescheduleHeartbeatTimer();
        }
    }

    string getProtocol() {
        return "http2";
    }

    #! Queues a structured SSE event for sending
    /** @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer; nothing is queued (see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat")
    */
    queueEvent(*string event_type, string data, *string id, *int retry) {
        queueRawEvent({
            "type": event_type,
            "data": data,
            "id": id,
            "retry": retry,
        });
    }

    #! Queues a raw SSE event hash for sending
    /** @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer; nothing is queued (see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat")
    */
    queueRawEvent(hash<auto> event) {
        queueRawString(formatSseEvent(event));
    }

    #! Pushes a pre-formatted SSE text frame onto the HTTP/2 stream
    /** Writes the frame as one or more HTTP/2 DATA frames.  Safe to call from
        any thread; never waits for the client, and wakes the I/O controller
        after every submission.

        @param data the pre-formatted SSE frame text

        @throw SSE-CLOSED the stream is closed
        @throw SSE-CLIENT-OVERFLOW the frame would take the data buffered for the client over its bound (see
        @ref setMaxBufferSize()); the frame is not queued, and the client has been dropped
    */
    queueRawString(string data) {
        if (!isStreamOpen()) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        bool queued;
        try {
            queued = sendRawStringIntern(data);
        } catch (hash<ExceptionInfo> ex) {
            markStreamClosed();
            rethrow;
        }
        if (!queued) {
            dropOverflowingClient(data.size());
        }
        noteSseDataSent();
    }

    #! Configures the SSE heartbeat for this stream
    /** The heartbeat is an SSE comment frame sent only after the stream has had
        no outbound SSE DATA traffic for \a interval_ms.  Timers are driven by
        the async I/O controller, and each heartbeat wakes the HTTP/2 connection
        so queued DATA frames are flushed promptly.

        @param interval_ms heartbeat interval in milliseconds; 0 disables the heartbeat
        @param msg raw bytes to send (must already be SSE-framed, e.g. \c ":hb\\n\\n")
    */
    setHeartbeat(int interval_ms, string msg = DefaultSseHeartbeatMsg) {
        AutoLock al(heartbeat_mutex);
        heartbeat_ms = interval_ms;
        heartbeat_msg = msg;
        last_send_us = clock_get_monotonic_us();
        rescheduleHeartbeatTimerLocked();
    }

    #! Hands data to the stream; returns @ref False if it would take the buffered data over the stream's bound
    private bool sendRawStringIntern(string data) {
        try {
            sock.sendHttp2StreamData(stream_id, binary(data), False);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "HTTP2-FLOW-CONTROL") {
                return False;
            }
            rethrow;
        }
        if (wake_callback) {
            wake_callback();
        }
        return True;
    }

    #! Sets the bound of the data buffered for the stream in the HTTP/2 session
    private applyMaxBufferSize(int size) {
        try {
            sock.setHttp2StreamSendLimit(stream_id, size);
        } catch (hash<ExceptionInfo> ex) {
            # the stream has already ended: its next send reports it
        }
    }

    bool isOpen() {
        AutoLock al(heartbeat_mutex);
        return stream_open && sock.isOpen();
    }

    bool isDisconnected() {
        AutoLock al(heartbeat_mutex);
        return !stream_open || !sock.isOpen();
    }

    #! Returns 0 — events are handed to the controller immediately, with no adapter queue
    int getQueueSize() {
        return 0;
    }

    close() {
        # the end of the stream is reported however it ends (see setEndCallback()), and first: the teardown below
        # can release the last reference to this object
        notifyEnded();
        if (!markStreamClosed()) {
            return;
        }
        # close() is a teardown path: swallow HTTP/2 transport errors
        # (stream already reset, session gone, GOAWAY in flight, etc.) so
        # cleanup callers don't have to wrap every close in try/catch.
        # stream_open has already been flipped, so isOpen() / isDisconnected()
        # report the closed state regardless of whether END_STREAM lands.
        try {
            sock.sendHttp2StreamData(stream_id, NOTHING, True);
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
        }
    }

    abort() {
        notifyEnded();
        markStreamClosed();
        # abort() is a teardown path: same rationale as close().
        try {
            sock.resetHttp2Stream(stream_id);
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
        }
    }

    #! Returns the HTTP/2 stream ID
    int getStreamId() {
        return stream_id;
    }

    #! Returns the underlying socket
    Socket getSocket() {
        return sock;
    }

    #! Submits the response headers that start the SSE stream
    /** Must not be called if the constructor was invoked with
        \c headers_already_sent = True.

        @param extra_headers additional response headers to include alongside
            \c Content-Type: text/event-stream
    */
    sendHeaders(*hash<auto> extra_headers) {
        if (headers_sent) {
            throw "SSE-HEADERS-SENT", "SSE headers already sent";
        }
        # the headers are set in an "auto!" local: a caller's literal may be narrowed (e.g. hash<string, int> for
        # {"X-Count": 1}), and adding the content-type header would throw
        hash<auto!> headers = extra_headers ?? {};
        headers."content-type" = "text/event-stream";
        headers."cache-control" = "no-cache";
        sock.submitHttp2StreamingResponseHeaders(stream_id, 200, headers);
        headers_sent = True;
        if (wake_callback) {
            wake_callback();
        }
        noteSseDataSent();
    }

    #! Formats an SSE event according to the spec
    /** @param event hash with optional keys: type, data, id, retry
        @return formatted SSE event string

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer; see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat"
    */
    static string formatSseEvent(hash<auto> event) {
        return SseEventFormat::formatEvent(event);
    }

    private bool isStreamOpen() {
        AutoLock al(heartbeat_mutex);
        return stream_open;
    }

    private bool markStreamClosed() {
        AutoLock al(heartbeat_mutex);
        if (!stream_open) {
            return False;
        }
        stream_open = False;
        cancelHeartbeatTimerLocked();
        return True;
    }

    private noteSseDataSent() {
        AutoLock al(heartbeat_mutex);
        last_send_us = clock_get_monotonic_us();
        rescheduleHeartbeatTimerLocked();
    }

    private rescheduleHeartbeatTimer() {
        AutoLock al(heartbeat_mutex);
        rescheduleHeartbeatTimerLocked();
    }

    private rescheduleHeartbeatTimerLocked() {
        cancelHeartbeatTimerLocked();
        if (heartbeat_ms <= 0 || !stream_open || !headers_sent) {
            return;
        }
        int elapsed_ms = (clock_get_monotonic_us() - last_send_us) / 1000;
        int delay_ms = heartbeat_ms - elapsed_ms;
        if (delay_ms < 1) {
            delay_ms = 1;
        }
        int generation = heartbeat_generation;
        AsyncSocketIo::AsyncSocketIoController controller =
            AsyncSocketIo::getGlobalAsyncIoController();
        heartbeat_timer_id = controller.addTimerDelay(milliseconds(delay_ms),
            sub () { self.handleHeartbeatTimer(generation); });
    }

    private cancelHeartbeatTimerLocked() {
        ++heartbeat_generation;
        if (heartbeat_timer_id) {
            try {
                AsyncSocketIo::AsyncSocketIoController controller =
                    AsyncSocketIo::getGlobalAsyncIoController();
                controller.cancelTimer(heartbeat_timer_id);
            } catch (hash<ExceptionInfo> ex) {
            }
            remove heartbeat_timer_id;
        }
    }

    private handleHeartbeatTimer(int generation) {
        string msg;
        {
            AutoLock al(heartbeat_mutex);
            if (!stream_open || !headers_sent || heartbeat_ms <= 0
                    || generation != heartbeat_generation) {
                return;
            }
            remove heartbeat_timer_id;
            int elapsed_ms = (clock_get_monotonic_us() - last_send_us) / 1000;
            if (elapsed_ms < heartbeat_ms) {
                rescheduleHeartbeatTimerLocked();
                return;
            }
            msg = heartbeat_msg;
        }

        try {
            if (!sendRawStringIntern(msg)) {
                # a client that does not take even the heartbeat is dropped; no caller gets the error
                try {
                    dropOverflowingClient(msg.size());
                } catch (hash<ExceptionInfo> ex) {
                }
                return;
            }
            noteSseDataSent();
        } catch (hash<ExceptionInfo> ex) {
            markStreamClosed();
        }
    }
}

} # namespace HttpServerAsyncIo
