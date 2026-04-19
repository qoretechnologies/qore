# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SseObservable.qc SSE Observable using I/O-thread event parsing

/** SseObservable.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Main namespace for the HttpClientStreamingIo module
public namespace HttpClientStreamingIo {

#! SSE Observable using I/O-thread event parsing via SseAction
/** This class provides a push-based SSE event stream using the Observer pattern.
    SSE events are parsed on the I/O thread (via C++ SseAction) and delivered to
    observers as parsed SseMessageInfo hashes — zero thread hops for parsing.

    The EventNotifier is polled by the AsyncIoController; when it fires, the
    Queue populated by the I/O thread is drained without blocking and observers
    are notified.

    @par Example: Observing SSE events
    @code{.py}
%requires HttpClientIo
%requires HttpClientStreamingIo

HttpClientIo::HttpClientConnectionManager mgr();
SseObservable obs(mgr, "https://api.example.com", "GET", "/events");
obs.registerObserver(sub (string event_type, auto data) {
    if (event_type == EVENT_SSE_DATA) {
        printf("SSE event: %y\n", data);
    }
});
obs.observersReady();
# ... events delivered via observer callbacks ...
obs.stopEvents();
    @endcode

    @par Example: POST with JSON body
    @code{.py}
SseObservable obs(mgr, "https://api.example.com", "POST", "/stream",
    {"Content-Type": "application/json"},
    make_json({"prompt": "Hello"}),
    {"serialization": "json"});
obs.registerObserver(my_observer);
obs.observersReady();
    @endcode

    @since %HttpClientStreamingIo 1.0
*/

namespace Priv {

class SseNotifierPollOperation inherits Qore::AbstractPollOperation {
    private {
        EventNotifier notifier;
        code<bool()> drain;
        code<nothing()> done;
        bool complete = False;
    }

    constructor(EventNotifier notifier, code<bool()> drain, code<nothing()> done) {
        self.notifier = notifier;
        self.drain = drain;
        self.done = done;
    }

    string getGoal() {
        return "sse-events";
    }

    string getState() {
        return complete ? "complete" : "waiting";
    }

    bool goalReached() {
        return complete;
    }

    *hash<SocketPollInfo> continuePoll() {
        if (complete) {
            return NOTHING;
        }

        try {
            notifier.acknowledge();
            if (drain()) {
                complete = True;
                done();
                return NOTHING;
            }
        } catch (hash<ExceptionInfo> ex) {
            complete = True;
            done();
            rethrow;
        }

        return <SocketPollInfo>{
            "socket": notifier,
            "events": SOCK_POLLIN,
        };
    }

    abort() {
        if (!complete) {
            complete = True;
            done();
        }
    }

    onComplete(hash<SocketPollResultInfo> result) {
        if (!complete) {
            complete = True;
            done();
        }
    }
}

} # namespace Priv

public class SseObservable inherits DataProvider::DelayedObservable {
    private {
        #! Connection manager
        HttpClientIo::HttpClientConnectionManager mgr;

        #! SSE endpoint URL
        string url;

        #! HTTP method
        string method;

        #! Request path
        string path;

        #! Request headers
        *hash<auto> headers;

        #! Request body
        *data body;

        #! Options
        hash<auto> opts;

        #! Data serialization: "json" or "none"
        string serialization = "none";

        #! I/O timeout retained for compatibility with the original options
        timeout timeout_ms = 30s;

        #! Stop flag
        bool stop = False;

        #! Mutex for thread safety
        Mutex m();

        #! Signals notifier poll operation state changes
        Condition stop_cond();

        #! The stream handle (for cancellation)
        *HttpClientIo::HttpClientStreamHandle stream;

        #! The SSE event Queue (populated by SseAction on I/O thread)
        *Queue sse_queue;

        #! EventNotifier signaled when SSE queue entries are available
        *EventNotifier notifier;

        #! True while the notifier poll operation is active
        bool notifier_active = False;

        #! Notifier delivery worker thread ID
        *int tid;

        #! True after EVENT_SSE_CLOSED has been delivered
        bool closed = False;

        #! Async I/O controller key for the notifier poll operation
        *string poll_key;
    }

    #! Creates an SSE Observable
    /** @param mgr the HttpClientConnectionManager for connection pooling
        @param url the SSE endpoint URL
        @param method HTTP method (e.g., "GET", "POST")
        @param path request path
        @param headers request headers (optional)
        @param body request body (optional)
        @param opts options:
        - \c serialization: "json" or "none" (default: "none") — deserialize event data
        - \c timeout: I/O timeout for reading events (default: 30s)
    */
    constructor(HttpClientIo::HttpClientConnectionManager mgr, string url,
            string method, string path, *hash<auto> headers, *data body,
            *hash<auto> opts) {
        self.mgr = mgr;
        self.url = url;
        self.method = method;
        self.path = path;
        self.headers = headers;
        self.body = body;
        self.opts = opts ?? {};
        if (self.opts.serialization) {
            serialization = self.opts.serialization;
        }
        if (self.opts.timeout) {
            timeout_ms = self.opts.timeout;
        }
    }

