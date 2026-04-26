# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3SseStream.qc SSE streaming over HTTP/3

/** Http3SseStream.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! SSE connection over HTTP/3 stream
/** This class manages a Server-Sent Events stream over an HTTP/3/QUIC connection.
    Uses the QUIC streaming API (submitQuicResponseStreaming / submitQuicStreamData)
    to send SSE events as DATA frames on an HTTP/3 stream.

    The stream is kept open (no END_STREAM flag) until explicitly closed,
    allowing the server to send events incrementally.

    @since Qore 2.1
*/
public class Http3SseStream inherits AbstractSseConnection {
    private {
        #! The socket for the QUIC connection
        Socket sock;

        #! The QUIC session ID
        int session_id;

        #! The HTTP/3 stream ID for this SSE stream
        int stream_id;

        #! Optional callback to wake the I/O thread after submitting data
        /** Called after each @ref submitQuicStreamData() call so the I/O thread
            processes the queued data immediately rather than waiting for the next
            timer or socket event.

            @see @ref AsyncSocketIo::AsyncSocketIoController::wake()
        */
        *code<nothing()> wake_callback;

        #! Queue of events to send
        Queue event_queue();

        #! True if the stream is open
        bool stream_open = True;

        #! True if response headers have been sent
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

    #! Creates an SSE stream over HTTP/3
    /** @param sock the socket for the QUIC connection
        @param session_id the QUIC session ID
        @param stream_id the HTTP/3 stream ID to use for SSE
        @param wake_callback optional callback to wake the I/O thread after data submission
        @param headers_already_sent set to True if the caller already submitted
            the response headers (via @ref Qore::Socket::submitQuicResponseStreaming());
            in that case @ref sendHeaders() must not be called again
    */
    constructor(Socket sock, int session_id, int stream_id, *code<nothing()> wake_callback,
            bool headers_already_sent = False) {
        self.sock = sock;
        self.session_id = session_id;
        self.stream_id = stream_id;
        self.wake_callback = wake_callback;
        self.headers_sent = headers_already_sent;
        if (headers_sent) {
            rescheduleHeartbeatTimer();
        }
    }

    string getProtocol() {
        return "http3";
    }

    queueEvent(*string event_type, string data, *string id, *int retry) {
        queueRawEvent({
            "type": event_type,
            "data": data,
            "id": id,
            "retry": retry,
        });
    }

    queueRawEvent(hash<auto> event) {
        queueRawString(Http2SseStream::formatSseEvent(event));
    }

    #! Pushes a pre-formatted SSE text frame onto the HTTP/3 stream
    /** Writes the frame as DATA on the QUIC stream.  Safe to call from any
        thread.  Handles QUIC stream-level backpressure by waking the I/O
        controller and blocking on
        @ref Qore::Socket::waitForQuicStreamDrain().

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
        the async I/O controller, and each heartbeat wakes the HTTP/3 connection
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
        try {
            while (sock.submitQuicStreamData(session_id, stream_id, data, False) == 1) {
                if (wake_callback) {
                    wake_callback();
                }
                sock.waitForQuicStreamDrain(session_id, stream_id, 5s);
            }
            if (wake_callback) {
                wake_callback();
            }
        } catch (hash<ExceptionInfo> ex) {
            rethrow;
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

    int getQueueSize() {
        return event_queue.size();
    }

    close() {
        if (!markStreamClosed()) {
            return;
        }
        # close() is a teardown path: swallow QUIC transport errors
        # (stream/session already gone, GOAWAY in flight, etc.) so
        # cleanup callers don't have to wrap every close in try/catch.
        # stream_open has already been flipped, so isOpen() / isDisconnected()
        # report the closed state regardless of whether END_STREAM lands.
        try {
            sock.submitQuicStreamData(session_id, stream_id, NOTHING, True);
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
            sock.cancelQuicStream(session_id, stream_id);
        } catch (hash<ExceptionInfo> ex) {
        }
    }

    #! Returns the QUIC session ID
    int getSessionId() {
        return session_id;
    }

    #! Returns the HTTP/3 stream ID
    int getStreamId() {
        return stream_id;
    }

    #! Returns the wake callback
    *code<nothing()> getWakeCallback() {
        return wake_callback;
    }

    #! Returns the underlying socket
    Socket getSocket() {
        return sock;
    }

    #! Sends response headers to start the SSE stream
    /** This must be called before sending any events.
        Sends a 200 response with Content-Type: text/event-stream via deferred body
    */
    sendHeaders(*hash<auto> extra_headers) {
        if (headers_sent) {
            throw "SSE-HEADERS-SENT", "SSE headers already sent";
        }

        hash<auto> headers = extra_headers ?? {};
        headers."content-type" = "text/event-stream";
        headers."cache-control" = "no-cache";

        # Start streaming response (deferred body, no END_STREAM)
        sock.submitQuicResponseStreaming(session_id, stream_id, 200, headers);
        headers_sent = True;
        if (wake_callback) {
            wake_callback();
        }
        noteSseDataSent();
    }

    #! Sends an SSE event on the HTTP/3 stream
    /** Formats and sends the event immediately. Handles backpressure via
        waitForQuicStreamDrain().

        @param event hash with optional keys: type, data, id, retry
    */
    sendEvent(hash<auto> event) {
        if (!isStreamOpen()) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        if (!headers_sent) {
            sendHeaders();
        }
        queueRawEvent(event);
    }

    #! Gets the next event from the queue (for sending)
    /** @return event hash or NOTHING if queue is empty
    */
    *hash<auto> getNextEvent() {
        try {
            return event_queue.get(0ms);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "QUEUE-TIMEOUT") {
                return NOTHING;
            }
            rethrow;
        }
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
