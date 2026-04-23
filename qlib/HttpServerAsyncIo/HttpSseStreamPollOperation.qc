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
        const STATE_DISCONNECTED = "disconnected";
        const STATE_ERROR = "error";

        #! Default socket send timeout
        const DefaultSocketSendTimeout = 30s;
    }

    private {
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

        #! Bytes already written from pending_data
        /** @note Currently unused because \c send2() guarantees full delivery; retained for
            future partial-write support if non-blocking sends are introduced.
        */
        int pending_offset = 0;

        #! Heartbeat interval in milliseconds; 0 disables the heartbeat
        int heartbeat_ms = 0;

        #! Heartbeat payload sent when the interval expires with no outbound traffic
        string heartbeat_msg = ":hb\n\n";

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
        interval elapses with no outbound traffic; the operation then sends \a msg
        (typically a \c ":hb\\n\\n" comment) to keep proxies and load balancers from
        dropping the idle connection.

        @param interval_ms heartbeat interval in milliseconds; 0 disables the heartbeat
        @param msg raw bytes to send (must already be SSE-framed, e.g. \c ":hb\\n\\n")

        @since HttpServerAsyncIo 1.0.1
    */
    setHeartbeat(int interval_ms, string msg = ":hb\n\n") {
        heartbeat_ms = interval_ms;
        heartbeat_msg = msg;
    }

    #! Sets the wake callback used to notify the I/O controller after queuing data
    /** The HTTP server framework calls this after @ref ServerSentEventHandler::setupAsyncSseConnection()
        returns.  Wakeups are now provided by the EventNotifier registered in
        @ref continuePoll(); this method is retained for source compatibility.

        @param cb the wake callback

        @since %HttpServerAsyncIo 1.2
    */
    setWakeCallback(*code<nothing()> cb) {
        wake_callback = cb;
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "sse_stream";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when the operation is complete (disconnected or error)
    bool goalReached() {
        return state != STATE_STREAMING;
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        if (state != STATE_STREAMING) {
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
        # wakeups are enough to flush queued events; send2() reports a disconnect
        # when the next event is written.

        # Send queued data using send2(), which guarantees full delivery. Since
        # send2() blocks with a bounded timeout, flush data in this worker call
        # instead of depending on a follow-up POLLOUT wake to make progress.
        while (True) {
            if (pending_data) {
                try {
                    sock.send2(pending_data, DefaultSocketSendTimeout);
                    remove pending_data;
                    pending_offset = 0;
                    last_send_us = clock_getmicros();
                } catch (hash<ExceptionInfo> ex) {
                    state = STATE_DISCONNECTED;
                    return NOTHING;
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
            pending_offset = 0;
        }

        # No data to send — send a heartbeat inline if the interval has
        # elapsed, otherwise arm poll_timeout_ms so the controller wakes us
        # when the next heartbeat tick is due.  If no heartbeat is
        # configured, wait for the notifier to signal us from queueEvent()
        # / queueRaw*().
        if (heartbeat_ms > 0) {
            int elapsed_ms = (clock_getmicros() - last_send_us) / 1000;
            if (elapsed_ms >= heartbeat_ms) {
                try {
                    sock.send2(heartbeat_msg, DefaultSocketSendTimeout);
                    last_send_us = clock_getmicros();
                } catch (hash<ExceptionInfo> ex) {
                    state = STATE_DISCONNECTED;
                    return NOTHING;
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

    #! Queues an SSE event for sending
    /** @param event_type the event type (optional)
        @param data the event data
        @param id the event ID (optional)
        @param retry retry interval in ms (optional)
    */
    queueEvent(*string event_type, string data, *string id, *int retry) {
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
        event_channel.send(event);
        notifier.notify();
    }

    #! Queues a pre-formatted SSE string for sending
    /** @param data already-formatted SSE text (e.g., from ServerSentEventHandler::encodeMessage())
    */
    queueRawString(string data) {
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
        return state == STATE_DISCONNECTED;
    }

    #! Returns the number of queued events
    int getQueueSize() {
        return event_channel.size();
    }

    #! Aborts the operation and closes the connection
    abort() {
        state = STATE_DISCONNECTED;
        notifier.notify();
        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }
}

} # namespace HttpServerAsyncIo
