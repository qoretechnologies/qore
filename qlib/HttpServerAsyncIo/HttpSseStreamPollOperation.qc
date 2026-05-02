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
        AutoLock al(m);
        heartbeat_ms = interval_ms;
        heartbeat_msg = msg;
    }

    #! Sets the wake callback used to notify the I/O controller after queuing data
    /** The HTTP server framework calls this after @ref ServerSentEventHandler::setupAsyncSseConnection()
        returns.  Wakeups are now provided by the EventNotifier registered in
        @ref continuePoll(); this method is retained for source compatibility.

        @param cb the wake callback
    */
    setWakeCallback(*code<nothing()> cb) {
        AutoLock al(m);
        wake_callback = cb;
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "sse_stream";
    }

    #! Returns the current state
    string getState() {
        AutoLock al(m);
        return state;
    }

    #! Returns True when the operation is complete (disconnected or error)
    /** @ref STATE_DRAINING is intentionally NOT terminal: the operation is
        still pulling from the event_channel and writing to the socket.
        Returning True here would cause the controller to stop polling us
        before we drain the queued events.
    */
    bool goalReached() {
        AutoLock al(m);
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

        # Do not register the client socket for read readiness while an H1 SSE
        # stream is idle.  A closed peer makes the fd permanently readable, and
        # this Qore poll operation does not consume from it.  EventNotifier
        # wakeups are enough to flush queued events; a non-blocking send detects
        # disconnects when the next event is written.

        # Send queued data through Socket::startPollSend(), keeping all socket
        # I/O in the async poll state machine.
        while (True) {
            if (pending_data) {
                *hash<SocketPollInfo> info = continueSend();
                if (state != STATE_STREAMING) {
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
                if (state != STATE_STREAMING) {
                    return NOTHING;
                }
                if (info) {
                    return info;
                }
                return <SocketPollInfo>{
                    "events": SOCK_POLLIN,
                    "socket": notifier,
                    "poll_timeout_ms": heartbeat_ms,
                };
            }
            return <SocketPollInfo>{
                "events": SOCK_POLLIN,
                "socket": notifier,
                "poll_timeout_ms": heartbeat_ms - elapsed_ms,
            };
        }
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": notifier,
        };
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
    */
    queueEvent(*string event_type, string data, *string id, *int retry) {
        AutoLock al(m);
        event_channel.send({
            "type": event_type,
            "data": data,
            "id": id,
            "retry": retry,
        });
        notifier.notify();
    }

    #! Queues a raw SSE message
    /** @param event hash with optional keys: type, data, id, retry
    */
    queueRawEvent(hash<auto> event) {
        AutoLock al(m);
        event_channel.send(event);
        notifier.notify();
    }

    #! Queues a pre-formatted SSE string for sending
    /** @param data already-formatted SSE text (e.g., from ServerSentEventHandler::encodeMessage())
    */
    queueRawString(string data) {
        AutoLock al(m);
        event_channel.send(data);
        notifier.notify();
    }

    #! Formats an SSE event according to the spec
    private string formatSseEvent(hash<auto> event) {
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

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns the output of the operation
    auto getOutput() {
        AutoLock al(m);
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
        AutoLock al(m);
        return state == STATE_DISCONNECTED;
    }

    #! Returns the number of queued events
    int getQueueSize() {
        AutoLock al(m);
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

        @since %Qore 2.4
    */
    close() {
        AutoLock al(m);
        if (state == STATE_STREAMING) {
            state = STATE_DRAINING;
            notifier.notify();
        }
    }

    #! Aborts the operation and closes the connection
    abort() {
        {
            AutoLock al(m);
            state = STATE_DISCONNECTED;
            notifier.notify();
        }
        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }
}

} # namespace HttpServerAsyncIo
