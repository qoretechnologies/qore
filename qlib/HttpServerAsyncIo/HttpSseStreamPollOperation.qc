# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpSseStreamPollOperation.qc Poll operation for SSE streaming

/** HttpSseStreamPollOperation.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! Poll operation for Server-Sent Events streaming
/** This poll operation manages an SSE (Server-Sent Events) connection by:
    - Waiting for queued events with an EventNotifier
    - Providing a thread-safe queue for sending events
    - Sending queued events when the socket is writable

    SSE connections are persistent and unidirectional (server to client).
    The client receives events in the text/event-stream format.
*/
public class HttpSseStreamPollOperation inherits HttpConnectionPollOperationBase {
    public {
        #! State constants
        const STATE_STREAMING = "streaming";
        #! Producer signalled end-of-stream; drain queued events, then complete
        /** Set by @ref close().  In this state @ref continuePoll() keeps
            flushing the @ref event_channel and any partially-written
            @ref pending_data; once both are empty, the operation transitions
            to @ref STATE_DISCONNECTED and the controller closes the socket
            from the disconnect handler.  This avoids
            a race in the H1 response-header dispatch path where calling
            @ref abort() before the I/O controller has issued
            @c startPollSendHttpResponse would race ahead and leave the
            controller calling into a closed socket — see
            @ref close() for the full rationale.
        */
        const STATE_DRAINING = "draining";
        const STATE_DISCONNECTED = "disconnected";
        const STATE_ERROR = "error";

    }

    private {
        #! Protects poll-operation state shared by producer threads and controller workers
        Mutex m();

        #! The SSE socket
        Socket sock;

        #! Current state
        string state = STATE_STREAMING;

        #! Channel of events to send
        Channel event_channel(-1);

        #! Notifier signaled when events are queued
        EventNotifier notifier();

        #! Error info if state is ERROR
        *hash<ExceptionInfo> error_info;

        #! Current data being written (partial write support)
        *string pending_data;

        #! Active non-blocking send operation for pending_data
        *AbstractPollOperation send_op;

        #! Heartbeat interval in milliseconds; 0 disables the heartbeat
        int heartbeat_ms = DefaultSseHeartbeatIntervalMs;

        #! Heartbeat payload sent when the interval expires with no outbound traffic
        string heartbeat_msg = DefaultSseHeartbeatMsg;

        #! Epoch microseconds of the last successful send (or construction)
        int last_send_us = clock_getmicros();

        #! Optional callback retained for API compatibility
        /** SSE queue wakeups are driven by @ref notifier.  This callback is no
            longer needed for normal operation, but the setter remains so older
            callers can keep wiring it without changing behavior.
        */
        *code<nothing()> wake_callback;

        #! Callback run by the HTTP server when this SSE stream is disconnected
        *code<nothing()> close_callback;

        #! Maximum time producer/control threads may wait for the stream state lock
        const StateLockTimeoutMs = 5000;

        #! Protects the accounting of the queued data; never held while waiting for anything
        Mutex qm();

        #! Bytes queued but not yet written to the socket (the event being written counts until it is written)
        int queued_bytes = 0;

        #! The bytes of @ref pending_data counted in @ref queued_bytes (0 for a heartbeat)
        int pending_counted = 0;

        #! The bound of the queued data set with @ref setMaxBufferSize(), if any
        *int max_buffer_size;

        #! The callback that logs a warning when the client is dropped
        *code<nothing(string)> warning_callback;

        #! Set when the client was dropped because its queued data would have exceeded the bound
        bool overflowed = False;
    }

    #! Creates the poll operation
    /** @param sock the socket for the SSE connection (response already sent)
    */
    constructor(Socket sock) {
        self.sock = sock;
    }

    #! Configures the SSE heartbeat
    /** The async I/O controller wakes the operation via \c poll_timeout_ms when the
        interval elapses with no outbound traffic; the operation then sends
        \a msg (typically a \c ":hb\\n\\n" comment) to keep proxies and load
        balancers from dropping the idle connection.  Heartbeats are enabled
        by default; call this method with \a interval_ms set to \c 0 to opt out.

        @param interval_ms heartbeat interval in milliseconds; 0 disables the heartbeat
        @param msg raw bytes to send (must already be SSE-framed, e.g. \c ":hb\\n\\n")
    */
    setHeartbeat(int interval_ms, string msg = DefaultSseHeartbeatMsg) {
        lockState("setHeartbeat");
        on_exit m.unlock();
        heartbeat_ms = interval_ms;
        heartbeat_msg = msg;
    }

