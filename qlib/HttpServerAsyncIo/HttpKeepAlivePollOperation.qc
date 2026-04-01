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
    2. **Header phase**: Reads HTTP headers (delegates to SocketReadHttpHeaderPollOperation)

    When data arrives during the idle phase, the operation transitions internally to
    header reading without returning to the controller. The operation only completes when:
    - Headers are fully read (success)
    - Idle timeout expires
    - Connection is closed by peer
    - An error occurs

    The core poll logic (continuePoll) runs in pure C++ via the
    HttpKeepAlivePollOperationBase parent class.

    This is an HTTP/1.x-only optimization. HTTP/2 connections use Http2PollOperation
    which already stays in continuous read state.

*/
public class HttpKeepAlivePollOperation inherits HttpKeepAlivePollOperationBase, HttpConnectionPollOperationBase {
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

    #! Creates the poll operation
    /** @param sock the socket to monitor
        @param ttl keep-alive timeout duration
    */
    constructor(Socket sock, timeout ttl = DefaultIdleTimeout) :
            HttpKeepAlivePollOperationBase(sock, int(ttl)) {
    }

    #! Override onComplete to fix MI method resolution
    /** Without this override, evalMethod("onComplete") resolves to the C++ no-op in
        AbstractPollOperation (via HttpKeepAlivePollOperationBase) instead of the Qore
        override in HttpConnectionPollOperationBase.
    */
    nothing onComplete(hash<SocketPollResultInfo> result) {
        HttpConnectionPollOperationBase::onComplete(result);
    }
}

} # namespace HttpServerAsyncIo
