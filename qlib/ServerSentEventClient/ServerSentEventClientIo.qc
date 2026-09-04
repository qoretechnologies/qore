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

        #! HTTP method for the SSE request (default: GET)
        string method = "GET";

        #! Request body for non-GET requests
        *data request_body;

        #! Optional AsyncAPI validator
        *AsyncApi::AsyncApi3Schema asyncapi_validator;

        #! AsyncAPI channel name for validation
        *string asyncapi_channel;
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
        - \c asyncapi: a string URL or file path to an AsyncAPI schema to load for automatic validation of
          deserialized event data
        - \c validator: an @ref AsyncApi::AsyncApi3Schema "AsyncApi3Schema" object to use for automatic
          validation of deserialized event data; this takes precedence over \c asyncapi
        - \c asyncapi_channel: the AsyncAPI channel name to use with \c asyncapi or \c validator; required when
          either validation option is configured
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

        logger = opts.logger;

        timeout connect_timeout = opts.connect_timeout ?? 30s;
        if (exists opts.timeout) {
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
        if (opts.serialization || opts.data) {
            serialization = opts.serialization ?? opts.data;
            if (!ServerSentEventClient::DataSerializationSupport{serialization}) {
                throw "SSE-ERROR", sprintf("unsupported serialization %y; valid values: %y",
                    serialization, keys ServerSentEventClient::DataSerializationSupport);
            }
        }
        yield = opts.yield;
        start_thread = opts.start_thread;

        if (exists opts.last_event_id) {
            last_event_id = opts.last_event_id.toString();
        }

        connect_headers = {} + (opts.headers ?? {});
        if (opts.token.val() && !connect_headers.Authorization) {
            connect_headers.Authorization = sprintf("%s %s", opts.token_type ?? "Bearer", opts.token);
        } else if (!connect_headers.Authorization) {
            hash<UrlInfo> url_info = parse_url(url);
            *string username = url_info.username;
            *string password = url_info.password;
            if ((!username.val() || !password.val()) && opts.username.val() && opts.password.val()) {
                username = opts.username;
                password = opts.password;
            }
            if (username.val() && password.val()) {
                connect_headers.Authorization = "Basic " + (username + ":" + password).toBase64();
            }
        }

        configureAsyncApiOptions(opts);

        # set HTTP method option, if any
        if (opts.method) {
            method = opts.method.upr();
        }

        # set request body option, if any
        if (exists opts.body) {
            if (method == "GET") {
                throw "SSE-ERROR", "'body' option cannot be used with GET method";
            }
            request_body = opts.body;
        }

        hash<HttpClientIo::HttpClientConnectionManagerOptions> manager_opts =
            <HttpClientIo::HttpClientConnectionManagerOptions>{
                "connect_timeout": connect_timeout,
                "request_timeout": timeout_ms,
                "protocol": ConnectionProvider::HttpConnection::AsyncHttpVersionMap{
                    opts.http_version ?? "auto"},
                "ssl_verify_mode": opts.ssl_verify_cert ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
                "redirect_passthru": opts.redirect_passthru,
                "logger": opts.logger,
            };
        if (exists opts.max_redirects) {
            manager_opts.max_redirects = opts.max_redirects;
        }
        if (opts.proxy.val()) {
            manager_opts.proxy_url = opts.proxy;
        }
        *data cert_data = opts.ssl_cert_data;
        if (!exists cert_data && opts.ssl_cert_path.val()) {
            cert_data = getSslFileData(opts.ssl_cert_path);
        }
        if (exists cert_data) {
            manager_opts.client_cert = new SSLCertificate(cert_data);
        }
        *data key_data = opts.ssl_key_data;
        if (!exists key_data && opts.ssl_key_path.val()) {
            key_data = getSslFileData(opts.ssl_key_path);
        }
        if (exists key_data) {
            manager_opts.client_pk = new SSLPrivateKey(key_data, opts.ssl_key_password);
        }
        mgr = new HttpClientIo::HttpClientConnectionManager(manager_opts);
    }

    #! Loads string data through FileLocationHandler without introducing a static module dependency
    private static string getSslFileData(string location) {
        load_module("FileLocationHandler");
        Class c = Class::forName("FileLocationHandler");
        StaticMethod m = c.getStaticMethod("getTextFileFromLocation");
        return m.call(location);
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
        if (opts.method) {
            method = opts.method.upr();
        }
        if (exists opts.body) {
            request_body = opts.body;
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

    #! Called when all observers have been added to the object
    /** Satisfies the @ref DataProvider::DelayedObservable "DelayedObservable"
        contract; starts the background event loop by calling @ref connect()
        with no extra headers.  Callers that need to supply per-request
        headers or an alternate method/body should invoke @ref connect()
        themselves and skip this method.
    */
    observersReady() {
        connect();
    }

    #! Called to stop generating events — disconnects the SSE connection
    stopEvents() {
        disconnect();
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

    #! Sets an AsyncAPI validator for received SSE event data
    /** @param validator the AsyncAPI schema validator
        @param channel the channel name to use for validation
    */
    setAsyncApiValidator(AsyncApi::AsyncApi3Schema validator, string channel) {
        asyncapi_validator = validator;
        asyncapi_channel = channel;
    }

    #! Returns the current AsyncAPI validator, if any
    *AsyncApi::AsyncApi3Schema getAsyncApiValidator() {
        return asyncapi_validator;
    }

    #! Returns the current AsyncAPI channel name, if any
    *string getAsyncApiChannel() {
        return asyncapi_channel;
    }

    #! Clears the AsyncAPI validator
    clearAsyncApiValidator() {
        delete asyncapi_validator;
        delete asyncapi_channel;
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

        # serialize body for non-GET methods
        *data send_body;
        if (exists request_body) {
            if (serialization == "json" && request_body.typeCode() != NT_STRING) {
                send_body = make_json(request_body);
            } else {
                send_body = request_body;
            }
            if (serialization == "json" && !headers."Content-Type") {
                headers."Content-Type" = MimeTypeJson;
            }
        }

        stream.startStreaming(method, url_path, headers, False, send_body);

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
                validateIncomingEvent(evt);
            } catch (hash<ExceptionInfo> ex) {
                self.logError("Error processing SSE event: %s", get_exception_string(ex));
                continue;
            }

            notifyObservers(EVENT_SSE_DATA, evt);
        }
    }

    #! Deserializes SSE event data based on the serialization setting
    private auto deserializeData(string buf) {
        if (serialization == "json") {
            return parse_json(buf);
        }
        return buf;
    }

    private configureAsyncApiOptions(hash<auto> opts) {
        *string asyncapi_url;
        if (exists opts.validator) {
            if (!(opts.validator instanceof AsyncApi::AsyncApi3Schema)) {
                throw "SSE-ERROR", sprintf("option 'validator' has type %y; expecting AsyncApi::AsyncApi3Schema",
                    opts.validator.fullType());
            }
            asyncapi_validator = opts.validator;
        }
        if (exists opts.asyncapi) {
            if (opts.asyncapi.typeCode() != NT_STRING || !opts.asyncapi.val()) {
                throw "SSE-ERROR", sprintf("option 'asyncapi' has type %y and value %y; expecting a non-empty "
                    "string URL or file path", opts.asyncapi.fullType(), opts.asyncapi);
            }
            asyncapi_url = opts.asyncapi;
        }
        if (exists opts.asyncapi_channel) {
            if (opts.asyncapi_channel.typeCode() != NT_STRING || !opts.asyncapi_channel.val()) {
                throw "SSE-ERROR", sprintf("option 'asyncapi_channel' has type %y and value %y; expecting a "
                    "non-empty string channel name", opts.asyncapi_channel.fullType(), opts.asyncapi_channel);
            }
            asyncapi_channel = opts.asyncapi_channel;
        }
        if (!asyncapi_validator && asyncapi_url) {
            asyncapi_validator = AsyncApi::AsyncApi3Loader::fromUrl(asyncapi_url);
        }
        if (asyncapi_validator && !asyncapi_channel) {
            throw "SSE-ERROR", "option 'asyncapi_channel' is required when 'asyncapi' or 'validator' is configured";
        }
        if (!asyncapi_validator && asyncapi_channel) {
            throw "SSE-ERROR", "option 'asyncapi_channel' requires 'asyncapi' or 'validator'";
        }
    }

    private validateIncomingEvent(hash<auto> event) {
        if (asyncapi_validator && asyncapi_channel) {
            auto payload = exists event.data ? event.data : NOTHING;
            asyncapi_validator.validateIncomingMessage(asyncapi_channel, \payload, getAsyncApiHeaders(event));
            if (exists event.data) {
                event.data = payload;
            }
        }
    }

    private static hash<auto> getAsyncApiHeaders(hash<auto> event) {
        hash<auto> headers;
        foreach string key in (("event", "id", "retry", "comment")) {
            if (exists event{key}) {
                headers{key} = event{key};
            }
        }
        return headers;
    }

    #! Emit a DEBUG-level log entry via the wrapped logger, if any
    private logDebug(string fmt, ...) {
        logArgs(LoggerLevel::LevelDebug, fmt, argv);
    }

    #! Emit an INFO-level log entry via the wrapped logger, if any
    private logInfo(string fmt, ...) {
        logArgs(LoggerLevel::LevelInfo, fmt, argv);
    }

    #! Emit an ERROR-level log entry via the wrapped logger, if any
    private logError(string fmt, ...) {
        logArgs(LoggerLevel::LevelError, fmt, argv);
    }
}

} # namespace ServerSentEventClient
