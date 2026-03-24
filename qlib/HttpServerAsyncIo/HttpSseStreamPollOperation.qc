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
    - Monitoring for client disconnect (POLLIN for close/error detection)
    - Providing a thread-safe queue for sending events
    - Sending queued events when the socket is writable

    SSE connections are persistent and unidirectional (server to client).
    The client receives events in the text/event-stream format.

*/
public class HttpSseStreamPollOperation inherits AbstractPollOperation {
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

        #! Error info if state is ERROR
        *hash<ExceptionInfo> error_info;

        #! Current data being written (partial write support)
        *string pending_data;

        #! Bytes already written from pending_data
        int pending_offset = 0;

    }

    #! Creates the poll operation
    /** @param sock the socket for the SSE connection (response already sent)
    */
    constructor(Socket sock) {
        self.sock = sock;
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

        # Try to detect client disconnect by checking for readable data
        # (client shouldn't send data on SSE connection, so data = disconnect)
        try {
            if (sock.isDataAvailable(0ms)) {
                # Try to read - if we get 0 bytes or error, client disconnected
                *string data = sock.recv(1, 0ms);
                if (!data || !data.size()) {
                    state = STATE_DISCONNECTED;
                    return NOTHING;
                }
                # Unexpected data from client - ignore it
            }
        } catch (hash<ExceptionInfo> ex) {
            # Socket error means disconnect
            state = STATE_DISCONNECTED;
            return NOTHING;
        }

        # Send pending data using send2() which guarantees full delivery
        if (pending_data) {
            try {
                sock.send2(pending_data, DefaultSocketSendTimeout);
                remove pending_data;
                pending_offset = 0;
            } catch (hash<ExceptionInfo> ex) {
                state = STATE_DISCONNECTED;
                return NOTHING;
            }
        }

        # Check event channel for more data to send
        {
            auto result = event_channel.tryRecv();
            if (exists result) {
                # String values are pre-formatted; hash values need SSE formatting
                pending_data = result.value.typeCode() == NT_STRING
                    ? result.value
                    : formatSseEvent(result.value);
                pending_offset = 0;
                # Request write poll
                return <SocketPollInfo>{
                    "events": SOCK_POLLOUT | SOCK_POLLIN,
                    "socket": sock,
                };
            }
        }

        # No data to send - just monitor for disconnect
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sock,
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
    }

    #! Queues a raw SSE message
    /** @param event hash with optional keys: type, data, id, retry
    */
    queueRawEvent(hash<auto> event) {
        event_channel.send(event);
    }

    #! Queues a pre-formatted SSE string for sending
    /** @param data already-formatted SSE text (e.g., from ServerSentEventHandler::encodeMessage())
    */
    queueRawString(string data) {
        event_channel.send(data);
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
        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }
}

} # namespace HttpServerAsyncIo
