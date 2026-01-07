# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2SseStream.qc SSE streaming over HTTP/2

/** Http2SseStream.qc Copyright 2025 Qore Technologies, s.r.o.

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
/** This class manages a Server-Sent Events stream over an HTTP/2 connection.
    Unlike HTTP/1.x SSE which uses chunked transfer encoding, HTTP/2 SSE uses
    DATA frames on the stream.

    The stream is kept open (no END_STREAM flag) until explicitly closed,
    allowing the server to send events incrementally.

    @since HttpServerAsyncIo 1.0
*/
public class Http2SseStream inherits AbstractSseConnection {
    private {
        #! The HTTP/2 poll operation managing this connection
        Http2PollOperation poll_op;

        #! The stream ID for this SSE stream
        int stream_id;

        #! Queue of events to send
        Queue event_queue();

        #! True if the stream is open
        bool stream_open = True;

        #! True if response headers have been sent
        bool headers_sent = False;
    }

    #! Creates an SSE stream over HTTP/2
    /** @param poll_op the HTTP/2 poll operation for the connection
        @param stream_id the HTTP/2 stream ID to use for SSE
    */
    constructor(Http2PollOperation poll_op, int stream_id) {
        self.poll_op = poll_op;
        self.stream_id = stream_id;
    }

    string getProtocol() {
        return "http2";
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
        return stream_open && poll_op.getSocket().isOpen();
    }

    bool isDisconnected() {
        return !stream_open || !poll_op.getSocket().isOpen();
    }

    int getQueueSize() {
        return event_queue.size();
    }

    close() {
        stream_open = False;
        # Send END_STREAM on the HTTP/2 stream
        # This is handled by the poll operation when we stop sending data
    }

    abort() {
        stream_open = False;
        try {
            poll_op.abort();
        } catch () {
            # Ignore errors on abort
        }
    }

    #! Returns the HTTP/2 stream ID
    int getStreamId() {
        return stream_id;
    }

    #! Returns the underlying poll operation
    Http2PollOperation getPollOperation() {
        return poll_op;
    }

    #! Sends response headers to start the SSE stream
    /** This must be called before sending any events.
        Sends a 200 response with Content-Type: text/event-stream
    */
    sendHeaders(*hash<auto> extra_headers) {
        if (headers_sent) {
            throw "SSE-HEADERS-SENT", "SSE headers already sent";
        }

        hash<auto> headers = extra_headers ?? {};
        headers."Content-Type" = "text/event-stream";
        headers."Cache-Control" = "no-cache";
        headers."Connection" = "keep-alive";

        # Start streaming response (no body yet, no END_STREAM)
        poll_op.startSendResponse(200, headers);
        headers_sent = True;
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
