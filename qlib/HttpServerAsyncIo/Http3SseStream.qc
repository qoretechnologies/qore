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
    }

    #! Creates an SSE stream over HTTP/3
    /** @param sock the socket for the QUIC connection
        @param session_id the QUIC session ID
        @param stream_id the HTTP/3 stream ID to use for SSE
        @param wake_callback optional callback to wake the I/O thread after data submission
    */
    constructor(Socket sock, int session_id, int stream_id, *code<nothing()> wake_callback) {
        self.sock = sock;
        self.session_id = session_id;
        self.stream_id = stream_id;
        self.wake_callback = wake_callback;
    }

    string getProtocol() {
        return "http3";
    }

    queueEvent(*string event_type, string data, *string id, *int retry) {
        if (!stream_open) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        event_queue.push({
            "type": event_type,
            "data": data,
            "id": id,
            "retry": retry,
        });
    }

    queueRawEvent(hash<auto> event) {
        if (!stream_open) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        event_queue.push(event);
    }

    bool isOpen() {
        return stream_open && sock.isOpen();
    }

    bool isDisconnected() {
        return !stream_open || !sock.isOpen();
    }

    int getQueueSize() {
        return event_queue.size();
    }

    close() {
        if (stream_open) {
            stream_open = False;
            try {
                # Send END_STREAM to close the HTTP/3 stream
                sock.submitQuicStreamData(session_id, stream_id, NOTHING, True);
                if (wake_callback) {
                    wake_callback();
                }
            } catch () {
                # Ignore errors on close
            }
        }
    }

    abort() {
        stream_open = False;
        try {
            sock.cancelQuicStream(session_id, stream_id);
        } catch () {
            # Ignore errors on abort
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
    }

    #! Sends an SSE event on the HTTP/3 stream
    /** Formats and sends the event immediately. Handles backpressure via
        waitForQuicStreamDrain().

        @param event hash with optional keys: type, data, id, retry
    */
    sendEvent(hash<auto> event) {
        if (!stream_open) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        if (!headers_sent) {
            sendHeaders();
        }
        string msg = Http2SseStream::formatSseEvent(event);
        # Send data with backpressure handling
        while (sock.submitQuicStreamData(session_id, stream_id, msg, False) == 1) {
            if (wake_callback) {
                wake_callback();
            }
            sock.waitForQuicStreamDrain(session_id, stream_id, 5s);
        }
        # Wake the I/O thread so it sends the queued data
        if (wake_callback) {
            wake_callback();
        }
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
}

} # namespace HttpServerAsyncIo