    #! Sets the wake callback used to notify the I/O controller after queuing data
    /** The HTTP server framework calls this after \c ServerSentEventHandler::setupAsyncSseConnection()
        returns.  Wakeups are now provided by the EventNotifier registered in
        @ref continuePoll(); this method is retained for source compatibility.

        @param cb the wake callback
    */
    setWakeCallback(*code<nothing()> cb) {
        lockState("setWakeCallback");
        on_exit m.unlock();
        wake_callback = cb;
    }

    #! Sets the callback invoked when the stream has ended
    /** The callback is called exactly once when the stream has ended for any reason: by the async I/O controller, on a
        thread of its handler pool, for a disconnect by the client, the end of a stream that the producer closed, a
        connection error, or the removal of the listener or a stop of the controller (see @ref takeCloseCallback());
        or, for a client dropped because its data would exceed its bound, on the thread that queued the data, before
        \c SSE-CLIENT-OVERFLOW is raised.
    */
    setCloseCallback(*code<nothing()> cb) {
        lockState("setCloseCallback");
        on_exit m.unlock();
        close_callback = cb;
    }

    #! Returns the callback invoked when the stream is disconnected
    *code<nothing()> getCloseCallback() {
        lockState("getCloseCallback");
        on_exit m.unlock();
        return close_callback;
    }

    #! Returns the callback invoked when the stream has ended and clears it, so that it is called at most once
    /** The async I/O controller takes the callback on every path that ends the stream: a disconnect, a connection
        error, and the removal of the listener or a stop of the controller.
    */
    *code<nothing()> takeCloseCallback() {
        lockState("takeCloseCallback");
        on_exit m.unlock();
        return remove close_callback;
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "sse_stream";
    }

    #! Returns the current state
    string getState() {
        lockState("getState");
        on_exit m.unlock();
        return state;
    }

    #! Returns True when the operation is complete (disconnected or error)
    /** @ref STATE_DRAINING is intentionally NOT terminal: the operation is
        still pulling from the event_channel and writing to the socket.
        Returning True here would cause the controller to stop polling us
        before we drain the queued events.
    */
    bool goalReached() {
        lockState("goalReached");
        on_exit m.unlock();
        return state == STATE_DISCONNECTED || state == STATE_ERROR;
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        AutoLock al(m);

        # @ref STATE_DRAINING is also live: the producer has signalled
        # end-of-stream but we still need to flush queued events.
        if (state != STATE_STREAMING && state != STATE_DRAINING) {
            return NOTHING;
        }

        # Check if socket is still open
        if (!sock.isOpen()) {
            state = STATE_DISCONNECTED;
            return NOTHING;
        }

        notifier.acknowledge();

        # A client-side close makes the H1 socket readable.  Monitor it as an
        # extra fd while the main wait remains on the EventNotifier, and use a
        # non-blocking idle probe to turn EOF/fatal read-side errors into prompt
        # SSE deregistration instead of waiting for a heartbeat or later write.
        if (checkPeerClosed()) {
            return NOTHING;
        }

        # Send queued data through Socket::startPollSend(), keeping all socket
        # I/O in the async poll state machine.
        while (True) {
            if (pending_data) {
                *hash<SocketPollInfo> info = continueSend();
                # Only a terminal state (a failed or aborted send sets
                # @ref STATE_DISCONNECTED / @ref STATE_ERROR) ends the operation
                # here.  @ref STATE_DRAINING is still live: we must finish this
                # send and then keep draining the @ref event_channel so a frame
                # queued behind the in-flight one (e.g. the SSE "data: [DONE]"
                # terminator) is delivered before close, not abandoned.
                if (state != STATE_STREAMING && state != STATE_DRAINING) {
                    return NOTHING;
                }
                if (info) {
                    return info;
                }
                continue;
            }

            auto result = event_channel.tryRecv();
            if (!exists result) {
                break;
            }

            # String values are pre-formatted; hash values need SSE formatting
            pending_data = result.value.typeCode() == NT_STRING
                ? result.value
                : formatSseEvent(result.value);
            pending_counted = pending_data.size();
        }

        # Graceful end-of-stream: producer signalled close() and we have
        # now flushed the queue and any partial pending_data.  Do not call
        # Socket::close() here: continuePoll() is still in-flight in the
        # async controller, and sync close/cancel would wait for this same
        # worker to return.  The controller closes the socket from
        # handleSseDisconnect() after this result is delivered.
        if (state == STATE_DRAINING) {
            state = STATE_DISCONNECTED;
            return NOTHING;
        }

        # No data to send — send a heartbeat inline if the interval has
        # elapsed, otherwise arm poll_timeout_ms so the controller wakes us
        # when the next heartbeat tick is due.  If no heartbeat is
        # configured, wait for the notifier to signal us from queueEvent()
        # / queueRaw*().
        if (heartbeat_ms > 0) {
            int elapsed_ms = (clock_getmicros() - last_send_us) / 1000;
            if (elapsed_ms >= heartbeat_ms) {
                pending_data = heartbeat_msg;
                *hash<SocketPollInfo> info = continueSend();
                if (state == STATE_DISCONNECTED || state == STATE_ERROR) {
                    return NOTHING;
                }
                if (info) {
                    return info;
                }
                return getIdlePollInfo(heartbeat_ms);
            }
            return getIdlePollInfo(heartbeat_ms - elapsed_ms);
        }
        return getIdlePollInfo();
    }

