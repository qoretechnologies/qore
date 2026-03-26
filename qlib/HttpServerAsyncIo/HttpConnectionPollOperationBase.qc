# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpConnectionPollOperationBase.qc common base for HTTP server connection poll operations

/*  HttpConnectionPollOperationBase.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace HttpServerAsyncIo;

#! Common base class for HTTP server connection poll operations
/** Provides onComplete() handling that calls handleConnectionResult() on the
    HttpAsyncSocketIoController. All HTTP server connection poll operations
    (Http2PollOperation, HttpIdlePollOperation, HttpKeepAlivePollOperation,
    HttpSseStreamPollOperation, HttpWebSocketPollOperation) inherit from this.

    @since Qore 2.3
*/
public class HttpServerAsyncIo::HttpConnectionPollOperationBase inherits AbstractPollOperation {
    private {
        #! Controller for completion dispatch
        *HttpAsyncSocketIoController completion_controller;

        #! Connection unique hash for result correlation
        *string completion_uh;

        #! Connection state when the operation was submitted
        *string completion_state;
    }

    #! Sets the completion context for onComplete() dispatch
    /** Called by HttpAsyncSocketIoController before submitting the operation.
        @param controller the controller to call handleConnectionResult() on
        @param uh the connection unique hash
        @param state the connection state when submitted
    */
    setCompletionContext(HttpAsyncSocketIoController controller, string uh, string state) {
        completion_controller = controller;
        completion_uh = uh;
        completion_state = state;
    }

    #! Called when the operation completes; dispatches result to the controller
    nothing onComplete(hash<SocketPollResultInfo> result) {
        if (completion_controller) {
            completion_controller.handleConnectionResult(result, completion_uh, completion_state);
        }
    }
}
