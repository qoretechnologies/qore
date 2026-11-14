# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpPersistentRequestEnd.qc Defines the HttpPersistentRequestEnd class

/** HttpPersistentRequestEnd.qc Copyright 2026 Qore Technologies, s.r.o.

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
#! Reports the end of the WebSocket connection or SSE stream that a request left open to the request's async target
/** The server creates one for a request whose WebSocket connection or SSE stream stays open after the request
    returns (its handler manages the connection from then on), and returns it in the request's result
    (@ref HttpRequestResultInfo::persistent_request_end); the connection's end calls @ref setEnded(), and the async
    I/O controller calls @ref requestReturned() once the target's @ref HttpAsyncTarget::onHttpRequest() "onHttpRequest()"
    has returned.  When both have been called, @ref HttpAsyncTarget::onPersistentRequestEnd() "onPersistentRequestEnd()"
    of the target is called, once, with the request's information.

    @note Used by the HttpServer and the async I/O controller; an application does not need to use this class
    itself, but an @ref HttpAsyncTarget that builds its own result for a request must keep the
    \c persistent_request_end of the result that the server returned
*/
public class HttpPersistentRequestEnd {
    private:internal {
        #! Protects the state below
        Mutex m();

        #! @ref True once the connection has ended
        bool ended = False;

        #! The target to notify and the request's information, set when the request has returned
        *HttpAsyncTarget target;
        *hash<HttpConnectionInfo> info;

        #! Runs the notification on a thread of the handler pool, set when the request has returned
        *code<nothing(code<nothing()>)> runner;

        #! @ref True once the notification has been made (or is being made)
        bool notified = False;
    }

    #! Reports that the connection or stream of the request has ended
    /** Can be called on any thread, also before the request has returned; only the first call has an effect.  The
        notification runs on a thread of the handler pool (see @ref HttpAsyncTarget::onPersistentRequestEnd()), so
        a caller can hold locks of its own.  It never raises an exception.
    */
    setEnded() {
        {
            m.lock();
            on_exit m.unlock();
            if (ended) {
                return;
            }
            ended = True;
            if (!runner) {
                # the request has not returned: requestReturned() notifies
                return;
            }
        }
        notify();
    }

    #! Reports that the request's @ref HttpAsyncTarget::onHttpRequest() "onHttpRequest()" has returned
    /** Called by the async I/O controller with the target that handled the request and the information it was
        given; only the first call has an effect.

        @param target the target that handled the request, which is notified
        @param info the information that the target was given for the request
        @param runner runs the given code on a thread of the handler pool with the dispatch context of the target,
        on a thread of its own if the pool does not take it, or in the calling thread if no thread can be started; if
        it raises an exception (it can run after the controller that made it has been deleted), the code runs on a
        thread of its own, or, if no thread can be started, in the calling thread
    */
    requestReturned(HttpAsyncTarget target, hash<HttpConnectionInfo> info,
            code<nothing(code<nothing()>)> runner) {
        {
            m.lock();
            on_exit m.unlock();
            if (self.runner) {
                return;
            }
            self.target = target;
            self.info = info;
            self.runner = runner;
            if (!ended) {
                # setEnded() notifies
                return;
            }
        }
        notify();
    }

    #! Returns @ref True if the notification has been made or is being made
    bool isNotified() {
        m.lock();
        on_exit m.unlock();
        return notified;
    }

    #! Makes the notification once; called without the lock held when the request has returned and ended
    private:internal notify() {
        HttpAsyncTarget t;
        hash<HttpConnectionInfo> i;
        code<nothing(code<nothing()>)> r;
        {
            m.lock();
            on_exit m.unlock();
            if (notified) {
                return;
            }
            notified = True;
            # the references are released, as the connection's end callbacks can keep this object
            t = remove target;
            i = remove info;
            r = remove runner;
        }
        # the notification does not refer to this object, which the caller can release before the notification runs
        code<nothing()> c = getNotification(t, i);
        try {
            r(c);
        } catch (hash<ExceptionInfo> ex) {
            # the runner failed (the async I/O controller has been deleted): the notification is still made, on a
            # thread of its own, or, if no thread can be started, in this thread
            try {
                background c();
            } catch () {
                c();
            }
        }
    }

    #! Returns the notification of the target; it never raises an exception
    /** Static, so that the closure refers only to the target and the information, not to the object that makes the
        notification
    */
    private:internal static code<nothing()> getNotification(HttpAsyncTarget t, hash<HttpConnectionInfo> i) {
        return sub () {
            try {
                t.onPersistentRequestEnd(i);
            } catch (hash<ExceptionInfo> ex) {
                # an exception raised by the target is ignored
                try {
                    if (*HttpServer::HttpListenerInterface listener = i.listener) {
                        listener.logError("onPersistentRequestEnd() of %s raised an exception: %s", t.className(),
                            get_exception_string(ex));
                    }
                } catch () {
                    # the listener has been deleted
                }
            }
        };
    }
}
}