    #! Continues the active non-blocking send operation
    /** @return poll info when the socket must wait for writability, or NOTHING
        when pending_data was fully sent or the stream was disconnected
    */
    private *hash<SocketPollInfo> continueSend() {
        try {
            if (!send_op) {
                send_op = sock.startPollSend(pending_data);
            }

            *hash<SocketPollInfo> info = send_op.continuePoll();
            if (info) {
                return info;
            }
            if (!send_op.goalReached()) {
                state = STATE_DISCONNECTED;
                remove send_op;
                return NOTHING;
            }

            remove send_op;
            remove pending_data;
            if (pending_counted) {
                AutoLock al(qm);
                queued_bytes -= pending_counted;
                pending_counted = 0;
            }
            last_send_us = clock_getmicros();
            return NOTHING;
        } catch (hash<ExceptionInfo> ex) {
            state = STATE_DISCONNECTED;
            remove send_op;
            return NOTHING;
        }
    }

    #! Queues an SSE event for sending
    /** @param event_type the event type (optional)
        @param data the event data
        @param id the event ID (optional)
        @param retry retry interval in ms (optional)

        @throw SSE-EVENT-ERROR the event cannot be written; see @ref queueRawEvent()
        @throw SSE-CLIENT-OVERFLOW the event would take the queued data over its bound; see @ref queueRawString()
        @throw SSE-CLOSED the client was dropped because its queued data would have exceeded the bound
    */
    queueEvent(*string event_type, string data, *string id, *int retry) {
        queueRawEvent({
            "type": event_type,
            "data": data,
            "id": id,
            "retry": retry,
        });
    }

    #! Queues a raw SSE message
    /** The event is formatted when it is queued, so that an event that cannot be written raises an exception in the
        caller instead of failing in the I/O thread.

        @param event hash with optional keys: type, data, id, retry

        @throw SSE-EVENT-ERROR the event type or ID contains a NUL, CR or LF character, or the retry value is not a
        non-negative integer; see @ref HttpServerAsyncIo::SseEventFormat "SseEventFormat"
        @throw SSE-CLIENT-OVERFLOW the event would take the queued data over its bound; see @ref queueRawString()
        @throw SSE-CLOSED the client was dropped because its queued data would have exceeded the bound
    */
    queueRawEvent(hash<auto> event) {
        queueIntern(formatSseEvent(event));
    }

