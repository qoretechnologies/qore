# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file CompletionPollProxy.qc proxy that delegates AbstractPollOperation methods and adds onComplete()

/*  CompletionPollProxy.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace AsyncSocketIo;

#! Proxy that wraps any AbstractPollOperation and adds completion handling via onComplete()
/** Use this when you cannot modify the poll operation class directly (e.g., operations
    returned by Socket::startPollConnect() or connection::startPollPing()).

    The proxy delegates all AbstractPollOperation methods to the inner operation and
    overrides onComplete() to call a stored completion handler.

    @since Qore 2.3
*/
public class AsyncSocketIo::CompletionPollProxy inherits AbstractPollOperation {
    private {
        #! The wrapped poll operation
        AbstractPollOperation inner;

        #! Completion handler called from onComplete()
        code<nothing(hash<SocketPollResultInfo>)> completion_handler;
    }

    #! Creates a proxy wrapping the given poll operation with a completion handler
    /** @param inner the poll operation to wrap
        @param completion_handler called from onComplete() with the operation result
    */
    constructor(AbstractPollOperation inner,
            code<nothing(hash<SocketPollResultInfo>)> completion_handler) {
        self.inner = inner;
        self.completion_handler = completion_handler;
    }

    string getGoal() {
        return inner.getGoal();
    }

    string getState() {
        return inner.getState();
    }

    bool goalReached() {
        return inner.goalReached();
    }

    *hash<SocketPollInfo> continuePoll() {
        return inner.continuePoll();
    }

    nothing abort() {
        inner.abort();
    }

    auto getOutput() {
        return inner.getOutput();
    }

    nothing onComplete(hash<SocketPollResultInfo> result) {
        completion_handler(result);
    }
}
