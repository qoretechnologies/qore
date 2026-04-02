# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientPollOperationImpl.qc HTTP/3 client poll operation implementation over QUIC

/** Http3ClientPollOperationImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace HttpClientIo {

#! HTTP/3 poll operation implementation over QUIC
/** This poll operation manages an HTTP/3 client connection lifecycle:

    1. Create a UDP socket and bind it locally
    2. Perform QUIC handshake (includes TLS 1.3)
    3. Set up HTTP/3 layer (control + QPACK streams)
    4. Submit multiple concurrent HTTP/3 requests
    5. Read QUIC packets and dispatch HTTP/3 responses by stream ID

    Unlike HTTP/2 which requires separate TCP connect + SSL upgrade + preface steps,
    QUIC handles all of this in a single handshake phase.

    The C++ base class (Http3ClientPollOperationBase) provides the SocketPollOperationBase
    fast path for the AsyncIoController, implementing the continuePoll() state machine
    entirely in C++.

    @par Response Delivery
    Responses are delivered via C++ completion actions (PromiseAction for
    request/response, ChannelAction for streaming) — no closures on the I/O thread.
*/
public class Http3ClientPollOperation inherits Http3ClientPollOperationBase, HttpClientPollOperation {
    public {
        #! Maximum STREAM_ID_BLOCKED retries before signaling capacity error
        const MaxStreamBlockedRetries = 3;
    }

    private {
        #! Target URL info (kept for submitRequest header generation)
        hash<auto> h3_url_info;

        #! Logger (kept for Qore-level logging)
        *LoggerInterface h3_logger;

        #! Stream data listeners: stream_id string -> listener object
        hash<string, object> stream_listeners;

        #! Pending data flags for streams where onStreamData fired before
        #! the listener was registered
        hash<string, bool> pending_stream_data;

        #! Lock protecting stream listener registration and dispatch
        Mutex stream_listener_mutex();
    }