    #! Queues a pre-formatted SSE string for sending
    /** Never waits for the client.  The data queued but not yet written to the socket is bounded (see
        @ref setMaxBufferSize()); data that would take it over the bound, when it is not the only data queued, drops
        the client: a warning is logged through the warning callback, the connection is closed, the end of the
        stream is reported through the close callback, and \c SSE-CLIENT-OVERFLOW is raised.

        @param data already-formatted SSE text (e.g., from ServerSentEventHandler::encodeMessage())

        @throw SSE-CLIENT-OVERFLOW the data would take the queued data over its bound; it is not queued, and the
        client has been dropped
        @throw SSE-CLOSED the client was dropped because its queued data would have exceeded the bound
    */
    queueRawString(string data) {
        queueIntern(data);
    }

    #! Sets the bound of the data queued for the client but not yet written to the socket
    /** @param size the bound in bytes; must be > 0

        @throw SSE-OPTION-ERROR the size is not positive

        @see @ref HttpServerAsyncIo::DefaultSseMaxBufferSize "DefaultSseMaxBufferSize"
    */
    setMaxBufferSize(int size) {
        if (size <= 0) {
            throw "SSE-OPTION-ERROR", sprintf("the SSE buffer bound must be > 0; got %d", size);
        }
        AutoLock al(qm);
        max_buffer_size = size;
    }

    #! Returns the bound of the data queued for the client, in bytes
    /** @return the bound set with @ref setMaxBufferSize(), or
        @ref HttpServerAsyncIo::DefaultSseMaxBufferSize "DefaultSseMaxBufferSize" if none was set
    */
    int getMaxBufferSize() {
        AutoLock al(qm);
        return max_buffer_size ?? DefaultSseMaxBufferSize;
    }

    #! Returns @ref True if a bound was set with @ref setMaxBufferSize()
    bool hasMaxBufferSize() {
        AutoLock al(qm);
        return exists max_buffer_size;
    }

    #! Returns the number of bytes queued for the client but not yet written to the socket
    int getQueuedBytes() {
        AutoLock al(qm);
        return queued_bytes;
    }

    #! Sets the callback that logs a warning when the client is dropped
    /** @param cb the callback, called with the message; exceptions that it raises are ignored
    */
    setWarningCallback(*code<nothing(string)> cb) {
        AutoLock al(qm);
        warning_callback = cb;
    }

    #! Returns @ref True if a warning callback was set with @ref setWarningCallback()
    bool hasWarningCallback() {
        AutoLock al(qm);
        return exists warning_callback;
    }

    #! Queues data unless it would take the queued data over the bound, in which case the client is dropped
    private queueIntern(string data) {
        int len = data.size();
        *code<nothing(string)> cb;
        int limit;
        {
            AutoLock al(qm);
            if (overflowed) {
                throw "SSE-CLOSED", "SSE stream is closed: the client was dropped when its queued data would have "
                    "exceeded the bound";
            }
            limit = max_buffer_size ?? DefaultSseMaxBufferSize;
            if (!queued_bytes || (queued_bytes + len) <= limit) {
                queued_bytes += len;
                event_channel.send(data);
                notifier.notify();
                return;
            }
            overflowed = True;
            cb = warning_callback;
        }
        # a stream that has already ended (the client went away, or it was aborted) is not dropped for its bound: it
        # is closed; a draining stream (see close()) still has a client, which is dropped
        bool ended;
        try {
            string st = getState();
            ended = st == STATE_DISCONNECTED || st == STATE_ERROR;
        } catch (hash<ExceptionInfo> ex) {
            # the state lock could not be taken: the stream is dropped below
        }
        if (ended) {
            throw "SSE-CLOSED", "SSE stream is closed";
        }
        string msg = sprintf("http1 SSE stream: queueing %d bytes would take the data queued for the client over its "
            "bound of %d bytes; dropping the client", len, limit);
        if (cb) {
            try {
                cb(msg);
            } catch (hash<ExceptionInfo> ex) {
                # logging must not change the outcome
            }
        }
        # the end of the stream is reported here, on the thread that queued the data, before the stream is aborted: the
        # callback is taken first, so that the controller, which ends the operation once the socket is closed, finds
        # none, and the client has been dropped and deregistered when the exception is raised (a callback run by the
        # controller could still be running then)
        *code<nothing()> end_cb;
        try {
            end_cb = takeCloseCallback();
        } catch (hash<ExceptionInfo> ex) {
            # the state lock could not be taken: the controller reports the end
        }
        # the socket is closed even if the state lock cannot be taken, so that the client is dropped in any case
        try {
            abort();
        } catch (hash<ExceptionInfo> ex) {
            try {
                sock.close();
            } catch () {
            }
        }
        if (end_cb) {
            try {
                end_cb();
            } catch (hash<ExceptionInfo> ex) {
                # the end of the stream is reported; the exception raised is the overflow
            }
        }
        throw SseClientOverflowErr, msg;
    }

