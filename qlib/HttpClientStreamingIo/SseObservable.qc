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

    The read loop runs on the AsyncIoController worker pool, reading from the
    Queue populated by the I/O thread and notifying observers.

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

        #! I/O timeout for Queue reads
        timeout timeout_ms = 30s;

        #! Idle-timeout deadline; if no SSE traffic (events or heartbeat comments)
        #! arrives within this window, the stream is treated as dead and closed.
        #! Set \c 0 to disable idle-timeout detection.
        timeout idle_timeout_ms = 45s;

        #! If \c True, comment-only SSE messages (e.g. \c ":hb\\n\\n") are delivered to
        #! observers via @ref EVENT_SSE_HEARTBEAT; if \c False, they are silently
        #! consumed but still reset the idle-timeout deadline.
        bool emit_heartbeat = True;

        #! Epoch microseconds of the last SSE message received (data or heartbeat)
        int last_event_us = 0;

        #! Stop flag
        bool stop = False;

        #! Mutex for thread safety
        Mutex m();

        #! The stream handle (for cancellation)
        *HttpClientIo::HttpClientStreamHandle stream;

        #! The SSE event Queue (populated by SseAction on I/O thread)
        *Queue sse_queue;
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
        - \c idle_timeout: inactivity deadline; if no SSE traffic (events or
          heartbeat comments) arrives within this window, the stream is treated
          as dead and closed via @ref EVENT_SSE_CLOSED with
          \c reason = \c "idle-timeout". Set \c 0 to disable. Default \c 45s.
        - \c emit_heartbeat: if \c True (default), deliver comment-only SSE
          messages (e.g. \c ":hb\\n\\n") to observers via
          @ref EVENT_SSE_HEARTBEAT; if \c False, consume them silently while
          still using them to reset the idle-timeout deadline.
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
        if (exists self.opts.idle_timeout) {
            idle_timeout_ms = self.opts.idle_timeout;
        }
        if (exists self.opts.emit_heartbeat) {
            emit_heartbeat = self.opts.emit_heartbeat;
        }
    }

    #! Called when all observers have been added — starts the SSE stream
    /** Acquires a stream, calls startSseStreaming(), and dispatches the
        read loop to the AsyncIoController's worker pool.
    */
    observersReady() {
        {
            AutoLock al(m);
            stop = False;
        }

        stream = mgr.acquireStream(url);

        hash<auto> req_headers = {
            "Accept": "text/event-stream",
        } + (headers ?? {});

        sse_queue = stream.startSseStreaming(method, path, req_headers, body);

        notifyObservers(EVENT_SSE_CONNECTED, {});

        # Dispatch the read loop to the async I/O controller's worker pool
        AsyncSocketIo::AsyncSocketIoController controller =
            AsyncSocketIo::getGlobalAsyncIoController();
        controller.submitTask(sub () {
            readLoop();
        });
    }

    #! Stops the SSE event stream
    stopEvents() {
        {
            AutoLock al(m);
            stop = True;
        }
        if (stream) {
            stream.cancel();
        }
    }

    #! Returns True if the stream is stopped
    bool isStopped() {
        AutoLock al(m);
        return stop;
    }

    #! The read loop — runs on the worker pool thread
    /** Reads parsed SSE events from the Queue, optionally deserializes JSON,
        and notifies observers.

        @par Idle-timeout detection
        When \c idle_timeout_ms > 0, the loop tracks the wall-clock interval
        since the last received SSE message (data event or heartbeat comment).
        If the interval exceeds \c idle_timeout_ms, the stream is treated as
        dead and the loop returns via @ref EVENT_SSE_CLOSED with
        \c reason = \c "idle-timeout".
    */
    private readLoop() {
        hash<auto> close_info = {};
        on_exit {
            # Always cancel the underlying stream so the poll op completes
            # and its SseAction releases its queue ref.  Without this, a
            # loop exit on idle-timeout or NOTHING sentinel would leave the
            # poll op running with a dangling queue ref.
            if (stream) {
                try {
                    stream.cancel();
                } catch (hash<ExceptionInfo> ex) {
                    # Already cancelled or stream gone — fine
                }
            }
            if (self) {
                notifyObservers(EVENT_SSE_CLOSED, close_info);
            }
        }

        last_event_us = clock_getmicros();

        while (True) {
            if (!self) {
                return;
            }
            {
                AutoLock al(m);
                if (stop) {
                    return;
                }
            }

            # If an idle timeout is configured, bound the Queue wait so we can
            # detect dead streams even when no traffic arrives.
            timeout wait_ms = timeout_ms;
            if (idle_timeout_ms > 0) {
                int elapsed_ms = (clock_getmicros() - last_event_us) / 1000;
                int remaining_ms = idle_timeout_ms - elapsed_ms;
                if (remaining_ms <= 0) {
                    close_info = {"reason": "idle-timeout"};
                    return;
                }
                if (remaining_ms < wait_ms) {
                    wait_ms = remaining_ms;
                }
            }

            auto evt;
            try {
                evt = sse_queue.get(wait_ms);
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err == "QUEUE-TIMEOUT") {
                    # Loop; idle-timeout check at the top handles dead streams
                    continue;
                }
                # Any other Queue error terminates the read loop
                return;
            }

            # Reset idle deadline on any received event, including error hashes
            last_event_us = clock_getmicros();

            # NOTHING sentinel means stream ended
            if (!exists evt) {
                return;
            }

            if (evt.typeCode() != NT_HASH) {
                continue;
            }

            # Error hash from SseAction (plain hash<auto> with "err"/"desc" keys)
            # vs parsed SseMessageInfo hash — distinguish by hasKey("err")
            hash<auto> evt_hash = cast<hash<auto>>(evt);
            if (evt_hash.hasKey("err")) {
                # Error from SseAction — terminate the read loop
                return;
            }

            # Comment-only SSE message (heartbeat) — no data/event/id/retry
            if (evt_hash.comment.val() && !evt_hash.data.val() && !evt_hash.event.val()
                    && !evt_hash.id.val() && !evt_hash.retry.val()) {
                if (emit_heartbeat) {
                    if (!self) {
                        return;
                    }
                    notifyObservers(EVENT_SSE_HEARTBEAT, evt_hash);
                }
                continue;
            }

            # Parsed SseMessageInfo hash
            {
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
                    return;
                }
                notifyObservers(EVENT_SSE_DATA, evt_hash);
            }
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
