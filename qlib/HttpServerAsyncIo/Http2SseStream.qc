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

    @since %HttpServerAsyncIo 1.3 unified with @ref HttpSseStreamPollOperation
    via @ref AbstractSseConnection
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

        @since %HttpServerAsyncIo 1.3
    */
    constructor(Socket sock, int stream_id, *code<nothing()> wake_callback,
            bool headers_already_sent = False) {
        self.sock = sock;
        self.stream_id = stream_id;
        self.wake_callback = wake_callback;
        self.headers_sent = headers_already_sent;
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
        if (!stream_open) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
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
                # Any other failure (stream closed/reset, session gone, etc.) means the
                # underlying HTTP/2 stream is effectively dead.  Flip stream_open so
                # isOpen()/isDisconnected() report the true state and subsequent sends
                # fail fast with SSE-CLOSED rather than surfacing raw transport errors
                # to every caller.
                stream_open = False;
                rethrow;
            }
        }
        if (wake_callback) {
            wake_callback();
        }
    }

    bool isOpen() {
        return stream_open && sock.isOpen();
    }

    bool isDisconnected() {
        return !stream_open || !sock.isOpen();
    }

    #! Returns 0 — events are written synchronously, there is no internal queue
    int getQueueSize() {
        return 0;
    }

    close() {
        if (!stream_open) {
            return;
        }
        stream_open = False;
        sock.sendHttp2StreamData(stream_id, NOTHING, True);
        if (wake_callback) {
            wake_callback();
        }
    }

    abort() {
        stream_open = False;
        sock.resetHttp2Stream(stream_id);
        if (wake_callback) {
            wake_callback();
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
}

} # namespace HttpServerAsyncIo