    private hash<SocketPollInfo> getIdlePollInfo(*int poll_timeout_ms) {
        hash<SocketPollInfo> rv = <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": notifier,
            "extra_fds": (
                <ExtraPollFdInfo>{
                    "pollable": sock,
                    "events": SOCK_POLLIN,
                },
            ),
        };
        if (exists poll_timeout_ms) {
            rv.poll_timeout_ms = poll_timeout_ms;
        }
        return rv;
    }

    private bool checkPeerClosed() {
        try {
            if (sock.checkIdleDataForAsyncPoll() != 0) {
                state = STATE_DISCONNECTED;
                return True;
            }
        } catch (hash<ExceptionInfo> ex) {
            state = STATE_DISCONNECTED;
            return True;
        }
        return False;
    }

    #! Formats an SSE event according to the spec
    private string formatSseEvent(hash<auto> event) {
        return SseEventFormat::formatEvent(event);
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns the output of the operation
    auto getOutput() {
        lockState("getOutput");
        on_exit m.unlock();
        return {
            "sock": sock,
            "state": state,
            "disconnected": state == STATE_DISCONNECTED,
            "error": state == STATE_ERROR,
            "error_info": error_info,
        };
    }

    #! Returns True if the client disconnected
    bool isDisconnected() {
        lockState("isDisconnected");
        on_exit m.unlock();
        return state == STATE_DISCONNECTED;
    }

    #! Returns the number of queued events
    int getQueueSize() {
        lockState("getQueueSize");
        on_exit m.unlock();
        return event_channel.size();
    }

    #! Initiates a graceful end-of-stream: drain queued events, then finish
    /** Use this when the SSE producer has finished emitting events and
        wants the stream to end cleanly.  Sets @ref state to
        @ref STATE_DRAINING and notifies @ref continuePoll(); the next
        poll cycle flushes any queued or partially-written data and
        only then finishes the poll operation so the controller can close the
        socket after the in-flight poll worker has returned.

        Why a separate method from @ref abort(): the H1 SSE handoff
        spawns a producer thread (see
        @c WebAppHandler::startSseDrainer in qorus) that begins pulling
        chunks immediately after @c setupAsyncSseConnection() returns —
        before the I/O controller has issued
        @c startPollSendHttpResponse() to write the response headers.
        For fast producers (qonsole's slash-form @c /list, for example),
        the producer can drain its queue and signal end-of-stream
        before the controller has even started the response.  Using
        @ref abort() in that path immediately closes the socket and
        the controller sees @c SOCKET-NOT-OPEN when it tries to write
        the headers.  @ref close() defers the socket close until
        @ref continuePoll() runs (which it cannot until the response
        headers are out and the SSE state machine is installed),
        eliminating that race.

        Idempotent against itself; @ref abort() called after
        @ref close() still wins (transitions to @ref STATE_DISCONNECTED
        and closes the socket immediately for error paths).

        @since %Qore 3.0
    */
    close() {
        lockState("close");
        on_exit m.unlock();
        if (state == STATE_STREAMING) {
            state = STATE_DRAINING;
            notifier.notify();
        }
    }

    #! Aborts the operation and closes the connection
    abort() {
        {
            lockState("abort");
            on_exit m.unlock();
            state = STATE_DISCONNECTED;
            notifier.notify();
        }
        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }

    private lockState(string caller) {
        if (m.lock(StateLockTimeoutMs)) {
            throw "SSE-STREAM-LOCK-TIMEOUT", sprintf(
                "timed out after %d ms waiting for SSE stream state lock in %s(); "
                "the async I/O controller worker may be blocked in continuePoll()",
                StateLockTimeoutMs, caller);
        }
    }
}

} # namespace HttpServerAsyncIo
