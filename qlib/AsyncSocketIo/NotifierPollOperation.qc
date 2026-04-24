# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file NotifierPollOperation.qc One-shot poll operation that waits for an EventNotifier to be signaled

/*  NotifierPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! The AsyncSocketIo namespace
/** All public declarations in the %AsyncSocketIo module are defined in this namespace
*/
public namespace AsyncSocketIo {

#! One-shot poll operation that completes when an EventNotifier becomes signaled
/** The @ref Qore::EventNotifier class inherits @ref Qore::AbstractPollableIoObject and
    exposes a file descriptor (eventfd on Linux, pipe on others) that the
    @ref Qore::AsyncIoController can monitor for @ref Qore::SOCK_POLLIN "SOCK_POLLIN"
    readability.  This class is the poll-operation glue: submit it to the controller
    along with the notifier, block on the returned @ref Qore::Thread::Queue "Queue",
    and the I/O thread drives the fd wait.

    After the operation completes, callers should invoke @ref Qore::EventNotifier::acknowledge()
    "acknowledge()" on the notifier to drain the fd (matching the behavior of the
    former @ref Qore::Socket::poll() "Socket::poll()"-based pattern).

    Typical usage via the convenience wrapper
    @ref AsyncSocketIo::AsyncSocketIoController::waitForNotifier()
    "AsyncSocketIoController::waitForNotifier()":

    @code{.py}
    if (controller.waitForNotifier(notifier, 5s)) {
        notifier.acknowledge();
        # ... check whatever state the signaler updated ...
    }
    @endcode

    @since AsyncSocketIo 1.3
*/
public class NotifierPollOperation inherits Qore::AbstractPollOperation {
    private {
        #! The notifier being waited on (referenced for the lifetime of the operation)
        EventNotifier notifier;

        #! True after the first continuePoll() has registered interest with the I/O thread
        bool started = False;

        #! True after the I/O thread has reported the notifier's fd readable
        bool signaled = False;
    }

    #! Creates a one-shot wait on the given notifier
    /** @param notifier the @ref Qore::EventNotifier "EventNotifier" to wait on
    */
    constructor(EventNotifier notifier) {
        self.notifier = notifier;
    }

    #! Goal label for this operation
    string getGoal() {
        return "notifier-signaled";
    }

    #! Current state label (\c "pending" → \c "waiting" → \c "signaled")
    string getState() {
        return signaled ? "signaled" : (started ? "waiting" : "pending");
    }

    #! Returns True once the notifier has been reported readable
    bool goalReached() {
        return signaled;
    }

    #! Drive the poll state machine
    /** On the first call, returns a @ref Qore::SocketPollInfo "SocketPollInfo" asking
        the controller to watch the notifier's fd for @ref Qore::SOCK_POLLIN "SOCK_POLLIN".
        On the second call — invoked by the controller after the fd becomes readable —
        the operation transitions to the signaled state and returns
        @ref NOTHING to report completion.
    */
    *hash<SocketPollInfo> continuePoll() {
        if (signaled) {
            return NOTHING;
        }
        if (!started) {
            started = True;
            return <SocketPollInfo>{
                "events": SOCK_POLLIN,
                "socket": notifier,
            };
        }
        # controller woke us → fd is readable → notifier has been signaled
        signaled = True;
        return NOTHING;
    }
}

} # namespace AsyncSocketIo
