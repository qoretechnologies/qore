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
    supporting HTTP/1.x, HTTP/2, and HTTP/3 transports transparently.

    Implementations:
    - @ref HttpSseStreamPollOperation for HTTP/1.x
    - @ref Http2SseStream for HTTP/2
    - @ref Http3SseStream for HTTP/3

*/
public class AbstractSseConnection {
    private:internal {
        #! Callback run once when the transport reports that the peer is gone
        *code<nothing()> peer_close_callback;

        #! Set when @ref firePeerCloseCallback() has already run the callback
        bool peer_close_fired = False;

        #! Protects the peer-close callback slot
        Mutex peer_close_mutex();
    }

    #! Returns the underlying transport protocol
    /** @return "http1" for HTTP/1.x chunked transfer, "http2" for HTTP/2 stream
    */
    abstract string getProtocol();

    #! Sets the callback run when the transport detects that the peer disconnected
    /** Multiplexed transports (HTTP/2, HTTP/3) do not close the socket when a
        single response stream dies, so an SSE stream on such a transport learns
        about the peer only from an explicit notification.  The HTTP server sets
        this callback to deregister the connection from its handler, mirroring
        @ref HttpSseStreamPollOperation::setCloseCallback() on the HTTP/1.x path.

        @param cb the callback; called at most once, from a worker thread
    */
    setPeerCloseCallback(*code<nothing()> cb) {
        AutoLock al(peer_close_mutex);
        peer_close_callback = cb;
    }

    #! Marks the stream closed and runs the peer-close callback exactly once
    /** Called by the transport-close notification path.  Idempotent: repeated
        calls (a stream reset followed by the connection close, for instance) run
        the callback only the first time.
    */
    notifyPeerClosed() {
        *code<nothing()> cb;
        {
            AutoLock al(peer_close_mutex);
            if (peer_close_fired) {
                return;
            }
            peer_close_fired = True;
            cb = peer_close_callback;
        }
        # close the stream first so any concurrent send fails fast instead of
        # writing into a dead transport
        try {
            close();
        } catch (hash<ExceptionInfo> ex) {
            # teardown path: the transport is already gone
        }
        if (cb) {
            cb();
        }
    }

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

    #! Queues a pre-formatted SSE text frame for sending
    /** The string must already be formatted per the SSE spec (e.g., from
        @ref ServerSentEventHandler::encodeMessage()).  Used by
        @ref ServerSentEventConnection::sendEncoded() to deliver events across
        all HTTP protocol versions through a uniform write path.

        @param data the pre-formatted SSE frame text
    */
    abstract queueRawString(string data);

    #! Configures the SSE heartbeat for this connection
    /** A heartbeat is a comment-only SSE frame sent automatically when the
        stream has had no outbound traffic for \a interval_ms; this prevents
        proxies and load balancers from dropping idle connections.  Heartbeats
        are enabled by default; pass \a interval_ms = \c 0 to opt out.

        @param interval_ms heartbeat interval in milliseconds; 0 disables the heartbeat
        @param msg raw bytes to send (must already be SSE-framed, e.g. \c ":hb\\n\\n")
    */
    abstract setHeartbeat(int interval_ms, string msg = DefaultSseHeartbeatMsg);

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

#! Stream listener that tears down a multiplexed SSE stream when its peer goes away
/** HTTP/2 and HTTP/3 multiplex many streams onto one transport, so neither a
    socket close nor a failed write reports the death of a single SSE response
    stream: the client can reset just that stream and keep using the connection,
    and a peer that vanishes entirely takes the connection down without the SSE
    stream ever being told.  Under HTTP/1.x the socket *is* the stream, which is
    why @ref HttpSseStreamPollOperation needs none of this.

    An SSE response stream also registers no inbound Queue — the request is
    complete and nothing further arrives from the peer — so it never sees the
    \c NOTHING sentinel that reports teardown to a CONNECT tunnel.  This class
    closes that gap: it registers as a stream listener on the connection's poll
    operation, and on every wakeup asks the transport whether the stream is
    actually dead.  When it is, the stream is deregistered from the poll
    operation (so the listener map stays bounded) and
    @ref AbstractSseConnection::notifyPeerClosed() runs the SSE handler's
    deregistration exactly once.

    @note \c notifyIo() runs on an async I/O controller worker thread, or — when the peer
    reset was already recorded before this listener was registered — inline on the
    registering handler thread as the replay from
    @ref HttpServerAsyncIo::Http2PollOperation::registerStreamListener() "Http2PollOperation::registerStreamListener()"
    / @ref Qore::Http3ServerPollOperation::registerStreamListener() "Http3ServerPollOperation::registerStreamListener()".
    It never runs on the I/O thread.
*/
public class SseStreamCloseNotifier {
    private {
        #! The SSE stream to tear down
        AbstractSseConnection sse;

        #! Returns True when the transport reports the stream as dead
        code<bool()> closed_check;

        #! Removes this listener from the owning poll operation
        *code<nothing()> deregister;
    }

    #! Creates the notifier
    /** @param sse the SSE stream to tear down when the peer disconnects
        @param closed_check returns @ref True when the transport reports that the
            stream can no longer carry data to the peer
        @param deregister removes this listener from the owning poll operation
    */
    constructor(AbstractSseConnection sse, code<bool()> closed_check,
            *code<nothing()> deregister) {
        self.sse = sse;
        self.closed_check = closed_check;
        self.deregister = deregister;
    }

    #! Called by the poll operation when something happened on the stream
    /** Ignores wakeups while the stream is still live; an SSE response stream
        never receives inbound data, so the only meaningful event is teardown.
    */
    nothing notifyIo() {
        if (!closed_check()) {
            return;
        }
        if (deregister) {
            deregister();
        }
        sse.notifyPeerClosed();
    }
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

    queueRawString(string data) {
        poll_op.queueRawString(data);
    }

    setHeartbeat(int interval_ms, string msg = DefaultSseHeartbeatMsg) {
        poll_op.setHeartbeat(interval_ms, msg);
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
        poll_op.close();
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
