# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClientIo.qc SSE client using HttpClientIo for async I/O

/*  ServerSentEventClientIo.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! the ServerSentEventClient namespace contains all the definitions in the ServerSentEventClient module
public namespace ServerSentEventClient {

#! SSE client using HttpClientIo for async I/O across HTTP/1.x, HTTP/2, and HTTP/3
/** This class provides the same Observable-based API as @ref ServerSentEventClient but uses
    @ref HttpClientIo::HttpClientConnectionManager for async I/O and
    @ref HttpClientStreamingIo::SseStreamReader for SSE event parsing.

    @par Example:
    @code{.py}
ServerSentEventClientIo sse({
    "url": "https://example.com/events",
});
sse.registerObserver(sub (string event_type, auto data) {
    if (event_type == EVENT_SSE_DATA) {
        printf("SSE event: %y\n", data);
    }
});
sse.connect();
# ... wait for events ...
sse.disconnect();
    @endcode

    @note The event loop runs in a background thread.  Event callbacks are invoked from
    that thread.  This class is thread-safe for connect/disconnect/reconnect calls.
*/
public class ServerSentEventClientIo inherits Logger::LoggerWrapper, ConnectionProvider::OptionHelper,
        DataProvider::DelayedObservable {

    private {
        Mutex m();
        Condition stop_cond();

        #! Connection manager
        HttpClientIo::HttpClientConnectionManager mgr;

        #! URL for SSE connection
        string url;

        #! Request path extracted from the URL
        string url_path;

        #! Additional headers for the SSE request
        *hash<auto> connect_headers;

        #! I/O timeout for SSE event reading
        timeout timeout_ms = ServerSentEventClient::DefaultTimeout;

        #! Stop flag
        bool stop = False;

        #! TID of the event thread
        *int tid;

        #! Reconnect flag
        bool reconnect_flag = False;

        #! Reconnect delay
        timeout reconnect_delay = ServerSentEventClient::ReconnectDelay;

        #! Max reconnect retries; negative = infinite
        int max_reconnect_retries = ServerSentEventClient::MaxReconnectRetries;

        #! Number of connections made
        int num_connects = 0;

        #! Data serialization: "json" or "none"
        string serialization = "none";

        #! Last event ID for reconnection
        *string last_event_id;

        #! Callable to yield thread execution (for sandboxed environments)
        *code<nothing()> yield;

        #! Callable to start a new thread (for sandboxed environments)
        *code<int(code<nothing()>)> start_thread;
    }

    #! Creates the SSE client
    /** @param opts constructor options:
        - \c url: (required) the SSE endpoint URL
        - \c connect_timeout: connection timeout (default: 30s)
        - \c timeout: I/O timeout for SSE event reading (default: 15s)
        - \c reconnect: if True, auto-reconnect on connection loss (default: False)
        - \c reconnect_delay: delay between reconnect attempts (default: 2s)
        - \c max_reconnect_retries: max retries; negative = infinite (default: -1)
        - \c serialization: "json" or "none" (default: "none")
        - \c yield: callable to yield thread execution (for sandboxed environments)
        - \c start_thread: callable to start a new thread (for sandboxed environments)
    */
    constructor(hash<auto> opts) {
        url = getString(opts, "url");
        url_path = parse_url(url).path ?? "/";
        if (!url.val()) {
            throw "SSE-ERROR", "missing 'url' option";
        }
        # Normalize sse:// and sses:// schemes
        url =~ s/^sse:/http:/;
        url =~ s/^sses:/https:/;

        timeout connect_timeout = opts.connect_timeout ?? 30s;
        if (opts.timeout) {
            timeout_ms = opts.timeout;
        }
        if (opts.reconnect) {
            reconnect_flag = True;
        }
        if (exists opts.reconnect_delay) {
            reconnect_delay = opts.reconnect_delay;
        }
        if (exists opts.max_reconnect_retries) {
            max_reconnect_retries = opts.max_reconnect_retries;
        }
        if (opts.serialization) {
            serialization = opts.serialization;
            if (!ServerSentEventClient::DataSerializationSupport{serialization}) {
                throw "SSE-ERROR", sprintf("unsupported serialization %y; valid values: %y",
                    serialization, keys ServerSentEventClient::DataSerializationSupport);
            }
        }
        yield = opts.yield;
        start_thread = opts.start_thread;

        # Use "auto" for HTTPS (H2 via ALPN), "h1" for plain HTTP
        string protocol = url =~ /^https/i ? "auto" : "h1";
        mgr = new HttpClientIo::HttpClientConnectionManager(
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "connect_timeout": connect_timeout,
                "request_timeout": timeout_ms,
                "protocol": protocol,
            });
    }

    #! Destroys the client
    destructor() {
        disconnect();
        mgr.closeAll();
    }

    #! Connects to the SSE endpoint and starts the event loop in a background thread
    /** If already connected, the existing connection is stopped before starting a new one.

        @param opts optional connection options:
        - \c hdr: additional headers for the SSE request
    */
    connect(*hash<auto> opts) {
        {
            AutoLock al(m);
            if (tid) {
                stop = True;
                stop_cond.broadcast();
                while (tid) {
                    stop_cond.wait(m);
                }
            }
            stop = False;
        }

        if (opts.hdr) {
            connect_headers = opts.hdr;
        }

        int new_tid;
%ifdef PO_NO_THREAD_CONTROL
        new_tid = start_thread(\eventLoop());
%else
        new_tid = start_thread ? start_thread(\eventLoop()) : background eventLoop();
%endif

        # Set tid under lock so disconnect() can find the thread immediately
        {
            AutoLock al(m);
            tid = new_tid;
            ++num_connects;
        }

        self.logDebug("SSE I/O thread started: TID %d", new_tid);
        notifyObservers(EVENT_SSE_CONNECTED, {});
    }

    #! Disconnects from the SSE endpoint and stops the event loop
    /** Blocks until the background event thread terminates.  Clears the reconnect flag
        to prevent automatic reconnection.
    */
    disconnect() {
        AutoLock al(m);
        reconnect_flag = False;
        if (tid) {
            stop = True;
            stop_cond.broadcast();
            while (tid) {
                stop_cond.wait(m);
            }
        }
    }

    #! Returns True if the event loop is running
    /** @return \c True if the background event thread is active
    */
    bool isOpen() {
        AutoLock al(m);
        return exists tid;
    }

    #! Returns the reconnect flag
    /** @return \c True if auto-reconnection is enabled
    */
    bool getReconnect() {
        return reconnect_flag;
    }

    #! Returns the SSE endpoint URL
    /** @return the URL passed to the constructor (with \c sse:// normalized to \c http://)
    */
    string getUrl() {
        return url;
    }

    #! Background event loop using HttpClientIo + SseStreamReader
    private eventLoop() {
        on_exit {
            if (self) {
                AutoLock al(m);
                remove tid;
                stop = False;
                stop_cond.broadcast();
                self.logDebug("SSE I/O thread TID %d terminating", gettid());
            }
        }

        int reconnect_attempts = 0;

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

            try {
                reconnect_attempts = 0;
                runSseStream();
            } catch (hash<ExceptionInfo> ex) {
                if (!self) {
                    return;
                }
                self.logError("SSE stream error: %s: %s", ex.err, ex.desc);
            }

            if (!self) {
                return;
            }

            notifyObservers(EVENT_SSE_CLOSED, {});

            {
                AutoLock al(m);
                if (stop || !reconnect_flag) {
                    return;
                }
            }

            # Check retry limit (negative = infinite)
            ++reconnect_attempts;
            if (max_reconnect_retries >= 0 && reconnect_attempts > max_reconnect_retries) {
                self.logInfo("SSE max reconnect retries (%d) exceeded; giving up", max_reconnect_retries);
                return;
            }

            # Reconnect with delay
            self.logInfo("SSE connection lost; reconnecting in %y (attempt %d/%s)",
                reconnect_delay, reconnect_attempts,
                max_reconnect_retries < 0 ? "inf" : string(max_reconnect_retries));
            {
                AutoLock al(m);
                stop_cond.wait(m, reconnect_delay);
                if (stop) {
                    return;
                }
            }
            {
                AutoLock al(m);
                ++num_connects;
            }
            self.logDebug("SSE reconnecting (connection %d)", num_connects);
        }
    }

    #! Runs a single SSE stream session (connect, read events, until close or error)
    private runSseStream() {
        HttpClientIo::HttpClientStreamHandle stream = mgr.acquireStream(url);
        hash<auto> headers = {
            "Accept": "text/event-stream",
        } + (connect_headers ?? {});
        if (last_event_id) {
            headers."Last-Event-ID" = last_event_id;
        }

        stream.startStreaming("GET", url_path, headers);

        HttpClientStreamingIo::SseStreamReader reader(stream);

        while (True) {
            if (!self) {
                return;
            }
            {
                AutoLock al(m);
                if (stop) {
                    reader.cancel();
                    return;
                }
            }

            *hash<SseMessageInfo> evt = reader.readEvent(timeout_ms);
            if (!evt) {
                if (reader.isDone()) {
                    return;
                }
                # Timeout — yield and retry
%ifdef PO_NO_PROCESS_CONTROL
                yield();
%else
                if (yield) { yield(); }
%endif
                continue;
            }

            # Track last event ID for reconnection
            if (evt.id.val()) {
                last_event_id = string(evt.id);
            }

            # Deserialize and dispatch
            try {
                if (evt.data.val()) {
                    evt.data = deserializeData(evt.data);
                }
                if (evt.id =~ /^[0-9]+$/) {
                    evt.id = evt.id.toInt();
                }
            } catch (hash<ExceptionInfo> ex) {
                self.logError("Error deserializing SSE event: %s", get_exception_string(ex));
                continue;
            }

            notifyObservers(EVENT_SSE_DATA, evt);
        }
    }

    #! Deserializes SSE event data based on the serialization setting
    private auto deserializeData(string buf) {
%ifndef NoJson
        if (serialization == "json") {
            return parse_json(buf);
        }
%endif
        return buf;
    }
}

} # namespace ServerSentEventClient
