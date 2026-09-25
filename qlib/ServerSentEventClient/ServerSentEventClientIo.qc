# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClientIo.qc SSE client using RestClientIo for async I/O

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

#! SSE client using asynchronous I/O across HTTP/1.x, HTTP/2, and HTTP/3
/** This class provides the same Observable-based API as @ref ServerSentEventClient; the event stream is opened with
    <a href="../../RestClientIo/html/index.html">RestClientIo</a>, which validates the response and decodes any
    content encoding.

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

    @par Events
    - @ref EVENT_SSE_CONNECTED "EVENT_SSE_CONNECTED": raised when the event stream has been opened, also after a reconnect
    - @ref EVENT_SSE_DATA "EVENT_SSE_DATA": raised for each event received; the data is deserialized according to the serialization
      option
    - @ref EVENT_SSE_CLOSED "EVENT_SSE_CLOSED": raised when the event stream ends, including when it is stopped with
      @ref disconnect()

    @note The event loop runs in a background thread.  Event callbacks are invoked from
    that thread.  This class is thread-safe for connect/disconnect/reconnect calls; the event thread holds a
    reference to the object, so @ref disconnect() must be called, or the object deleted, to stop it.
*/
public class ServerSentEventClientIo inherits Logger::LoggerWrapper, ConnectionProvider::OptionHelper,
        DataProvider::DelayedObservable {

    private {
        #! Protects the connection state
        Mutex m();

        #! Signaled when the event thread terminates and to interrupt a reconnect delay
        Condition stop_cond();

        #! Serializes connect() and disconnect() calls
        Mutex op_lock();

        #! The REST client used to open the event stream
        RestClientIo::RestClientIo rest;

        #! URL for SSE connection
        string url;

        #! Additional headers for the SSE request
        *hash<auto> connect_headers;

        #! I/O timeout for opening the stream and for SSE event reading
        timeout timeout_ms = ServerSentEventClient::DefaultTimeout;

        #! Stop flag
        bool stop = False;

        #! TID of the event thread
        *int tid;

        #! The reader of the open event stream
        *HttpClientStreamingIo::SseStreamReader cur_reader;

        #! True if an event stream is open
        bool connected = False;

        #! Reconnect flag
        bool reconnect_flag = False;

        #! Set by @ref reconnect() to reconnect immediately
        bool do_reconnect = False;

        #! Reconnect delay
        timeout reconnect_delay = ServerSentEventClient::ReconnectDelay;

        #! Max reconnect retries; negative = infinite
        int max_reconnect_retries = ServerSentEventClient::MaxReconnectRetries;

        #! Number of connections made
        int num_connects = 0;

        #! Unique connection ID of the current event stream; -1 when not connected
        int cid = -1;

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
        auto request_body;

        #! Optional AsyncAPI validator
        *AsyncApi::AsyncApi3Schema asyncapi_validator;

        #! AsyncAPI channel name for validation
        *string asyncapi_channel;

        #! Connection sequence
        static Sequence seq();
    }

    public {
        #! Options passed through to the REST client
        const RestClientOptions = (
            "http_version", "max_redirects", "redirect_passthru", "proxy", "ssl_cert_path",
            "ssl_cert_data", "ssl_key_path", "ssl_key_data", "ssl_key_password", "max_response_body_size",
        );
    }

    #! Creates the SSE client
    /** @param opts constructor options:
        - \c url: (required) the SSE endpoint URL; \c sse:// and \c sses:// URLs are converted to \c http:// and
          \c https:// URLs
        - \c connect_timeout: connection timeout (default: 30s)
        - \c timeout: I/O timeout for opening the event stream and for SSE event reading (default: 15s)
        - \c reconnect: if True, auto-reconnect on connection loss (default: False)
        - \c reconnect_delay: delay between reconnect attempts (default: 2s)
        - \c max_reconnect_retries: max retries; negative = infinite (default: -1)
        - \c serialization or \c data: "json" or "none" (default: "none")
        - \c headers: headers to send in the request
        - \c token and \c token_type: an authorization token sent in the \c Authorization header
        - \c username and \c password: credentials for HTTP basic authentication, if not given in the URL
        - \c last_event_id: the ID sent in the \c Last-Event-ID header
        - \c method and \c body: the HTTP method and request body (default: \c GET without a body)
        - \c http_version, \c proxy, \c max_redirects, \c redirect_passthru, \c ssl_verify_cert, \c ssl_cert_path,
          \c ssl_cert_data, \c ssl_key_path, \c ssl_key_data, \c ssl_key_password, \c max_response_body_size:
          HTTP options of the REST client
        - \c logger: a logger
        - \c yield: callable to yield thread execution (for sandboxed environments)
        - \c start_thread: callable to start a new thread (for sandboxed environments)
        - \c asyncapi: a string URL or file path to an AsyncAPI schema to load for automatic validation of
          deserialized event data
        - \c validator: an @ref AsyncApi::AsyncApi3Schema "AsyncApi3Schema" object to use for automatic
          validation of deserialized event data; this takes precedence over \c asyncapi
        - \c asyncapi_channel: the AsyncAPI channel name to use with \c asyncapi or \c validator; required when
          either validation option is configured

        @throw SSE-ERROR missing or invalid option
    */
    constructor(hash<auto> opts) {
        url = getString(opts, "url");
        if (!url.val()) {
            throw "SSE-ERROR", "missing 'url' option";
        }
        # Normalize sse:// and sses:// schemes
        url =~ s/^sse:/http:/;
        url =~ s/^sses:/https:/;

        logger = opts.logger;

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

        # the credentials are sent in the Authorization header built above
        string rest_url = url;
        rest_url =~ s/^([a-z][a-z0-9+.-]*:\/\/)[^@\/]*@/$1/i;
        hash<auto> rest_opts = {
            "url": rest_url,
            "timeout": timeout_ms,
            "connect_timeout": opts.connect_timeout ?? 30s,
            "ssl_verify_mode": opts.ssl_verify_cert ? SSL_VERIFY_PEER : SSL_VERIFY_NONE,
            "logger": opts.logger,
        };
        foreach string key in (RestClientOptions) {
            if (exists opts{key}) {
                rest_opts{key} = opts{key};
            }
        }
        rest = new RestClientIo::RestClientIo(rest_opts);
    }

    #! Destroys the client
    destructor() {
        disconnect();
        rest.close();
    }

    #! Sets or clears the logger
    /** @param logger the logger, if any
    */
    setLogger(*LoggerInterface logger) {
        LoggerWrapper::setLogger(logger);
        rest.setLogger(logger);
    }

    #! Connects to the SSE endpoint and starts the event loop in a background thread
    /** The event stream is opened in the calling thread, so errors opening it are raised by this method.  If an
        event thread is already running, it is stopped before the new connection is made.

        @param opts optional connection options:
        - \c hdr: headers for the SSE request, replacing the headers given in the constructor
        - \c method: the HTTP method
        - \c body: the request body

        @throw SSE-ERROR the server returned an error response or a response that is not an event stream
        @throw HTTPCLIENT-CONNECT-ERROR the connection cannot be established
    */
    connect(*hash<auto> opts) {
        AutoLock al(op_lock);
        stopEventThread();

        if (opts.hdr) {
            connect_headers = opts.hdr;
        }
        if (opts.method) {
            method = opts.method.upr();
        }
        if (exists opts.body) {
            request_body = opts.body;
        }

        HttpClientStreamingIo::SseStreamReader reader = openStream();
        on_error reader.cancel();

        {
            AutoLock al1(m);
            stop = False;
            do_reconnect = False;
            setConnectedUnlocked(reader);
        }

        code<nothing()> fn = sub () {
            try {
                eventLoop(reader);
            } catch (hash<ExceptionInfo> ex) {
                # the object was deleted from the event thread itself, for example by an observer
                if (ex.err != "OBJECT-ALREADY-DELETED") {
                    rethrow;
                }
            }
        };
        int new_tid;
%ifdef PO_NO_THREAD_CONTROL
        new_tid = start_thread(fn);
%else
        new_tid = start_thread ? start_thread(fn) : background fn();
%endif

        {
            AutoLock al1(m);
            tid = new_tid;
        }

        self.logDebug("SSE I/O thread started: TID %d", new_tid);
        notifyObservers(EVENT_SSE_CONNECTED, {});
    }

    #! Disconnects from the SSE endpoint and stops the event loop
    /** The event stream is canceled and the call blocks until the background event thread terminates; when called
        from the event thread itself, the thread is only told to stop.  Clears the reconnect flag to prevent
        automatic reconnection.
    */
    disconnect() {
        {
            AutoLock al(m);
            reconnect_flag = False;
            if (tid && tid == gettid()) {
                requestStopUnlocked();
                return;
            }
        }
        AutoLock al(op_lock);
        stopEventThread();
    }

    #! Initiates a reconnection to the server immediately
    /** Also sets the reconnect flag, so the client reconnects when the stream is closed

        @return @ref True if the reconnect was started, @ref False if the event thread is not running
    */
    bool reconnect() {
        AutoLock al(m);
        if (!tid) {
            return False;
        }
        reconnect_flag = True;
        if (!stop && !do_reconnect) {
            do_reconnect = True;
            if (cur_reader) {
                cur_reader.cancel();
            }
            stop_cond.broadcast();
        }
        return True;
    }

    #! Called when all observers have been added to the object
    /** Satisfies the @ref DataProvider::DelayedObservable "DelayedObservable"
        contract; starts the background event loop by calling @ref connect()
        with no extra headers.  Callers that need to supply per-request
        headers or an alternate method/body should invoke @ref connect()
        themselves and skip this method.

        @throw SSE-ERROR the server returned an error response or a response that is not an event stream
    */
    observersReady() {
        connect();
    }

    #! Called to stop generating events — disconnects the SSE connection
    stopEvents() {
        disconnect();
    }

    #! Returns True if an event stream is open
    /** @return \c True if an event stream is open
    */
    bool isOpen() {
        AutoLock al(m);
        return connected;
    }

    #! Returns True if the event thread is running but no event stream is open, because it is being reopened
    bool isReconnecting() {
        AutoLock al(m);
        return tid && !connected;
    }

    #! Returns the reconnect flag
    /** @return \c True if auto-reconnection is enabled
    */
    bool getReconnect() {
        return reconnect_flag;
    }

    #! Returns the data serialization value (\c "none", \c "json")
    string getSerialization() {
        return serialization;
    }

    #! Returns the ID of the last event received with an ID, or the ID given in the constructor
    *string getLastEventId() {
        AutoLock al(m);
        return last_event_id;
    }

    #! Returns information about the current connection
    /** @return a hash with the following keys:
        - \c cid: the unique ID of the current event stream; -1 if not connected
        - \c num_connects: how many times the client has connected to the server
        - \c connected: @ref True if an event stream is open
    */
    hash<auto> getConnectionInfo() {
        AutoLock al(m);
        return {
            "cid": cid,
            "num_connects": num_connects,
            "connected": connected,
        };
    }

    #! Returns the SSE endpoint URL
    /** @return the URL passed to the constructor (with \c sse:// normalized to \c http://)
    */
    string getUrl() {
        return url;
    }

    #! Returns the URL without any password
    string getSafeUrl() {
        return rest.getSafeUrl();
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

    #! Stops the event thread and waits for it to terminate; must be called with @ref op_lock held
    private stopEventThread() {
        AutoLock al(m);
        if (tid) {
            requestStopUnlocked();
            while (tid) {
                stop_cond.wait(m);
            }
        }
    }

    #! Tells the event thread to stop; must be called with @ref m held
    private requestStopUnlocked() {
        stop = True;
        if (cur_reader) {
            cur_reader.cancel();
        }
        stop_cond.broadcast();
    }

    #! Records an open event stream; must be called with @ref m held
    private setConnectedUnlocked(HttpClientStreamingIo::SseStreamReader reader) {
        cur_reader = reader;
        connected = True;
        ++num_connects;
        cid = seq.next();
    }

    #! Opens the event stream
    /** @return the reader of the event stream

        @throw SSE-ERROR the server returned an error response or a response that is not an event stream
    */
    private HttpClientStreamingIo::SseStreamReader openStream() {
        hash<auto> headers = {
            "Accept": MimeTypeSse,
        } + (connect_headers ?? {});
        {
            AutoLock al(m);
            if (last_event_id) {
                headers."Last-Event-ID" = last_event_id;
            }
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

        try {
            return rest.restSseReaderWithTimeout(method, "", headers, send_body, timeout_ms);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "REST-RESPONSE-ERROR" && ex.arg.typeCode() == NT_HASH && ex.arg.status_code) {
                if (ex.arg.status_code >= 300) {
                    throw "SSE-ERROR", sprintf("HTTP server at URL %y returned status code %d to our request; "
                        "expecting 200 OK", getSafeUrl(), ex.arg.status_code), ex.arg;
                }
                throw "SSE-ERROR", sprintf("HTTP server at URL %y did not return an event stream: %s",
                    getSafeUrl(), ex.desc), ex.arg;
            }
            rethrow;
        }
    }

    #! Background event loop
    /** @param reader the reader of the event stream opened by @ref connect()
    */
    private eventLoop(HttpClientStreamingIo::SseStreamReader reader) {
        on_exit if (self) {
            AutoLock al(m);
            remove tid;
            remove cur_reader;
            connected = False;
            cid = -1;
            stop = False;
            do_reconnect = False;
            stop_cond.broadcast();
            self.logDebug("SSE I/O thread TID %d terminating", gettid());
        }

        while (True) {
            try {
                readEvents(reader);
            } catch (hash<ExceptionInfo> ex) {
                if (!self) {
                    return;
                }
                self.logError("SSE stream error: %s: %s", ex.err, ex.desc);
            }
            # the reader is canceled if it has not ended, for example after a stream error
            reader.cancel();
            if (!self) {
                return;
            }

            {
                AutoLock al(m);
                remove cur_reader;
                connected = False;
                cid = -1;
            }
            notifyObservers(EVENT_SSE_CLOSED, {});

            *HttpClientStreamingIo::SseStreamReader new_reader = reopenStream();
            if (!new_reader) {
                return;
            }
            reader = new_reader;
            notifyObservers(EVENT_SSE_CONNECTED, {});
        }
    }

    #! Reopens the event stream if the client reconnects
    /** @return the reader of the new event stream, or @ref nothing if the event thread is to terminate
    */
    private *HttpClientStreamingIo::SseStreamReader reopenStream() {
        int reconnect_attempts = 0;
        while (True) {
            {
                AutoLock al(m);
                if (stop || !reconnect_flag) {
                    return;
                }
                if (do_reconnect) {
                    # an immediate reconnect requested with reconnect()
                    do_reconnect = False;
                } else {
                    # Check retry limit (negative = infinite)
                    ++reconnect_attempts;
                    if (max_reconnect_retries >= 0 && reconnect_attempts > max_reconnect_retries) {
                        self.logInfo("SSE max reconnect retries (%d) exceeded; giving up", max_reconnect_retries);
                        return;
                    }
                    self.logInfo("SSE connection lost; reconnecting in %y (attempt %d/%s)",
                        reconnect_delay, reconnect_attempts,
                        max_reconnect_retries < 0 ? "inf" : string(max_reconnect_retries));
                    stop_cond.wait(m, reconnect_delay);
                    if (stop) {
                        return;
                    }
                    do_reconnect = False;
                }
            }

            HttpClientStreamingIo::SseStreamReader reader;
            try {
                reader = openStream();
            } catch (hash<ExceptionInfo> ex) {
                self.logInfo("SSE reconnect failed: %s: %s", ex.err, ex.desc);
                continue;
            }

            AutoLock al(m);
            if (stop) {
                reader.cancel();
                return;
            }
            setConnectedUnlocked(reader);
            self.logDebug("SSE reconnected (connection %d)", num_connects);
            return reader;
        }
    }

    #! Reads events from the stream until it ends or the event thread is told to stop or reconnect
    private readEvents(HttpClientStreamingIo::SseStreamReader reader) {
        while (True) {
            if (!self) {
                return;
            }
            {
                AutoLock al(m);
                if (stop || do_reconnect) {
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
                if (yield) {
                    yield();
                }
%endif
                continue;
            }

            # Track last event ID for reconnection
            if (evt.id.val()) {
                AutoLock al(m);
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