    #! Called when all observers have been added — starts the SSE stream
    /** Acquires a stream, calls startSseStreamingWithNotifier(), and submits
        the notifier poll operation to the AsyncIoController.
    */
    observersReady() {
        {
            AutoLock al(m);
            stop = False;
            closed = False;
        }

        stream = mgr.acquireStream(url);

        hash<auto> req_headers = {
            "Accept": "text/event-stream",
        } + (headers ?? {});

        hash<auto> sse_info = stream.startSseStreamingWithNotifier(method, path, req_headers, body);
        sse_queue = sse_info.queue;
        notifier = sse_info.notifier;

        notifyObservers(EVENT_SSE_CONNECTED, {});

        bool stopped_reentrantly;
        {
            AutoLock al(m);
            stopped_reentrantly = stop;
        }
        if (stopped_reentrantly) {
            # stopEvents() was called reentrantly from the connected observer
            notifyClosed();
            return;
        }

        {
            AutoLock al(m);
            notifier_active = True;
            remove tid;
            poll_key = sprintf("HttpClientStreamingIo::SseObservable:%s", self.uniqueHash());
        }

        try {
            AsyncSocketIo::AsyncSocketIoController controller =
                AsyncSocketIo::getGlobalAsyncIoController();
            Priv::SseNotifierPollOperation poll_op(notifier, \drainNotifierEvents(), \notifierDone());
            controller.submit(<SocketPollOperationInfo>{
                "sock": notifier,
                "spop": poll_op,
                "owner": self.uniqueHash(),
                "key": poll_key,
                "has_on_complete": True,
            }, True);
        } catch (hash<ExceptionInfo> ex) {
            {
                AutoLock al(m);
                notifier_active = False;
                stop_cond.broadcast();
            }
            if (stream) {
                stream.cancel();
            }
            notifyClosed();
            rethrow;
        }
    }

    #! Stops the SSE event stream
    stopEvents() {
        *HttpClientIo::HttpClientStreamHandle local_stream;
        *Queue local_sse_queue;
        *EventNotifier local_notifier;
        bool wait_for_notifier;
        bool wake_notifier;
        bool notify_without_wait;

        {
            AutoLock al(m);
            stop = True;
            local_stream = stream;
            local_sse_queue = sse_queue;
            local_notifier = notifier;
        }
        if (local_stream) {
            local_stream.cancel();
        }

        {
            AutoLock al(m);
            while (tid && tid != gettid()) {
                stop_cond.wait(m);
            }
            wait_for_notifier = notifier_active && (!tid || tid != gettid());
            wake_notifier = notifier_active && !tid;
            notify_without_wait = !notifier_active && local_stream;
        }
        if (wake_notifier && local_sse_queue) {
            try {
                local_sse_queue.push();
            } catch (hash<ExceptionInfo> ex) {
            }
        }
        if (wake_notifier && local_notifier) {
            local_notifier.notify();
        }

        if (wait_for_notifier) {
            AutoLock al(m);
            while (notifier_active) {
                stop_cond.wait(m);
            }
        } else if (notify_without_wait) {
            notifyClosed();
        }
    }

    #! Returns True if the stream is stopped
    bool isStopped() {
        AutoLock al(m);
        return stop;
    }

    #! Drains queued SSE events after notifier wakeup
    /** Reads parsed SSE events from the Queue without blocking, optionally
        deserializes JSON, and notifies observers.
    */
    private bool drainNotifierEvents() {
        {
            AutoLock al(m);
            tid = gettid();
        }
        on_exit {
            if (self) {
                AutoLock al(m);
                remove tid;
                stop_cond.broadcast();
            }
        }
        on_error {
            notifyClosed();
        }

        while (True) {
            bool stopped;
            {
                AutoLock al(m);
                stopped = stop;
            }
            if (stopped) {
                notifyClosed();
                return True;
            }

            auto item = sse_queue.tryGet();
            if (!exists item) {
                return False;
            }

            auto evt = item.value;

            # NOTHING sentinel means stream ended
            if (!exists evt) {
                notifyClosed();
                return True;
            }

            if (evt.typeCode() != NT_HASH) {
                continue;
            }

            # Error hash from SseAction (plain hash<auto> with "err"/"desc" keys)
            # vs parsed SseMessageInfo hash — distinguish by hasKey("err")
            hash<auto> evt_hash = cast<hash<auto>>(evt);
            if (evt_hash.hasKey("err")) {
                notifyClosed();
                return True;
            }

            # Parsed SseMessageInfo hash
            try {
                if (evt_hash.data.val()) {
                    evt_hash.data = deserializeData(evt_hash.data);
                }
                if (evt_hash.id =~ /^[0-9]+$/) {
                    evt_hash.id = evt_hash.id.toInt();
                }
            } catch (hash<ExceptionInfo> ex) {
                continue;
            }

            if (!self) {
                return True;
            }
            notifyObservers(EVENT_SSE_DATA, evt_hash);
        }
    }

    private notifyClosed() {
        bool do_notify = False;
        {
            AutoLock al(m);
            if (!closed) {
                closed = True;
                do_notify = True;
            }
        }
        if (do_notify) {
            notifyObservers(EVENT_SSE_CLOSED, {});
        }
    }

    private notifierDone() {
        {
            AutoLock al(m);
            notifier_active = False;
            remove tid;
            remove stream;
            remove sse_queue;
            remove notifier;
            remove poll_key;
            stop_cond.broadcast();
        }
    }

    #! Deserializes SSE event data based on the serialization setting
    private auto deserializeData(string buf) {
        if (serialization == "json") {
            return parse_json(buf);
        }
        return buf;
    }
}

} # namespace HttpClientStreamingIo
