# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpIdlePollOperation.qc Poll operation for idle keep-alive connections

/** HttpIdlePollOperation.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! Poll operation for idle keep-alive connections
/** This poll operation monitors an idle HTTP connection for:
    - New data arriving (indicating a new request)
    - Connection close by client
    - Timeout expiration

    When data arrives, the operation completes with state "data_available"
    and the connection should transition to header reading.

*/
public class HttpIdlePollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_IDLE = "idle";
        const STATE_DATA_AVAILABLE = "data_available";
        const STATE_CLOSED = "closed";
        const STATE_TIMEOUT = "timeout";
    }

    private {
        #! The socket being monitored
        Socket sock;

        #! Current state
        string state = STATE_IDLE;

        #! Timeout deadline
        date deadline;

        #! Keep-alive timeout duration
        timeout ttl;
    }

    #! Creates the poll operation
    /** @param sock the socket to monitor for new data
        @param ttl keep-alive timeout duration (default: DefaultIdleTimeout)
    */
    constructor(Socket sock, timeout ttl = DefaultIdleTimeout) {
        self.sock = sock;
        self.ttl = ttl;
        self.deadline = now_us() + ttl;
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "idle_monitor";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when the operation is complete
    bool goalReached() {
        return state != STATE_IDLE;
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        if (state != STATE_IDLE) {
            return NOTHING;
        }

        # Check for timeout
        if (now_us() > deadline) {
            state = STATE_TIMEOUT;
            return NOTHING;
        }

        # Check if socket is still connected
        if (!sock.isOpen()) {
            state = STATE_CLOSED;
            return NOTHING;
        }

        # Check for data immediately available (non-blocking)
        if (sock.isDataAvailable(0ms)) {
            state = STATE_DATA_AVAILABLE;
            return NOTHING;
        }

        # Need to wait for data - return poll info
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sock,
        };
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
            "timed_out": state == STATE_TIMEOUT,
            "closed": state == STATE_CLOSED,
            "data_available": state == STATE_DATA_AVAILABLE,
        };
    }

    #! Returns True if the connection timed out
    bool timedOut() {
        return state == STATE_TIMEOUT;
    }

    #! Returns True if data is available for reading
    bool hasData() {
        return state == STATE_DATA_AVAILABLE;
    }

    #! Returns True if the connection was closed
    bool wasClosed() {
        return state == STATE_CLOSED;
    }

    #! Resets the timeout deadline
    resetTimeout() {
        deadline = now_us() + ttl;
    }

    #! Aborts the operation
    abort() {
        state = STATE_CLOSED;
    }
}

} # namespace HttpServerAsyncIo
