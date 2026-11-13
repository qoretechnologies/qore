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

        #! Set once the client has been dropped for exceeding the bound (the warning is logged once)
        bool overflow_dropped = False;

        #! Set when @ref notifyPeerClosed() has already run the callback
        bool peer_close_fired = False;

        #! Protects the peer-close callback slot
        Mutex peer_close_mutex();

        #! The bound of the data held for the client set with @ref setMaxBufferSize(), if any
        *int max_buffer_size;

        #! The callback that logs a warning, set with @ref setWarningCallback()
        *code<nothing(string)> warning_callback;

        #! Callback run once when the stream has ended, whichever side ended it; see @ref setEndCallback()
        *code<nothing()> end_callback;

        #! Set when @ref notifyEnded() has already run the end callback
        bool end_fired = False;
    }

    #! Reports the end of the stream if it has not been reported (see @ref setEndCallback())
    /** A stream object that is destroyed can no longer be used, so its destruction is its end; this covers a stream
        whose last reference is released before its end was reported, such as while it is being closed
    */
    destructor() {
        notifyEnded();
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

        @param cb the callback; called at most once; if the stream has already ended (for example when its client
        was dropped while the connection was being set up), it is called at once on this thread

        @note the callback runs on the thread that reports the end of the stream: a worker thread for an end that
        the transport reports, or the thread that queued the event for a client dropped for exceeding its bound
    */
    setPeerCloseCallback(*code<nothing()> cb) {
        {
            AutoLock al(peer_close_mutex);
            if (!peer_close_fired) {
                peer_close_callback = cb;
                return;
            }
        }
        # the end reported before the callback was set is delivered now
        if (cb) {
            cb();
        }
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
            # the callback runs once, and is not kept: it can refer to objects that refer to this stream
            cb = remove peer_close_callback;
        }
        # close the stream first so any concurrent send fails fast instead of
        # writing into a dead transport
        try {
            close();
        } catch (hash<ExceptionInfo> ex) {
            # teardown path: the transport is already gone
        }
        # the end is reported also for a subclass whose close() does not report it, and before the callback, which
        # can release the last reference to this object
        notifyEnded();
        if (cb) {
            cb();
        }
    }

    #! Sets the callback run once when the stream has ended, whichever side ended it
    /** Unlike the peer-close callback (see @ref setPeerCloseCallback()), which reports only an end that the
        transport detects, this callback also reports a stream that the server ends itself with @ref close() or
        @ref abort(); the HTTP server uses it to report the end of the stream to the async target of the request
        that set it up (see @ref HttpAsyncTarget::onPersistentRequestEnd()).

        @param cb the callback; called at most once; if the stream has already ended, it is called at once on this
        thread

        @note the callback runs on the thread that ends the stream or reports its end, and can run with locks of that
        thread held; it must not run user code there
    */
    setEndCallback(*code<nothing()> cb) {
        {
            AutoLock al(peer_close_mutex);
            if (!end_fired) {
                end_callback = cb;
                return;
            }
        }
        # the end reported before the callback was set is delivered now
        if (cb) {
            cb();
        }
    }

    #! Runs the end callback exactly once; called when the stream has ended (see @ref setEndCallback())
    /** The transports call it from @ref close() and @ref abort(), and @ref notifyPeerClosed() and the destructor call
        it; repeated calls have no effect.  A subclass that implements a transport must call it at the start of its
        @ref close() and @ref abort() (before anything that can release the last reference to the object), or a
        stream that the server ends is reported only when its peer's close is detected or the object is destroyed.
    */
    private notifyEnded() {
        *code<nothing()> cb;
        {
            AutoLock al(peer_close_mutex);
            if (end_fired) {
                return;
            }
            end_fired = True;
            # the callback runs once, and is not kept: it can refer to objects that refer to this stream
            cb = remove end_callback;
        }
        if (cb) {
            # close(), abort() and the destructor never raise an exception because of the report of the end
            try {
                cb();
            } catch () {
            }
        }
    }

    #! Sets the bound of the data held for the client of the stream
    /** The data held for the client is the data queued but not yet written to the socket (HTTP/1.x), or buffered for
        the stream beyond what the client's flow-control window has accepted (HTTP/2 and HTTP/3).  Queueing an event
        that would take it over the bound, when it is not the only data held, drops the client and raises
        \c SSE-CLIENT-OVERFLOW; see @ref httpserverasyncio_sse_buffer.

        @param size the bound in bytes; must be > 0

        @throw SSE-OPTION-ERROR the size is not positive

        @see @ref HttpServerAsyncIo::DefaultSseMaxBufferSize "DefaultSseMaxBufferSize"
    */
    setMaxBufferSize(int size) {
        if (size <= 0) {
            throw "SSE-OPTION-ERROR", sprintf("the SSE buffer bound must be > 0; got %d", size);
        }
        applyMaxBufferSize(size);
        AutoLock al(peer_close_mutex);
        max_buffer_size = size;
    }

    #! Returns the bound of the data held for the client of the stream, in bytes
    /** @return the bound set with @ref setMaxBufferSize(), or
        @ref HttpServerAsyncIo::DefaultSseMaxBufferSize "DefaultSseMaxBufferSize" if none was set
    */
    int getMaxBufferSize() {
        AutoLock al(peer_close_mutex);
        return max_buffer_size ?? DefaultSseMaxBufferSize;
    }

    #! Returns @ref True if a bound was set with @ref setMaxBufferSize()
    bool hasMaxBufferSize() {
        AutoLock al(peer_close_mutex);
        return exists max_buffer_size;
    }

    #! Sets the callback that logs a warning when the client of the stream is dropped
    /** @param cb the callback, called with the message; exceptions that it raises are ignored
    */
    setWarningCallback(*code<nothing(string)> cb) {
        AutoLock al(peer_close_mutex);
        warning_callback = cb;
    }

    #! Returns @ref True if a warning callback was set with @ref setWarningCallback()
    bool hasWarningCallback() {
        AutoLock al(peer_close_mutex);
        return exists warning_callback;
    }

    #! Queues an SSE event for sending
    /** @param event_type the event type (optional)
        @param data the event data
        @param id the event ID (optional)
        @param retry retry interval in ms (optional)

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer; nothing is queued (see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat")
        @throw SSE-CLIENT-OVERFLOW the event would take the data held for the client over its bound (see
        @ref setMaxBufferSize()); the event is not queued, and the client has been dropped
    */
    abstract queueEvent(*string event_type, string data, *string id, *int retry);

    #! Queues a raw SSE message
    /** @param event hash with optional keys: type, data, id, retry

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer; nothing is queued (see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat")
        @throw SSE-CLIENT-OVERFLOW the event would take the data held for the client over its bound (see
        @ref setMaxBufferSize()); the event is not queued, and the client has been dropped
    */
    abstract queueRawEvent(hash<auto> event);

    #! Queues a pre-formatted SSE text frame for sending
    /** The string must already be formatted per the SSE spec (e.g., from
        \c ServerSentEventHandler::encodeMessage()).  Used by
        \c ServerSentEventConnection::sendEncoded() to deliver events across
        all HTTP protocol versions through a uniform write path.  It never waits for the client.

        @param data the pre-formatted SSE frame text

        @throw SSE-CLIENT-OVERFLOW the frame would take the data held for the client over its bound (see
        @ref setMaxBufferSize()); the frame is not queued, and the client has been dropped
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

    #! Applies the bound of the data held for the client to the transport; called by @ref setMaxBufferSize()
    /** The default implementation does nothing.
    */
    private applyMaxBufferSize(int size) {
    }

    #! Drops the client of a stream whose data would exceed its bound, and raises \c SSE-CLIENT-OVERFLOW
    /** Logs a warning through the warning callback, aborts the stream, and reports its end on the calling thread
        (the peer-close callback, which it claims before the abort, so that a notification that the abort causes on
        another thread does not report it first); used by the multiplexed transports, whose streams learn about their
        end only from an explicit notification.

        @param len the size of the data that could not be queued

        @throw SSE-CLIENT-OVERFLOW always
    */
    private dropOverflowingClient(int len) {
        string msg = sprintf("%s SSE stream: queueing %d bytes would take the data held for the client over its "
            "bound of %d bytes; dropping the client", getProtocol(), len, getMaxBufferSize());
        bool first;
        {
            AutoLock al(peer_close_mutex);
            first = !overflow_dropped;
            overflow_dropped = True;
        }
        # concurrent producers can all find the stream over its bound; the drop is logged once
        if (first) {
            warn(msg);
        }
        # the end of the stream is reported here, on the thread that queued the data: the callback is claimed before
        # the stream is aborted, so that a notification of the transport, which the abort can cause on another thread,
        # finds it claimed, and the client has been dropped and deregistered when the exception is raised
        *code<nothing()> cb;
        {
            AutoLock al(peer_close_mutex);
            if (!peer_close_fired) {
                peer_close_fired = True;
                cb = remove peer_close_callback;
            }
        }
        try {
            abort();
        } catch (hash<ExceptionInfo> ex) {
            # the stream has already ended
        }
        if (cb) {
            try {
                cb();
            } catch (hash<ExceptionInfo> ex) {
                # the end of the stream is reported; the exception raised is the overflow, and this one is logged
                warn(sprintf("%s SSE stream: the peer-close callback of the dropped client raised %s: %s",
                    getProtocol(), ex.err, ex.desc));
            }
        }
        throw SseClientOverflowErr, msg;
    }

    #! Logs a warning through the warning callback, if any
    private warn(string msg) {
        *code<nothing(string)> cb;
        {
            AutoLock al(peer_close_mutex);
            cb = warning_callback;
        }
        if (cb) {
            try {
                cb(msg);
            } catch (hash<ExceptionInfo> ex) {
                # logging must not change the outcome
            }
        }
    }
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

    #! Sets the bound of the data queued for the client; see @ref HttpSseStreamPollOperation::setMaxBufferSize()
    setMaxBufferSize(int size) {
        poll_op.setMaxBufferSize(size);
    }

    int getMaxBufferSize() {
        return poll_op.getMaxBufferSize();
    }

    bool hasMaxBufferSize() {
        return poll_op.hasMaxBufferSize();
    }

    setWarningCallback(*code<nothing(string)> cb) {
        poll_op.setWarningCallback(cb);
    }

    bool hasWarningCallback() {
        return poll_op.hasWarningCallback();
    }

    #! Returns the underlying poll operation for direct access if needed
    HttpSseStreamPollOperation getPollOperation() {
        return poll_op;
    }
}

} # namespace HttpServerAsyncIo