    #! Creates the poll operation for a new QUIC connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode optional SSL certificate verification mode
        @param accept_all_certs if True, accept all server certificates
        @param client_cert optional client certificate for mutual TLS (mTLS)
        @param client_pk optional client private key for mutual TLS (mTLS)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *LoggerInterface logger,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *SSLCertificate client_cert, *SSLPrivateKey client_pk,
            HttpClientConnection connection) :
            Http3ClientPollOperationBase(
                Http3ClientPollOperation::createAndConfigureSocket(url_info, ssl_verify_mode,
                    accept_all_certs, client_cert, client_pk),
                url_info.host,
                url_info.port,
                connection) {
        h3_url_info = url_info;
        h3_logger = logger;
        # Store connection in Qore data hash for DGC cycle detection
        setConnection(connection);
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientPollOperation objects cannot be copied";
    }

    #! Registers a stream for onStreamData() notifications and registers
    #! a listener to be notified when data is dispatched
    /** @param stream_id the QUIC stream ID
        @param listener an object with a notifyIo() method
    */
    registerStreamNotifyAndListener(int stream_id, object listener) {
        # Register in C++ for data_ready_streams signaling
        registerStreamNotify(stream_id);
        # Register Qore listener for worker dispatch
        *bool has_pending;
        {
            AutoLock al(stream_listener_mutex);
            stream_listeners{stream_id} = listener;
            has_pending = remove pending_stream_data{stream_id};
        }
        if (has_pending) {
            listener.notifyIo();
        }
    }

    #! Called on a worker thread when stream data has been dispatched
    /** Dispatched by the async I/O controller after the C++ dispatch signals
        data_ready_streams. Wakes the handler for the given stream.
        @param stream_key stream identifier (string representation of stream_id)
    */
    nothing onStreamData(string stream_key) {
        int stream_id = stream_key.toInt();
        AutoLock al(stream_listener_mutex);
        *object listener = stream_listeners{stream_id};
        if (listener) {
            listener.notifyIo();
        } else {
            pending_stream_data{stream_id} = True;
        }
    }

    #! Submits a new HTTP/3 request on this connection
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param promise Promise to resolve with the response (pure C++ dispatch on I/O thread)
        @param max_streams maximum concurrent streams allowed on this connection
        @param streaming if True, submit without END_STREAM (for bidirectional streaming);
        the caller sends body data incrementally via sendStreamData()

        @return the stream ID assigned to this request

        @throw HTTP3-STATE-ERROR if the connection is not ready for requests
        @throw HTTP3-CAPACITY-ERROR if the connection is at capacity
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            Promise promise, int max_streams, bool streaming = False) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot submit request in state: %y", self.getState());
        }

        # Filter pseudo-headers — C++ layer handles them
        hash<auto> req_headers = {};
        if (headers) {
            foreach string key in (keys headers) {
                if (key[0] != ":") {
                    req_headers{key} = headers{key};
                }
            }
        }
        if (!req_headers.host) {
            req_headers.host = h3_url_info.port == 443
                ? h3_url_info.host
                : sprintf("%s:%d", h3_url_info.host, h3_url_info.port);
        }

        # Phase 1: advisory capacity check (C++ acquires stream_lock internally)
        self.checkCapacity(max_streams);

        # Phase 2: submit to QUIC layer with STREAM_ID_BLOCKED retry.
        #
        # IMPORTANT: sock.submitQuicRequest() acquires sock->priv->m internally.
        # The I/O thread holds sock->priv->m (via C++ continuePoll()) and then
        # acquires stream_lock (in handleReading()).  Holding stream_lock here
        # while calling submitQuicRequest() would therefore create a lock-order
        # inversion and risk deadlock.  submitQuicRequest() is intentionally
        # called outside stream_lock.
        self.beginSubmit();

        Socket sock = self.getSocket();
        int stream_id;
        int retries = 0;
        try {
            while (True) {
                try {
                    if (streaming) {
                        stream_id = sock.submitQuicRequestStreaming(method, path, req_headers);
                    } else {
                        stream_id = sock.submitQuicRequest(method, path, req_headers, body);
                    }
                    break;
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "QUIC-ERROR" && ex.desc =~ /STREAM_ID_BLOCKED/
                            && retries < MaxStreamBlockedRetries) {
                        ++retries;
                        # Backoff: 1ms, 2ms, 4ms
                        int backoff_val = 1 << (retries - 1);
                        log(LoggerLevel::DEBUG,
                            "STREAM_ID_BLOCKED retry %d/%d (backoff %dms)",
                            retries, MaxStreamBlockedRetries, backoff_val);
                        # Wait on condition — signaled when continuePoll()
                        # drains completed streams and frees stream slots.
                        self.waitStreamCapacity(backoff_val);

                        if (!self.isReady()) {
                            throw "HTTP3-STATE-ERROR",
                                sprintf("connection closed during STREAM_ID_BLOCKED "
                                    "retry (state: %y)", self.getState());
                        }
                        continue;
                    }
                    if (ex.err == "QUIC-ERROR" && ex.desc =~ /STREAM_ID_BLOCKED/) {
                        # Retries exhausted — record the limit
                        int captured_active = self.getActiveStreamCount();
                        if (!max_streams || captured_active < max_streams) {
                            self.setQuicStreamLimit(captured_active);
                        }
                        log(LoggerLevel::DEBUG,
                            "STREAM_ID_BLOCKED after %d retries — signaling "
                            "capacity error (active: %d, quic_limit: %d)",
                            MaxStreamBlockedRetries, captured_active,
                            self.getQuicStreamLimit());
                        throw "HTTP3-CAPACITY-ERROR",
                            sprintf("QUIC stream limit reached on connection"
                                " (active_streams: %d)", captured_active);
                    }
                    # Connection death errors — mark as closed immediately
                    if (ex.err == "QUIC-HTTP3-ERROR" || ex.err == "QUIC-READ-ERROR"
                            || ex.err == "QUIC-RECV-ERROR") {
                        self.setSubmitError(ex.err, ex.desc);
                    }
                    rethrow;
                }
            }
        } catch () {
            # Ensure submits_in_progress is decremented on all failure paths
            self.endSubmitFail();
            rethrow;
        }

        # Mark this stream_id as awaiting registration
        self.endSubmitOk(stream_id);

        # Phase 3: register Promise (C++ creates PromiseAction internally)
        *hash<auto> reg_result = self.registerStreamWithPromise(stream_id, promise);
        if (reg_result.need_cancel) {
            try {
                int sid = self.getSessionId();
                if (sid) {
                    sock.cancelQuicStream(sid, stream_id);
                }
            } catch () {}
            throw "HTTP3-STATE-ERROR",
                sprintf("connection closed during request submission (state: %y)",
                    self.getState());
        }

        # Deliver any buffered responses by resolving the Promise directly
        if (reg_result.buffered) {
            foreach hash<auto> resp in (reg_result.buffered) {
                resp.protocol = "h3";
                if (resp.end_stream.toBool()) {
                    # Final response — resolve the Promise and clean up
                    try { promise.set(resp); } catch () {}
                    self.cancelStream(stream_id);
                    break;
                }
            }
        }

        return stream_id;
    }

    #! Submits a new HTTP/3 request with Promise + EventNotifier for poll-based delivery
    /** Like submitRequest() but uses registerStreamWithPromiseAndNotifier() so that
        the I/O thread signals @a notifier when the response arrives, allowing the
        caller's continuePoll() to call Future::get() non-blockingly.

        @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param promise Promise to resolve with the response
        @param notifier EventNotifier to signal after the Promise is resolved
        @param max_streams maximum concurrent streams allowed on this connection

        @return the stream ID assigned to this request

        @throw HTTP3-STATE-ERROR if the connection is not ready for requests
        @throw HTTP3-CAPACITY-ERROR if the connection is at capacity
    */
    int submitRequestWithNotifier(string method, string path, *hash<auto> headers, *data body,
            Promise promise, EventNotifier notifier, int max_streams) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot submit request in state: %y", self.getState());
        }

        # Filter pseudo-headers — C++ layer handles them
        hash<auto> req_headers = {};
        if (headers) {
            foreach string key in (keys headers) {
                if (key[0] != ":") {
                    req_headers{key} = headers{key};
                }
            }
        }
        if (!req_headers.host) {
            req_headers.host = h3_url_info.port == 443
                ? h3_url_info.host
                : sprintf("%s:%d", h3_url_info.host, h3_url_info.port);
        }

        self.checkCapacity(max_streams);
        self.beginSubmit();

        Socket sock = self.getSocket();
        int stream_id;
        int retries = 0;
        try {
            while (True) {
                try {
                    stream_id = sock.submitQuicRequest(method, path, req_headers, body);
                    break;
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "QUIC-ERROR" && ex.desc =~ /STREAM_ID_BLOCKED/
                            && retries < MaxStreamBlockedRetries) {
                        ++retries;
                        int backoff_val = 1 << (retries - 1);
                        self.waitStreamCapacity(backoff_val);
                        if (!self.isReady()) {
                            throw "HTTP3-STATE-ERROR",
                                sprintf("connection closed during STREAM_ID_BLOCKED "
                                    "retry (state: %y)", self.getState());
                        }
                        continue;
                    }
                    if (ex.err == "QUIC-ERROR" && ex.desc =~ /STREAM_ID_BLOCKED/) {
                        int captured_active = self.getActiveStreamCount();
                        if (!max_streams || captured_active < max_streams) {
                            self.setQuicStreamLimit(captured_active);
                        }
                        throw "HTTP3-CAPACITY-ERROR",
                            sprintf("QUIC stream limit reached on connection"
                                " (active_streams: %d)", captured_active);
                    }
                    if (ex.err == "QUIC-HTTP3-ERROR" || ex.err == "QUIC-READ-ERROR"
                            || ex.err == "QUIC-RECV-ERROR") {
                        self.setSubmitError(ex.err, ex.desc);
                    }
                    rethrow;
                }
            }
        } catch () {
            self.endSubmitFail();
            rethrow;
        }

        self.endSubmitOk(stream_id);

        # Phase 3: register CompositeAction (PromiseAction + EventNotifierAction)
        *hash<auto> reg_result = self.registerStreamWithPromiseAndNotifier(stream_id, promise, notifier);
        if (reg_result.need_cancel) {
            try {
                int sid = self.getSessionId();
                if (sid) {
                    sock.cancelQuicStream(sid, stream_id);
                }
            } catch () {}
            throw "HTTP3-STATE-ERROR",
                sprintf("connection closed during request submission (state: %y)",
                    self.getState());
        }

        # Deliver buffered responses directly — also signal notifier since the
        # CompositeAction won't fire for data buffered before registration
        if (reg_result.buffered) {
            foreach hash<auto> resp in (reg_result.buffered) {
                resp.protocol = "h3";
                if (resp.end_stream.toBool()) {
                    try { promise.set(resp); } catch () {}
                    notifier.notify();
                    self.cancelStream(stream_id);
                    break;
                }
            }
        }

        return stream_id;
    }

    #! Submits a streaming request with Channel-based response delivery
    /** @param method HTTP method
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param channel Channel for response chunks
        @param max_streams maximum concurrent streams
        @param streaming if True, bidirectional streaming mode
        @return the stream ID
    */
    int submitRequestWithChannel(string method, string path, *hash<auto> headers, *data body,
            Channel channel, int max_streams, bool streaming = False) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot submit request in state: %y", self.getState());
        }

        # Filter pseudo-headers (keep :protocol for RFC 9220 extended CONNECT)
        hash<auto> req_headers = {};
        *string protocol;
        if (headers) {
            foreach string key in (keys headers) {
                if (key == ":protocol") {
                    protocol = headers{key};
                } else if (key[0] != ":") {
                    req_headers{key} = headers{key};
                }
            }
        }
        if (!req_headers.host) {
            req_headers.host = h3_url_info.port == 443
                ? h3_url_info.host
                : sprintf("%s:%d", h3_url_info.host, h3_url_info.port);
        }

        # Pass :protocol through for RFC 9220 extended CONNECT
        if (protocol) {
            req_headers.":protocol" = protocol;
        }

        self.checkCapacity(max_streams);
        self.beginSubmit();

        Socket sock = self.getSocket();
        int stream_id;
        try {
            if (streaming) {
                stream_id = sock.submitQuicRequestStreaming(method, path, req_headers);
            } else {
                stream_id = sock.submitQuicRequest(method, path, req_headers, body);
            }
        } catch () {
            self.endSubmitFail();
            rethrow;
        }

        self.endSubmitOk(stream_id);

        # Register Channel as the completion action
        *hash<auto> reg_result = self.registerStreamWithChannel(stream_id, channel);
        if (reg_result.need_cancel) {
            try {
                int sid = self.getSessionId();
                if (sid) {
                    sock.cancelQuicStream(sid, stream_id);
                }
            } catch () {}
            throw "HTTP3-STATE-ERROR",
                sprintf("connection closed during request submission (state: %y)",
                    self.getState());
        }

        # Deliver any responses that arrived during the submit-to-registration window
        if (reg_result.buffered) {
            foreach hash<auto> resp in (reg_result.buffered) {
                resp.protocol = "h3";
                channel.send(resp);
            }
        }

        return stream_id;
    }

    #! Sends data on a streaming HTTP/3 request
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be NOTHING to send END_STREAM only)
        @param end_stream if True, signals end of stream

        @return 0 on success, 1 if buffer full (backpressure), -1 on error

        @throw HTTP3-STATE-ERROR if not in ready state
    */
    int sendStreamData(int stream_id, *data data, bool end_stream = False) {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot send data in state: %y", self.getState());
        }

        log(LoggerLevel::DEBUG, "sending %d bytes on stream %d (end_stream=%y)",
            data ? data.size() : 0, stream_id, end_stream);

        Socket sock = self.getSocket();
        return sock.sendQuicClientStreamData(stream_id, data, end_stream);
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        # cancelStream notifies the action with executeError (pure C++)
        if (!self.cancelStream(stream_id)) {
            log(LoggerLevel::DEBUG, "cancel ignored: stream %d not found (already completed)",
                stream_id);
            return;
        }

        int sid = self.getSessionId();
        if (sid) {
            Socket sock = self.getSocket();
            try {
                sock.cancelQuicStream(sid, stream_id);
            } catch () {}
        }

        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Initiates QUIC connection migration to a new network path
    migrate() {
        if (!self.isReady()) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot migrate in state: %y", self.getState());
        }

        log(LoggerLevel::DEBUG, "requesting QUIC connection migration");
        self.setMigrationPending();
    }

    #! Internal: creates and configures a UDP socket for QUIC
    /** Called from the constructor initializer list before the C++ base constructor.
    */
    private static Socket createAndConfigureSocket(hash<auto> url_info, *int ssl_verify_mode,
            bool accept_all_certs, *SSLCertificate client_cert, *SSLPrivateKey client_pk) {
        Socket sock();

        # Apply SSL verification settings before connect
        if (accept_all_certs) {
            sock.setSslVerifyMode(SSL_VERIFY_NONE);
        } else if (exists ssl_verify_mode) {
            sock.setSslVerifyMode(ssl_verify_mode);
        }

        # Set client certificate and key for mTLS
        if (client_cert) {
            sock.setCertificate(client_cert);
        }
        if (client_pk) {
            sock.setPrivateKey(client_pk);
        }

        int family = Http3ClientPollOperation::resolveAddressFamily(url_info.host);
        string bind_addr = family == AF_INET6 ? "::" : "0.0.0.0";
        sock.bindINET(bind_addr, "0", True, family, SOCK_DGRAM);

        return sock;
    }

    #! Internal: resolves the address family for a hostname
    private static int resolveAddressFamily(string host) {
        if (host =~ /^\[.*\]$/) {
            return AF_INET6;
        }
        if (host =~ /:/) {
            return AF_INET6;
        }
        try {
            list<hash<auto>> addrs = getaddrinfo(host, NOTHING, AF_UNSPEC, AI_ADDRCONFIG);
            if (addrs) {
                return addrs[0].family;
            }
        } catch () {
            # Fall back to IPv4 if resolution fails
        }
        return AF_INET;
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (h3_logger) {
            call_function_args(\h3_logger.log(), (level, "Http3ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
