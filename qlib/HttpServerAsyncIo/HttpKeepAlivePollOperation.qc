# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpKeepAlivePollOperation.qc Combined idle + header read poll operation

/** HttpKeepAlivePollOperation.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

#! Combined poll operation for idle keep-alive monitoring and HTTP header reading
/** This poll operation combines idle connection monitoring and HTTP header reading
    into a single poll operation, eliminating the need to remove and re-add the socket
    to the EventLoop when transitioning from idle to header reading.

    The operation has two phases:
    1. **Idle phase**: Monitors for incoming data (like HttpIdlePollOperation)
    2. **Header phase**: Reads HTTP headers (delegates to Socket.startPollReadHttpHeader())

    When data arrives during the idle phase, the operation transitions internally to
    header reading without returning to the controller. The operation only completes when:
    - Headers are fully read (success)
    - Idle timeout expires
    - Connection is closed by peer
    - An error occurs

    This is an HTTP/1.x-only optimization. HTTP/2 connections use Http2PollOperation
    which already stays in continuous read state.

*/
public class HttpKeepAlivePollOperation inherits HttpConnectionPollOperationBase {
    public {
        #! Phase constants
        const PHASE_IDLE = "idle";
        const PHASE_HEADER = "header";

        #! State constants
        const STATE_IDLE = "idle";
        const STATE_HEADER_READING = "header_reading";
        const STATE_HEADER_COMPLETE = "header_complete";
        const STATE_CLOSED = "closed";
        const STATE_TIMEOUT = "timeout";
    }

    private {
        #! The socket being monitored
        Socket sock;

        #! Current phase
        string phase = PHASE_IDLE;

        #! Current state
        string state = STATE_IDLE;

        #! Timeout deadline for idle phase
        date deadline;

        #! Timeout deadline for header read phase
        *date header_deadline;

        #! Keep-alive timeout duration
        timeout ttl;

        #! Delegate header-read poll operation (created on transition to header phase)
        *AbstractPollOperation header_op;

        #! Parsed header output (available after header phase completes)
        *hash<auto> header_output;
    }

    #! Creates the poll operation
    /** @param sock the socket to monitor
        @param ttl keep-alive timeout duration
    */
    constructor(Socket sock, timeout ttl = DefaultIdleTimeout) {
        self.sock = sock;
        self.ttl = ttl;
        self.deadline = now_us() + ttl;
    }

    #! Returns the goal of the operation
    string getGoal() {
        return phase == PHASE_IDLE ? "keep_alive_idle" : "keep_alive_header";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Returns True when the operation is complete
    bool goalReached() {
        return state == STATE_HEADER_COMPLETE || state == STATE_CLOSED || state == STATE_TIMEOUT;
    }

    #! Continues the poll operation
    *hash<SocketPollInfo> continuePoll() {
        if (goalReached()) {
            return NOTHING;
        }

        if (phase == PHASE_IDLE) {
            return continueIdlePoll();
        }
        return continueHeaderPoll();
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns the output of the operation
    auto getOutput() {
        return header_output ?? {
            "sock": sock,
            "state": state,
        };
    }

    #! Returns True if the idle phase timed out
    bool timedOut() {
        return state == STATE_TIMEOUT;
    }

    #! Returns True if headers were successfully read
    bool headersReady() {
        return state == STATE_HEADER_COMPLETE;
    }

    #! Returns True if the connection was closed
    bool wasClosed() {
        return state == STATE_CLOSED;
    }

    #! Returns the current phase
    string getPhase() {
        return phase;
    }

    #! Resets the operation for another idle cycle (persistent registration)
    resetForIdle() {
        phase = PHASE_IDLE;
        state = STATE_IDLE;
        deadline = now_us() + ttl;
        header_deadline = NOTHING;
        header_op = NOTHING;
        header_output = NOTHING;
    }

    #! Aborts the operation
    abort() {
        state = STATE_CLOSED;
        if (header_op) {
            header_op.abort();
        }
    }

    #! Idle phase polling
    private *hash<SocketPollInfo> continueIdlePoll() {
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
            # Transition to header reading phase
            return transitionToHeaderPhase();
        }

        # Need to wait for data; include poll_timeout_ms so the EventLoop wakes up
        # when the idle deadline expires, not just when the full operation timeout
        # (TTL + DefaultIoOperationTimeout) expires
        int remaining_ms = (deadline - now_us()).durationMilliseconds();
        if (remaining_ms <= 0) {
            remaining_ms = 1;
        }
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sock,
            "poll_timeout_ms": remaining_ms,
        };
    }

    #! Transitions from idle to header reading phase
    private *hash<SocketPollInfo> transitionToHeaderPhase() {
        phase = PHASE_HEADER;
        state = STATE_HEADER_READING;
        header_deadline = now_us() + DefaultHeaderReadTimeout;
        header_op = sock.startPollReadHttpHeader();
        # Immediately continue the header poll to drive it forward
        return continueHeaderPoll();
    }

    #! Header phase polling (delegates to Socket header read operation)
    private *hash<SocketPollInfo> continueHeaderPoll() {
        # Enforce header read timeout to prevent slow-loris on keep-alive connections
        if (header_deadline && now_us() > header_deadline) {
            state = STATE_TIMEOUT;
            return NOTHING;
        }
        *hash<SocketPollInfo> info = header_op.continuePoll();
        if (!info) {
            # Header reading complete
            state = STATE_HEADER_COMPLETE;
            header_output = header_op.getOutput();
            return NOTHING;
        }
        return info;
    }
}

} # namespace HttpServerAsyncIo
