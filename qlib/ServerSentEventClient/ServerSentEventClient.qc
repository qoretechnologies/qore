# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ServerSentEventClient.qc SSE connection client

/*  ServerSentEventClient.qc Copyright 2025 Qore Technologies, s.r.o.

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
#! default port for connections
public const SseDefaultPort = 80;

#! ServerSentEvent data event constant
/** The event data has the following key:
    - \c data: the message data
*/
public const EVENT_SSE_DATA = "sse-data-event";

#! ServerSentEvent connection closed event constant
/** This event has no data
*/
public const EVENT_SSE_CLOSED = "sse-closed-event";

#! ServerSentEvent connected event type
/** This event has no data
*/
public const EVENT_SSE_CONNECTED = "sse-connected-event";

#! the main sse client class
/** To use this class, create a @ref ServerSentEventClient::ServerSentEventClient "ServerSentEventClient" object and the call
    @ref ServerSentEventClient::ServerSentEventClient::connect() "ServerSentEventClient::connect()".

    The @ref ServerSentEventClient::ServerSentEventClient::connect() "ServerSentEventClient::connect()" method starts a background
    thread to receive messages, which are then raised as observable events.

    To stop listening for SSE events, call
    @ref ServerSentEventClient::ServerSentEventClient::disconnect() "ServerSentEventClient::disconnect()".

    The @ref ServerSentEventClient::ServerSentEventClient "ServerSentEventClient" class includes support for running in sandboxed
    Program objects with the following parse options set:
    - \c PO_NO_PROCESS_CONTROL: in this case the \c "yield" option is required in
      @ref ServerSentEventClient::ServerSentEventClient::constructor() "ServerSentEventClient::constructor()"
*/
public class ServerSentEventClient inherits Logger::LoggerWrapper, ConnectionProvider::OptionHelper,
        DataProvider::DelayedObservable {
    public {
        #! Data serialization support
        const DataSerializationSupport = {
%ifndef NoJson
            "json": True,
%endif
            "none": True,
        };

        #! Default socket I/O operation timeout: 15 seconds
        const DefaultTimeout = 15s;

        #! Client version
        const Version = "1.0";

        #! default user agent string for HTTP requests
        const DefaultUserAgent = sprintf("Qore-SSE-Client/%s", ServerSentEventClient::Version);

        #! Max reconnect retries; negative numbers = infinite
        const MaxReconnectRetries = -1;

        #! Reconnect retry delay
        const ReconnectDelay = 2s;
    }

    private {
        Mutex m();
        Condition sleep_cond();
        HTTPClient hc;
        bool stop;

        #! Request Last-Event-Id when connecting
        string last_event_id;

        #! Flag indicating chunked transfer encoding
        bool chunked;

        #! The content encoding used by the server
        string content_encoding;

        #! The decompression output stream
        StringOutputStream ostream;

        #! The decompression transform stream
        TransformOutputStream dstream;

        timeout timeout_ms = DefaultTimeout;

        #! Reconnect flag
        bool reconnect = False;

        #! TID of the event thread; only changed in the log
        /** Threads can wait for it to be cleared using tid_waiting and tid_cond
        */
        int tid;

        #! Event thread waiting count
        int tid_waiting;

        #! Event thread condition variable
        Condition tid_cond();

        #! Unique sse connection ID; -1 when not connected
        int cid = -1;

        #! callable object to yield the current thread's execution
        *code yield;
        #! callable value to start a new thread; must take a callable object as a parameter and return an integer TID
        *code start_thread;

        #! Total number of connections made
        int num_connects;

        #! URL string
        string url;

        #! Flag for a user-initiated reconnect
        bool do_reconnect;

        #! data serialization option: json or none
        string serialization;

        #! Connection sequence
        static Sequence seq();

        #! Event disposition code: continue processing event loop
        const SSEEDC_CONTINUE = 1;

        #! Event disposition code: break event loop
        const SSEEDC_BREAK = 2;
    }

    #! Creates the ServerSentEvent client object from the arguments
    /** @par Example:
        @code{.py}
sub event(*data msg) {
    if (!msg) {
        printf("connection closed\n");
    } else {
        printf("%s msg: %y\n", now_us().format("YYYY-MM-DD HH:mm:SS.xx"), msg);
    }
}
ServerSentEventClient sse(\event(), {
    "url": "sse://example.com:8080/path",
});
        @endcode

        @param opts an option hash for the @ref Qore::HTTPClient::constructor(hash<auto>) "HTTPClient constructor"
        plus the following keys:
        - \c last_event_id: Request to the server to resend messages starting after this ID
        - \c logger: an optional @ref Logger::LoggerInterface "LoggerInterface" object for internal logging
        - \c reconnect: try to reconnect when the connection is closed or lost (default @ref False)
        - \c yield: an options @ref closure "closure" or @ref call_reference "call reference" for yielding the
            current thread's execution to another thread (default value: @ref Qore::Threads::thread_yield());
            required if @ref Qore::PO_NO_PROCESS_CONTROL is set in the current execution context

        @throw SSE-ERROR unknown scheme, missing 'url' key in option hash; invalid option value
    */
    constructor(hash<auto> opts) {
        constructorInit(opts);
    }

    #! disconnects the connection and stops the event thread if connected
    destructor() {
        if (reconnect) {
            reconnect = False;
        }
        disconnectIntern(True);
    }

    #! Called when all observers have been added to the object
    /** This method is meant to trigger event generation
    */
    observersReady() {
        start();
    }

    #! Called to stop generating events
    /** Disconnects from the server
    */
    stopEvents() {
        disconnect();
    }

    #! Returns the data serialization value (\c "none", \c "json")
    string getSerialization() {
        return serialization;
    }

    #! returns the URL given in the constructor
    string getUrl() {
        return url;
    }

    #! Returns a "safe" URL, without any password info
    string getSafeUrl() {
        return hc.getSafeURL() ?? "";
    }

    #! returns the unique connection ID
    int getConnectionId() {
        return cid;
    }

    #! returns a hash of URL scheme information for URL schemes supported by this object
    hash<auto> getSchemes() {
        return SseSchemes;
    }

    #! Returns the value of the reconnect flag
    bool getReconnect() {
        return reconnect;
    }

    #! Connects to the sse server
    /** @par Example:
        @code{.py}
sub event(*data msg) {
    if (!msg) {
        printf("connection closed\n");
    } else {
        printf("%s msg: %y\n", now_us().format("YYYY-MM-DD HH:mm:SS.xx"), msg);
    }
}
ServerSentEventClient sse(\event(), {
    "url": "sse://example.com:8080/path",
});
sse.connect();
        @endcode

        This method starts a background thread to receive messages, which are rasied as observable events.  If the
        server disconnects the SSE connection, the event thread also terminates and the ServerSentEventClient
        object will be in a disconnected state.

        To stop listening for SSE events, call
        @ref ServerSentEventClient::ServerSentEventClient::disconnect() "ServerSentEventClient::disconnect()".

        If this method is called while a connection is already in progress, then the existing connection is first
        implicitly disconnected.

        @param opts a hash with the following keys:
        - \c hdr: (optional) a @ref hash_type "hash" giving header values for the connection request to the SSE
          server
        @param info a @ref reference_type "reference" to a hash which will be set to information about the call setup

        @return a hash with information about the HTTP response from the HTTP server corresponding to the
        return value of @ref Qore::Socket::readHTTPHeader()

        @throw SSE-ERROR the option hash is missing either the 'url' key or there is type error in the option hash
    */
    hash<auto> connect(*hash<auto> opts, *reference<hash<auto>> info) {
        return connectIntern(opts, \info);
    }

    #! returns @ref True "True" if the connection is currently open and active, @ref False "False" if not
    bool isOpen() {
        m.lock();
        on_exit m.unlock();

        return hc.isOpen();
    }

    #! Initiate a server reconnect immediately
    /** @return True if the reconnect was started
    */
    bool reconnect() {
        AutoLock al(m);

        if (!tid) {
            return False;
        }

        if (!reconnect) {
            reconnect = True;
        }

        # check if already reconnecting
        if (!stop && !do_reconnect) {
            do_reconnect = True;
            # wake up any sleeping threads
            sleep_cond.broadcast();
        }
        # wait for the event thread to terminate
        waitTidCleared(tid);

        return True;
    }

    #! Disconnect with the given @ref closecodes "close code"
    disconnect() {
        # do not reconnect if an explicit disconnect is called
        if (reconnect) {
            reconnect = False;
        }

        disconnectIntern();
    }

    #! Returns @ref True if the client is reconnecting
    bool isReconnecting() {
        return num_connects && !isOpen();
    }

    #! Returns information about the current connection
    /** @returns a hash with the following keys:
        - \c cid: the ServerSentEvent client connection ID
        - \c num_connects: how many times has the client connected to the server
        - \c peer: information about the connected server socket (see
          @ref Qore::Socket::getPeerInfo() "Socket::getPeerInfo()")
        - \c socket: information about the connected local socket (see
          @ref Qore::Socket::getSocketInfo() "Socket::getSocketInfo()")
    */
    *hash<auto> getConnectionInfo() {
        # use += to keep "hash<auto>"
        hash<auto> rv += {
            "cid": cid,
            "num_connects": num_connects,
        };
        try {
            rv += {
                "peer": hc.getPeerInfo(),
                "socket": hc.getSocketInfo(),
            };
        } catch (hash<ExceptionInfo> ex) {
            # ignore exceptions when not connected and return NOTHING
        }
        return rv;
    }

    #! Returns any username set for the connection
    /** @return any username set for the connection
    */
    *string getUsername() {
        return hc.getUsername();
    }

    #! Returns any password set for the connection
    /** @return any password set for the connection
    */
    *string getPassword() {
        return hc.getPassword();
    }

    #! Connects and starts receiving messages
    private start() {
        hash<auto> info;
        try {
            connect(NOTHING, \info);
        } catch (hash<ExceptionInfo> ex) {
            logError("Failed to connect to HTTP server: %s", get_exception_string(ex));
            rethrow;
        }
    }

    #! Connects to the sse server
    private hash<auto> connectIntern(*hash<auto> opts, *reference<hash<auto>> info, *bool reconnect) {
        if (opts.hdr && opts.hdr.typeCode() != NT_HASH) {
            raiseError("option hash to ServerSentEventClient::connect() 'hdr' key not assigned to a hash; type "
                "received: %y", opts.hdr.fullType());
        }

        hash<auto> h;
        {
            m.lock();
            on_exit m.unlock();

            if (hc.isOpen()) {
                disconnectUnlocked();
            }

            h = connectUnlocked(opts.hdr, \info, reconnect);
            on_error {
                clearTid();
            }

%ifdef PO_NO_THREAD_CONTROL
            updateTid(start_thread(\eventLoop()));
%else
            updateTid(start_thread ? start_thread(\eventLoop()) : background eventLoop());
%endif
        }

        logDebug("Connected; SSE I/O thread TID %d; raising connected event: %y", tid,
            map {$1.key: $1.value.observer.className()}, observer_map.pairIterator());
        notifyObservers(EVENT_SSE_CONNECTED, {});
        return h;
    }

    #! Disconnect with the given @ref closecodes "close code"
    private disconnectIntern(*bool destroy) {
        AutoLock al(m);

        if (hc.isOpen()) {
            disconnectUnlocked();
        } else if (destroy && tid && tid != gettid()) {
            # wait for reconnect thread to stop if deleting when disconnected and reconnecting
            stop = True;
            # wake up any sleeping threads
            sleep_cond.broadcast();
            # wait for the event thread to terminate
            waitTidCleared(tid);
        }
    }

    #! Removes any warning @ref Qore::Thread::Queue "Queue" object from the Socket
    /** @par Example:
        @code{.py}
sse.clearWarningQueue();
        @endcode

        @see ServerSentEventClient::setWarningQueue()

        @since %ServerSentEventClient 1.1
    */
    clearWarningQueue() {
        hc.clearWarningQueue();
    }

    #! Sets a @ref Qore::Thread::Queue "Queue" object to receive socket warnings
    /** @par Example:
        @code{.py}
sse.setWarningQueue(5000, 5000, queue, "socket-1");
        @endcode

        @param warning_ms the threshold in milliseconds for individual socket actions (send, receive, connect), if
        exceeded, a socket warning is placed on the warning queue with the following keys:
        - \c "type": a string with the constant value \c "SOCKET-OPERATION-WARNING"
        - \c "operation": a string giving the operation that caused the warning (ex: \c "connect")
        - \c "us": an integer giving the number of microseconds for the operation
        - \c "timeout": an integer giving the warning threshold in microseconds
        - \c "arg": if any \c "arg" argument is passed to the
          @ref setWarningQueue() "ServerSentEventClient::setWarningQueue()" method, it will be included in the warning hash
          here
        @param warning_bs value in bytes per second; if any call has performance below this threshold, a socket
        warning is placed on the warning queue with the following keys:
        - \c "type": a string with the constant value \c "SOCKET-THROUGHPUT-WARNING"
        - \c "dir": either \c "send" or \c "recv" depending on the direction of the data flow
        - \c "bytes": the amount of bytes sent
        - \c "us": an integer giving the number of microseconds for the operation
        - \c "bytes_sec": a float giving the transfer speed in bytes per second
        - \c "threshold": an integer giving the warning threshold in bytes per second
        - \c "arg": if any \c "arg" argument is passed to the
          @ref setWarningQueue() "ServerSentEventClient::setWarningQueue()" method, it will be included in the warning hash
          here
        @param queue the @ref Qore::Thread::Queue "Queue" object to receive warning events
        @param arg an optional argument to be placed in the \c "arg" key in each warning hash (could be used to
        identify the socket for example)
        @param min_ms the minimum transfer time with a resolution of milliseconds for a transfer to be eligible for
        triggering a warning; transfers that take less than this period of time are not eligible for raising a warning

        @throw QUEUE-ERROR the Queue passed has a maximum size set
        @throw SOCKET-SETWARNINGQUEUE-ERROR at least one of \a warning_ms and \a warning_bs must be > 0

        @see ServerSentEventClient::clearWarningQueue()

        @since %ServerSentEventClient 1.1
    */
    setWarningQueue(int warning_ms, int warning_bs, Queue queue, auto arg, timeout min_ms = 1s) {
        hc.setWarningQueue(warning_ms, warning_bs, queue, arg, min_ms);
    }

    #! Returns performance statistics for the socket
    /** @par Example:
        @code{.py}
hash<auto> h = sse.getUsageInfo();
        @endcode

        @return a hash with the following keys:
        - \c "bytes_sent": an integer giving the total amount of bytes sent
        - \c "bytes_recv": an integer giving the total amount of bytes received
        - \c "us_sent": an integer giving the total number of microseconds spent sending data
        - \c "us_recv": an integer giving the total number of microseconds spent receiving data
        - \c "arg": (only if warning values have been set with
          @ref setWarningQueue() "ServerSentEventClient::setWarningQueue()") the optional argument for warning hashes
        - \c "timeout": (only if warning values have been set with
          @ref setWarningQueue() "ServerSentEventClient::setWarningQueue()") the warning timeout in microseconds
        - \c "min_throughput": (only if warning values have been set with
          @ref setWarningQueue() "ServerSentEventClient::setWarningQueue()") the minimum warning throughput in bytes/sec

        @since %ServerSentEventClient 1.1

        @see ServerSentEventClient::clearStats()
    */
    hash<auto> getUsageInfo() {
        return hc.getUsageInfo();
    }

    #! Clears performance statistics
    /** @par Example:
        @code{.py}
sse.clearStats();
        @endcode

        @since %ServerSentEventClient 1.1

        @see ServerSentEventClient::getUsageInfo()
    */
    clearStats() {
        hc.clearStats();
    }

    #! Sets a @ref Qore::Thread::Queue "Queue" object to receive @ref socket_events "socket events"
    /** @par Example:
        @code{.py}
sse.setEventQueue(queue);
        @endcode

        @param queue the @ref Qore::Thread::Queue "Queue" object to receive @ref socket_events "socket events"; note that
        the Queue passed cannot have any maximum size set or a QUEUE-ERROR will be thrown
        @param arg an argument that will be included in each event hash in the \a arg key
        @param with_data if @ref True, then the actual raw data transferred / received is also included in the events

        @throw QUEUE-ERROR the Queue passed has a maximum size set

        @see @ref event_handling for more information

        @since %ServerSentEventClient 1.8
    */
    setEventQueue(Qore::Thread::Queue queue, auto arg, *bool with_data) {
        hc.setEventQueue(queue, arg, with_data);
    }

    #! Removes any @ref Qore::Thread::Queue "Queue" object so that @ref socket_events "socket events" are no longer added to the @ref Qore::Thread::Queue "Queue"
    /** @par Example:
        @code{.py}
sse.setEventQueue();
        @endcode

        @see @ref event_handling for more information

        @since %ServerSentEventClient 1.8
    */
    setEventQueue() {
        hc.setEventQueue();
    }

    #! Common constructor code
    private constructorInit(hash<auto> opts) {
        if (parse_boolean(opts.reconnect)) {
            reconnect = True;
        }
        # set logger, if any
        logger = opts.logger;

        if (exists opts.last_event_id) {
            if (!opts.last_event_id.strp()) {
                raiseError("ServerSentEventClient::constructor() options have an invalid 'last_event_id' key with type "
                    "%y; must be a type convertible to a string for the header value in the connection request to the "
                    "SSE server", opts.last_event_id.fullType());
            }
            last_event_id = opts.last_event_id.toString();
        }

        # += to keep "http_opts" "hash<auto>"
        hash<auto> http_opts += opts;
        url = getString(opts, "url", True);

        # remove SSE-only options
        remove http_opts{"logger", "last_event_id", "data", "reconnect", "yield", "start_thread"};

        if (opts.hasKey("yield")) {
            if (!opts.yield.callp()) {
                raiseError("ServerSentEventClient::constructor() options have an invalid 'yield' key with type "
                    "%y; must be a callable value", opts.yield.fullType());
            }
            yield = opts.yield;
        }
%ifdef PO_NO_PROCESS_CONTROL
        else {
            raiseError("ServerSentEventClient::constructor() options are missing the 'yield' key giving the thread "
                "yield closure/call reference which is required when PO_NO_PROCESS_CONTROL is in effect");
        }
%endif

        if (opts.hasKey("start_thread")) {
            if (!opts.start_thread.callp()) {
                raiseError("option hash to ServerSentEventClient::constructor() has an invalid 'start_thread' key "
                    "with type %y; must be a callable value", opts.start_thread.fullType());
            }
            start_thread = remove opts.start_thread;
        }
%ifdef PO_NO_THREAD_CONTROL
        else {
            raiseError("option hash to ServerSentEventClient::constructor() missing 'start_thread' key giving the a "
                "closure/call reference to start new threads which is required when PO_NO_THREAD_CONTROL is in "
                "effect");
        }
%endif

        # set serialization option, if any
        serialization = getString(opts, "data", True, DataSerializationSupport.firstKey());
        if (!DataSerializationSupport{serialization}) {
            unsupportedSerialization();
        }

        hash<UrlInfo> uh = parse_url(http_opts.url);
        if (uh.protocol) {
            hash<auto> sh = getSchemes();
            *hash<auto> ph = sh{uh.protocol};
            if (!ph) {
                raiseError("schema %y is unknown in %y; known schemes: %y", uh.protocol, http_opts.url, keys sh);
            }
            # replace sse scheme with an http scheme
            splice http_opts.url, 0, uh.protocol.size(), "http" + (ph.ssl ? "s" : "");
        }

        # ensure that the timeout value is set correctly and consistently
        if (http_opts."timeout") {
            timeout_ms = http_opts."timeout";
        } else {
            http_opts."timeout" = timeout_ms;
        }
        # remove the "token" option and use it if the Authorization header is not set
        if ((*string token = remove http_opts.token) && !http_opts.headers.Authorization) {
            http_opts.headers.Authorization = sprintf("%s %s", http_opts.token_type ?? "Bearer", token);
        }
        # unconditionally remove any "token_type" option
        remove http_opts.token_type;

        hc = new HTTPClient(http_opts);
    }

    private disconnectUnlocked() {
        # disconnect only if possible; in case this method is called from the destructor from the event thread, we
        # cannot wait for the event thread to terminate, as there would be a deadlock
        if (tid && (tid != gettid())) {
            stop = True;
            # wake up any sleeping threads
            sleep_cond.broadcast();
            # wait for the event thread to terminate
            waitTidCleared(tid);
            cid = -1;
        }
    }

    #! Handles messages
    private int handleEvent(hash<SseMessageInfo> event) {
        try {
            if (event."data".val()) {
                event."data" = deserializeData(event."data");
            }
            if (event.id =~ /^[0-9]+$/) {
                event.id = event.id.toInt();
            }
        } catch (hash<ExceptionInfo> ex) {
            logError("Error deserializing SSE event: %s (event data: %y)", get_exception_string(ex),
                event);
            return SSEEDC_CONTINUE;
        }

        notifyObservers(EVENT_SSE_DATA, event);

        return SSEEDC_CONTINUE;
    }

    #! Deserializes data received
    private auto deserializeData(string buf) {
        switch (serialization) {
%ifndef NoJson
            case "json":
                return parse_json(buf);
%endif
            case "none":
                return buf;
        }
        unsupportedSerialization();
    }

    #! Throws an UNSUPPORTED-SERIALIZATION error
    private unsupportedSerialization() {
        throw "UNSUPPORTED-SERIALIZATION", sprintf("unsupported data serialization value %y; known serialization "
            "values: %y", serialization, keys DataSerializationSupport);
    }

    #! Called when the connection is closed
    private closingConnection() {
        notifyObservers(EVENT_SSE_CLOSED, {});
    }

    private updateTid(int tid) {
        self.tid = tid;
    }

    #! Clears the tid and stop variables and wakes up any waiting threads
    private clearTid() {
        remove tid;
        stop = False;
        do_reconnect = False;
        if (tid_waiting) {
            tid_cond.broadcast();
        }
    }

    private waitTidCleared(int old_tid) {
        while (tid && tid != old_tid) {
            ++tid_waiting;
            tid_cond.wait(m);
            --tid_waiting;
        }
    }

    private setupDecompression() {
        Transform decompressor;
        switch (content_encoding) {
            case "deflate":
                decompressor = get_decompressor(Qore::COMPRESSION_ALG_ZLIB);
                break;

            case "gzip":
                decompressor = get_decompressor(Qore::COMPRESSION_ALG_GZIP);
                break;

            case "bzip2":
                decompressor = get_decompressor(Qore::COMPRESSION_ALG_BZIP2);
                break;

            case NOTHING:
            case "identity":
                return;

            default:
                throw "ENCODING-ERROR", sprintf("Don't know how to handle Content-Encoding %y",
                    content_encoding);
        }
        ostream = new StringOutputStream();
        dstream = new TransformOutputStream(ostream, decompressor);
    }

    private eventLoop() {
        on_exit {
            # issue #4697: went out of scope in event loop
            if (self) {
                m.lock();
                on_exit m.unlock();
                clearTid();

                logDebug("SSE I/O thread with TID %d terminating", gettid());
            }
        }

        setupDecompression();

        while (True) {
            # issue #4697: went out of scope in event loop
            if (!self) {
                break;
            }
            if (stop) {
                logInfo("SSE I/O thread stopping on user request");
                break;
            }
            if (do_reconnect) {
                logInfo("SSE I/O thread for an externally-initiated reconnect");
                break;
            }

            try {
                if (!hc.isDataAvailable(50ms)) {
%ifdef PO_NO_PROCESS_CONTROL
                    yield();
%else
                    yield ? yield() : Qore::thread_yield();
%endif
                    continue;
                }
                hash<SseMessageInfo> h;
                if (chunked) {
                    hash<auto> hh = hc.readHTTPChunk(timeout_ms);
                    if (hh.body) {
                        dstream.write(hh.body);
                    } else {
                        logInfo("Server closed the stream");
                        break;
                    }
                    h = hc.parseServerSentEvent(ostream.getData());
                } else {
                    h = hc.readServerSentEvent(content_encoding, timeout_ms);
                }
                int disp = handleEvent(h);
                if (disp == SSEEDC_CONTINUE) {
                    continue;
                } else if (disp == SSEEDC_BREAK) {
                    logInfo("I/O thread stopping on request");
                    break;
                }

                break;
            } catch (hash<ExceptionInfo> ex) {
                string err = sprintf("%s: %s: %s", get_ex_pos(ex), ex.err, ex.desc);
                try {
                    logError(err);
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "OBJECT-ALREADY-DELETED") {
                        return;
                    }
                    rethrow;
                }
                break;
            }
        }

        closingConnection();

        if (hc.isOpen()) {
            hc.shutdown();
            hc.close();
        }

        if (!self) {
            return;
        }
        if (reconnect && !stop) {
            startReconnect();
        } else {
            if (stop) {
                logInfo("Disconnected sse connection on external request");
            } else {
                logInfo("Disconnected sse connection; reconnect flag not set");
            }
        }
    }

    #! Starts a reconnect
    private startReconnect() {
        int tid;
%ifdef PO_NO_THREAD_CONTROL
        tid = start_thread(\doReconnect());
%else
        tid = start_thread ? start_thread(\doReconnect()) : background doReconnect();
%endif
        logInfo("Disconnected sse connection; starting reconnect in TID %d", tid);
    }

    #! Handles reconnections
    private doReconnect() {
        try {
            reconnectPreamble();

            # connect to the new gateway
            int retries = 0;
            while (!stop) {
                hash<auto> info;
                try {
                    reestablishConnection(\info);
                    break;
                } catch (hash<ExceptionInfo> ex) {
                    ++retries;
                    if (MaxReconnectRetries > 0 && (retries == MaxReconnectRetries)) {
                        logError("Reconnect %d: failed (%s: %s); max retries exhausted", retries, ex.err, ex.desc);
                        rethrow;
                    }
                    logInfo("Reconnect %d: failed to connect to ServerSentEvent server: %s: %s; sleeping for %y and "
                        "retrying", retries, ex.err, ex.desc, ReconnectDelay);

                    # Use the Mutex and Condition variable to delay, because sleep() is protected with
                    # PO_NO_PROCESS_CONTROL
                    AutoLock al(m);
                    sleep_cond.wait(m, ReconnectDelay);
                }
            }
            if (stop) {
                logInfo("Reconnect %d: interrupted by disconnect request", retries);
            }

            reconnectPostamble();
        } catch (hash<ExceptionInfo> ex) {
            try {
                logError("Failed to reconnect: %s", get_exception_string(ex));
            } catch (hash<ExceptionInfo> ex0) {
                if (ex0.err != "OBJECT-ALREADY-DELETED") {
                    rethrow;
                }
            }
        }
    }

    #! Executes before a reconnect
    private reconnectPreamble() {
        # disconnect if still connected
        disconnectIntern();
        # log connect with URL
        string sseurl = hc.getURL();
        sseurl =~ s/^http/sse/;
        logInfo("Reconnect to %y", sseurl);
    }

    #! Connects to the server when reconnecting
    private reestablishConnection(reference<hash<auto>> info) {
        connectIntern(NOTHING, \info, True);
    }

    #! Executes after a reconnect
    private reconnectPostamble() {
        # intentionally empty
    }

    private logArgs(LoggerLevel level, string message, *softlist<auto> args) {
        try {
            LoggerWrapper::logArgs(level, sprintf("ServerSentEventClient %s: ", cid) + message, args);
        } catch (hash<ExceptionInfo> ex) {
            stderr.printf("ServerSentEventClient cannot log: %s: %s: %s msg: %s", ex.err, ex.desc, level.getStr(),
                vsprintf(message, args));
        }
    }

    private logDebug(string fmt, ...) {
        logArgs(LoggerLevel::DEBUG, fmt, argv);
    }

    private logInfo(string fmt, ...) {
        logArgs(LoggerLevel::INFO, fmt, argv);
    }

    private logWarn(string fmt, ...) {
        logArgs(LoggerLevel::WARN, fmt, argv);
    }

    private logError(string fmt, ...) {
        logArgs(LoggerLevel::ERROR, fmt, argv);
    }

    #! Performs the HTTP socket connection to the ServerSentEvent server
    private hash<auto> connectUnlocked(*hash<auto> hdr0, *reference<hash<auto>> info, *bool reconnect) {
        # in case hdr0 is hash<string, int> or similar
        hash<auto> hdr = {
            "User-Agent": DefaultUserAgent,
        } + hdr0;
        if (last_event_id) {
            hdr."Last-Event-ID" = last_event_id;
        }

        hash<auto> rh;
        {
            on_error {
                logArgs(reconnect ? LoggerLevel::DEBUG : LoggerLevel::ERROR, "error connecting to %s: %N",
                    (url, info));
                rethrow $1.err, sprintf("%s: info: %y", $1.desc, info{"request-uri", "response-uri"});
            }
            rh = hc.send(NOTHING, "GET", NOTHING, hdr, NOTHING, \info);
        }

        if (rh.status_code != 200) {
            raiseError("HTTP server at URL %y returned status code '%d %s' to our request; expecting '200 OK'",
                hc.getURL(), rh.status_code, rh.status_message);
        }

        if (rh."content-type" != MimeTypeSse) {
            raiseError("HTTP server at URL %y did not return Content-Type: %s; got %y instead", hc.getURL(),
                MimeTypeSse, rh."content-type");
        }
        if (rh.hasKey("content-length")) {
            raiseError("HTTP server at URL %y return Content-Length: %d; invalid server response", hc.getURL(),
                rh."content-length");
        }
        if (rh."content-encoding".val()) {
            content_encoding = rh."content-encoding";
        }
        if (rh."transfer-encoding".val()) {
            if (rh."transfer-encoding" == "chunked") {
                chunked = True;
                Transform decompressor;
                switch (content_encoding) {
                    case "deflate":
                    case "gzip":
                    case "bzip2":
                    case NOTHING:
                    case "identity":
                        break;

                    default:
                        throw "ENCODING-ERROR", sprintf("Don't know how to handle Content-Encoding %y",
                            content_encoding);
                }
            } else {
                raiseError("HTTP server at URL %y returned unsupported Transfer-Encoding: %y", hc.getURL(),
                    rh."transfer-encoding");
            }
        }

        logInfo("Connected to %y (open: %y content encoding: %y chunked: %y)", hc.getURL(), hc.isOpen(),
            content_encoding, chunked ?? False);
        ++num_connects;
        cid = seq.next();

        return rh;
    }

    #! throsse an exception due to an error
    private raiseError(string fmt, ...) {
        throw "SSE-ERROR", vsprintf(fmt, argv);
    }
}
}

namespace Priv {
class QueuedObserver inherits DataProvider::Observer {
    private {
        Queue q();
    }

    Queue getQueue() {
        return q;
    }

    #! This method is called when an event is raised
    update(string event_id, hash<auto> data_) {
        q.push(data_."data");
    }
}
}