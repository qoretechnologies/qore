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

    The core poll logic (continuePoll) runs in pure C++ via the
    HttpIdlePollOperationBase parent class.

*/
public class HttpIdlePollOperation inherits HttpIdlePollOperationBase, HttpConnectionPollOperationBase {
    public {
        #! State constants
        const STATE_IDLE = "idle";
        const STATE_DATA_AVAILABLE = "data_available";
        const STATE_CLOSED = "closed";
        const STATE_TIMEOUT = "timeout";
    }

    #! Creates the poll operation
    /** @param sock the socket to monitor for new data
        @param ttl keep-alive timeout duration (default: DefaultIdleTimeout)
    */
    constructor(Socket sock, timeout ttl = DefaultIdleTimeout) :
            HttpIdlePollOperationBase(sock, int(ttl)) {
    }
}

} # namespace HttpServerAsyncIo
