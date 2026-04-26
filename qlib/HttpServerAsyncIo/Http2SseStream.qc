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

    Unlike @ref HttpSseStreamPollOperation (which queues events for an I/O
    thread loop to drain), this class writes DATA frames directly on the
    calling thread via @ref Qore::Socket::sendHttp2StreamData(), then wakes
    the async I/O controller so the nghttp2 session's \c sendPendingData()
    flushes them to the socket.  The write is thread-safe because
    \c sendHttp2StreamData() acquires the HTTP/2 session lock internally.

    Backpressure from @ref Qore::Socket::sendHttp2StreamData() is handled
    by waking the controller and blocking on
    @ref Qore::Socket::waitForHttp2StreamDrain().
*/
public class Http2SseStream inherits AbstractSseConnection {
    public {
        #! Default timeout for waitForHttp2StreamDrain on backpressure
        const DefaultDrainTimeout = 5s;
    }

    private {
        #! Socket the HTTP/2 session is running on
        Socket sock;

        #! The HTTP/2 stream ID for this SSE stream
        int stream_id;

        #! Optional callback to wake the async I/O controller after each write
        /** Called after @ref queueRawString() and @ref queueRawEvent() submit
            data so that the I/O thread flushes queued DATA frames promptly.
            Without this, events sit in the nghttp2 send buffer until the next
            frame-read cycle — effectively forever for a server-push SSE
            connection with no client traffic.

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

        #! Epoch microseconds of the last successful SSE DATA write
        int last_send_us = clock_getmicros();
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
    queueEvent(*string event_type, string data, *string id, *int retry) {
        queueRawEvent({
            "type": event_type,
            "data": data,
            "id": id,
            "retry": retry,
        });
    }

    #! Queues a raw SSE event hash for sending
    queueRawEvent(hash<auto> event) {
        queueRawString(formatSseEvent(event));
    }

    #! Pushes a pre-formatted SSE text frame onto the HTTP/2 stream
    /** Writes the frame as one or more HTTP/2 DATA frames.  Safe to call from
        any thread.  Blocks briefly under HTTP/2 flow-control backpressure
        (up to @ref DefaultDrainTimeout per iteration) and wakes the I/O
        controller after every submission.

        @param data the pre-formatted SSE frame text
    */
    queueRawString(string data) {
        if (!isStreamOpen()) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        try {
            sendRawStringIntern(data);
            noteSseDataSent();
        } catch (hash<ExceptionInfo> ex) {
            markStreamClosed();
            rethrow;
        }
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
        last_send_us = clock_getmicros();
        rescheduleHeartbeatTimerLocked();
    }

    private sendRawStringIntern(string data) {
        binary bin = binary(data);
        while (True) {
            try {
                sock.sendHttp2StreamData(stream_id, bin, False);
                break;
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "HTTP2-FLOW-CONTROL") {
                    if (wake_callback) {
                        wake_callback();
                    }
                    sock.waitForHttp2StreamDrain(stream_id, DefaultDrainTimeout);
                    continue;
                }
                rethrow;
            }
        }
        if (wake_callback) {
            wake_callback();
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

    #! Returns 0 — events are written synchronously, there is no internal queue
    int getQueueSize() {
        return 0;
    }

    close() {
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
        hash<auto> headers = extra_headers ?? {};
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
    */
    static string formatSseEvent(hash<auto> event) {
        string msg = "";

        if (event.id) {
            msg += "id: " + event.id + "\n";
        }
        if (event.type) {
            msg += "event: " + event.type + "\n";
        }
        if (event.retry) {
            msg += "retry: " + event.retry + "\n";
        }
        if (event.data) {
            # Split data by newlines and prefix each line with "data: "
            list<string> lines = event.data.split("\n");
            foreach string line in (lines) {
                msg += "data: " + line + "\n";
            }
        }
        msg += "\n";  # Empty line terminates the event
        return msg;
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
        last_send_us = clock_getmicros();
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
        int elapsed_ms = (clock_getmicros() - last_send_us) / 1000;
        int delay_ms = heartbeat_ms - elapsed_ms;
        if (delay_ms < 1) {
            delay_ms = 1;
        }
        int generation = heartbeat_generation;
        AsyncSocketIo::AsyncSocketIoController controller =
            AsyncSocketIo::getGlobalAsyncIoController();
        heartbeat_timer_id = controller.addTimer(now_us() + milliseconds(delay_ms),
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
            int elapsed_ms = (clock_getmicros() - last_send_us) / 1000;
            if (elapsed_ms < heartbeat_ms) {
                rescheduleHeartbeatTimerLocked();
                return;
            }
            msg = heartbeat_msg;
        }

        try {
            sendRawStringIntern(msg);
            noteSseDataSent();
        } catch (hash<ExceptionInfo> ex) {
            markStreamClosed();
        }
    }
}

} # namespace HttpServerAsyncIo
