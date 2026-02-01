# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AbstractSseConnection.qc Abstract SSE connection interface

/** AbstractSseConnection.qc Copyright 2025 - 2026 Qore Technologies, s.r.o.

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

#! Abstract interface for Server-Sent Events connections
/** This abstract class provides a protocol-agnostic interface for SSE connections,
    supporting both HTTP/1.x and HTTP/2 transports transparently.

    Implementations:
    - @ref HttpSseStreamPollOperation for HTTP/1.x
    - @ref Http2SseStream for HTTP/2

*/
public class AbstractSseConnection {
    #! Returns the underlying transport protocol
    /** @return "http1" for HTTP/1.x chunked transfer, "http2" for HTTP/2 stream
    */
    abstract string getProtocol();

    #! Queues an SSE event for sending
    /** @param event_type the event type (optional)
        @param data the event data
        @param id the event ID (optional)
        @param retry retry interval in ms (optional)
    */
    abstract queueEvent(*string event_type, string data, *string id, *int retry);

    #! Queues a raw SSE message
    /** @param event hash with optional keys: type, data, id, retry
    */
    abstract queueRawEvent(hash<auto> event);

    #! Returns True if the connection is still open
    abstract bool isOpen();

    #! Returns True if the client disconnected
    abstract bool isDisconnected();

    #! Returns the number of queued events
    abstract int getQueueSize();

    #! Closes the SSE connection
    abstract close();

    #! Aborts the connection immediately
    abstract abort();
}

#! HTTP/1.x SSE connection wrapper
/** Wraps HttpSseStreamPollOperation to implement AbstractSseConnection interface.
    This allows existing HTTP/1.x SSE code to work with the unified interface.

*/
public class Http1SseConnection inherits AbstractSseConnection {
    private {
        HttpSseStreamPollOperation poll_op;
    }

    #! Creates wrapper around an HTTP/1.x SSE poll operation
    constructor(HttpSseStreamPollOperation poll_op) {
        self.poll_op = poll_op;
    }

    string getProtocol() {
        return "http1";
    }

    queueEvent(*string event_type, string data, *string id, *int retry) {
        poll_op.queueEvent(event_type, data, id, retry);
    }

    queueRawEvent(hash<auto> event) {
        poll_op.queueRawEvent(event);
    }

    bool isOpen() {
        return !poll_op.isDisconnected() && poll_op.getState() == HttpSseStreamPollOperation::STATE_STREAMING;
    }

    bool isDisconnected() {
        return poll_op.isDisconnected();
    }

    int getQueueSize() {
        return poll_op.getQueueSize();
    }

    close() {
        poll_op.abort();
    }

    abort() {
        poll_op.abort();
    }

    #! Returns the underlying poll operation for direct access if needed
    HttpSseStreamPollOperation getPollOperation() {
        return poll_op;
    }
}

} # namespace HttpServerAsyncIo
