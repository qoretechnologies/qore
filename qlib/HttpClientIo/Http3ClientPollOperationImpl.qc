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

    @par Stream Callbacks
    When a response is complete, the registered callback for that stream ID is
    invoked with the response data. Callbacks are registered via submitRequest().
*/
public class Http3ClientPollOperation inherits HttpClientPollOperation {
    public {
        #! Maximum completed outputs to drain per continuePoll() call
        const MAX_DRAIN_ITERATIONS = 100;
    }

    private {
        #! Condition signaled when continuePoll() processes packets (may include MAX_STREAMS)
        Condition stream_capacity_cond();

        #! Number of threads waiting on stream_capacity_cond (STREAM_ID_BLOCKED retry)
        int stream_blocked_waiters = 0;

        #! Discovered QUIC stream limit (set when STREAM_ID_BLOCKED is hit);
        #! 0 means unknown/unlimited.  Used by canAcceptStream() via
        #! getQuicStreamLimit() to steer subsequent callers to new connections.
        int quic_stream_limit = 0;

        #! Maximum STREAM_ID_BLOCKED retries before signaling capacity error
        const MaxStreamBlockedRetries = 3;

        #! QUIC session ID (set when connection becomes ready)
        int session_id = 0;

        #! True when a GOAWAY has been received from the server
        bool goaway_received = False;

        #! Set by migrate() on the app thread; consumed by continuePoll() on
        #! the I/O thread so the fd swap happens on the thread that owns epoll
        bool migration_pending = False;
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
            *SSLCertificate client_cert, *SSLPrivateKey client_pk) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;

        sock = new Socket();

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

        # Resolve address family from hostname to support both IPv4 and IPv6
        int family = AF_INET;
        string bind_addr = "0.0.0.0";
        string host = url_info.host;
        if (host =~ /^\[.*\]$/) {
            family = AF_INET6;
            bind_addr = "::";
        } else if (host =~ /:/) {
            family = AF_INET6;
            bind_addr = "::";
        } else {
            try {
                list<hash<auto>> addrs = getaddrinfo(host, NOTHING, AF_UNSPEC, AI_ADDRCONFIG);
                if (addrs) {
                    family = addrs[0].family;
                    bind_addr = family == AF_INET6 ? "::" : "0.0.0.0";
                }
            } catch () {
                # Fall back to IPv4 if resolution fails
            }
        }
        sock.bindINET(bind_addr, "0", True, family, SOCK_DGRAM);

        # Start the QUIC connection
        startConnect();
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientPollOperation objects cannot be copied";
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http3_client_multiplex";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Always returns False; this is a long-running multiplexed operation
    bool goalReached() {
        return False;
    }

    #! Returns True if a GOAWAY has been received from the server
    bool isGoawayReceived() {
        return goaway_received;
    }

    #! Returns the QUIC session ID (0 if not yet connected)
    int getSessionId() {
        return session_id;
    }

    #! Returns the number of active streams (lock-free for performance)
    /** Overrides the base class locked version for the HTTP/3 hot path.
        The int type in Qore is atomic-read safe, and a stale-by-one value
        is acceptable because callers (e.g., \c canAcceptStream()) use this
        as an advisory check; the authoritative capacity check happens inside
        \c submitRequest() under \c stream_lock.
    */
    int getActiveStreamCount() {
        return active_stream_count;
    }

    #! Returns the discovered QUIC stream limit (0 = unknown/unlimited)
    /** Set when STREAM_ID_BLOCKED is encountered.  Used by the connection
        to override \c canAcceptStream() and steer new requests to fresh
        QUIC connections.
    */
    int getQuicStreamLimit() {
        return quic_stream_limit;
    }

    #! Returns True if the underlying QUIC session is still alive
    /** Checks the C++ poll operation state.  When the ngtcp2 session enters
        closing/draining or is destroyed, this returns False.
    */
    bool isQuicSessionOpen() {
        if (!current_op) {
            return False;
        }
        try {
            return current_op.getState() != "closed";
        } catch () {
            return False;
        }
    }

    #! Submits a new HTTP/3 request on this connection
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback callback to invoke when response is ready
        @param ctx optional context to pass to callback
        @param max_streams maximum concurrent streams allowed on this connection

        @return the stream ID assigned to this request

        @throw HTTP3-STATE-ERROR if the connection is not ready for requests
        @throw HTTP3-CAPACITY-ERROR if the connection is at capacity
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback, *hash<auto> ctx, int max_streams) {
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot submit request in state: %y", current_state);
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
            req_headers.host = url_info.port == 443
                ? url_info.host
                : sprintf("%s:%d", url_info.host, url_info.port);
        }

        # Submit with STREAM_ID_BLOCKED retry.  With the shared event loop
        # and packet budget, the I/O thread fairly services all connections,
        # so MAX_STREAMS frames are processed promptly.  Short retries with
        # condition-variable waits handle the common case; if retries are
        # exhausted, HTTP3-CAPACITY-ERROR signals the connection manager to
        # open a new QUIC connection.
        int stream_id;
        {
            AutoLock al(stream_lock);
            if (max_streams && active_stream_count >= max_streams) {
                throw "HTTP3-CAPACITY-ERROR",
                    sprintf("connection at capacity: %d/%d streams",
                        active_stream_count, max_streams);
            }
            int retries = 0;
            while (True) {
                try {
                    stream_id = sock.submitQuicRequest(method, path, req_headers, body);
                    break;
                } catch (hash<ExceptionInfo> ex) {
                    if (ex.err == "QUIC-ERROR" && ex.desc =~ /STREAM_ID_BLOCKED/
                            && retries < MaxStreamBlockedRetries) {
                        ++retries;
                        # Backoff: 1ms, 2ms, 4ms
                        int backoff_val = 1 << (retries - 1);
                        timeout backoff_ms = backoff_val * 1ms;
                        log(LoggerLevel::DEBUG,
                            "STREAM_ID_BLOCKED retry %d/%d (backoff %dms)",
                            retries, MaxStreamBlockedRetries, backoff_val);
                        # Wait on condition — signaled when continuePoll()
                        # drains completed streams and frees stream slots
                        ++stream_blocked_waiters;
                        stream_capacity_cond.wait(stream_lock, backoff_ms);
                        --stream_blocked_waiters;
                        string current_state = state;
                        if (current_state != STATE_READING
                                && current_state != STATE_WAIT_READ) {
                            throw "HTTP3-STATE-ERROR",
                                sprintf("connection closed during STREAM_ID_BLOCKED "
                                    "retry (state: %y)", current_state);
                        }
                        continue;
                    }
                    if (ex.err == "QUIC-ERROR" && ex.desc =~ /STREAM_ID_BLOCKED/) {
                        # Retries exhausted — record the limit so
                        # canAcceptStream() steers subsequent callers
                        # to a new QUIC connection
                        if (!max_streams || active_stream_count < max_streams) {
                            quic_stream_limit = active_stream_count;
                        }
                        log(LoggerLevel::DEBUG,
                            "STREAM_ID_BLOCKED after %d retries — signaling "
                            "capacity error (active: %d, quic_limit: %d)",
                            MaxStreamBlockedRetries, active_stream_count,
                            quic_stream_limit);
                        throw "HTTP3-CAPACITY-ERROR",
                            sprintf("QUIC stream limit reached on connection"
                                " (active_streams: %d)", active_stream_count);
                    }
                    # Connection death errors — mark as closed immediately
                    # so isClosed()/hasError() steer other callers away.
                    # Cannot call setError() here (stream_lock is held),
                    # so set state/error_info directly; the I/O thread will
                    # notify remaining pending streams on its next cycle.
                    if (ex.err == "QUIC-HTTP3-ERROR" || ex.err == "QUIC-READ-ERROR"
                            || ex.err == "QUIC-RECV-ERROR") {
                        error_info = {"err": ex.err, "desc": ex.desc};
                        state = STATE_CLOSED;
                        if (stream_blocked_waiters > 0) {
                            stream_capacity_cond.broadcast();
                        }
                    }
                    rethrow;
                }
            }
            string sid = string(stream_id);
            stream_callbacks{sid} = callback;
            if (ctx) {
                stream_ctxs{sid} = ctx;
            }
            ++active_stream_count;
        }

        return stream_id;
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        string sid = string(stream_id);
        *HttpResponseCallback cb;
        {
            AutoLock al(stream_lock);
            if (!stream_callbacks.hasKey(sid)) {
                log(LoggerLevel::DEBUG, "cancel ignored: stream %d not found (already completed)", stream_id);
                return;
            }

            if (session_id) {
                sock.cancelQuicStream(session_id, stream_id);
            } else {
                log(LoggerLevel::DEBUG, "cancel stream %d: no session yet, skipping QUIC cancel", stream_id);
            }

            cb = stream_callbacks{sid};
            remove stream_callbacks{sid};
            remove stream_ctxs{sid};
            --active_stream_count;
            # Wake one thread waiting in STREAM_ID_BLOCKED retry — one stream slot freed
            if (stream_blocked_waiters > 0) {
                stream_capacity_cond.signal();
            }
        }

        if (cb) {
            try {
                cb({}, {"err": "HTTP3-CANCELLED", "desc": "request cancelled"});
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "cancellation callback threw: %s: %s", ex.err, ex.desc);
            }
        }

        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Continues the poll operation
    *hash<SocketPollInfo> continuePoll() {
        {
            AutoLock al(stream_lock);
            callback_dispatched = False;
        }
        while (True) {
            switch (state) {
                case STATE_CONNECTING: {
                    return handleConnecting();
                }

                case STATE_READING: {
                    return handleReading();
                }

                case STATE_WAIT_READ: {
                    log(LoggerLevel::DEBUG, "resuming read after wait");
                    state = STATE_READING;
                    continue;
                }

                case STATE_CLOSED:
                    return NOTHING;
            }
        }
    }

    #! Initiates QUIC connection migration to a new network path
    migrate() {
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot migrate in state: %y", current_state);
        }

        # Set flag for the I/O thread to perform the migration on the next
        # continuePoll() cycle.  This ensures the fd swap happens on the same
        # thread that owns the epoll registration, avoiding races where the
        # old fd is closed while epoll is still using it.
        log(LoggerLevel::DEBUG, "requesting QUIC connection migration");
        migration_pending = True;
    }

    #! Aborts the operation and closes the connection
    /** @note Sets state to CLOSED first so any concurrent driver thread will
        see the closed state on its next iteration. Does NOT delete current_op
        to avoid use-after-free if a driver thread is still inside continuePoll().
        The current_op member is cleaned up when the poll operation is destroyed.
    */
    abort() {
        state = STATE_CLOSED;
        if (current_op) {
            try { current_op.abort(); } catch () {}
        }
        notifyPendingStreams({"err": "HTTP3-ABORT", "desc": "connection aborted"});
        # Wake any threads blocked in STREAM_ID_BLOCKED retry
        {
            AutoLock al(stream_lock);
            stream_capacity_cond.broadcast();
        }
        try { sock.close(); } catch () {}
    }

    #! Internal: starts the QUIC connection
    private startConnect() {
        log(LoggerLevel::DEBUG, "connecting to %s:%d via QUIC", url_info.host, url_info.port);
        current_op = sock.startPollQuicConnect(url_info.host, url_info.port);
        state = STATE_CONNECTING;
    }

    #! Internal: handles connecting state (QUIC handshake + HTTP/3 setup)
    private *hash<SocketPollInfo> handleConnecting() {
        if (!current_op) {
            setError("HTTP3-CONNECT-ERROR", "connect operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();

            if (current_op.getState() == "reading") {
                log(LoggerLevel::DEBUG, "QUIC connection established, HTTP/3 ready");
                state = STATE_READING;
                session_id = sock.getQuicSessionId();

                fireReadyCallback();

                return poll_info;
            }

            if (poll_info) {
                return poll_info;
            }

            setError("HTTP3-CONNECT-ERROR",
                sprintf("QUIC connection failed (state: %y)", current_op.getState()));
            return NOTHING;
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: handles reading state (process HTTP/3 responses)
    private *hash<SocketPollInfo> handleReading() {
        if (!current_op) {
            setError("HTTP3-READ-ERROR", "read operation not initialized");
            return NOTHING;
        }

        # Perform deferred migration on the I/O thread (set by migrate())
        if (migration_pending) {
            migration_pending = False;
            try {
                current_op.migrateQuicConnection();
                log(LoggerLevel::DEBUG, "QUIC migration completed on I/O thread");
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "QUIC migration failed: %s: %s", ex.err, ex.desc);
            }
        }

        try {
            auto poll_info = current_op.continuePoll();

            # Check for GOAWAY from server
            if (!goaway_received && session_id && sock.isQuicGoawayReceived(session_id)) {
                goaway_received = True;
                log(LoggerLevel::DEBUG, "GOAWAY received from server");
            }

            # Drain ALL completed responses
            int drain_count = 0;
            while (True) {
                *hash<auto> output = current_op.getOutput();
                if (!output) {
                    break;
                }
                if (++drain_count > MAX_DRAIN_ITERATIONS) {
                    log(LoggerLevel::ERROR, "output drain safety limit reached (%d)", drain_count);
                    break;
                }
                empty_read_count = 0;

                int stream_id = output.stream_id;
                string sid = string(stream_id);
                *HttpResponseCallback cb;
                *hash<auto> stream_ctx;
                {
                    AutoLock al(stream_lock);
                    cb = stream_callbacks{sid};
                    stream_ctx = stream_ctxs{sid};
                    if (cb) {
                        callback_dispatched = True;
                    }
                }
                if (cb) {
                    try {
                        cb(output + {"ctx": stream_ctx, "end_stream": True}, NOTHING);
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "stream callback exception: %s: %s",
                            ex.err, ex.desc);
                    }
                } else {
                    log(LoggerLevel::DEBUG, "discarding response for cancelled stream %d",
                        stream_id);
                }
                # Remove stream AFTER callback completes so the stream handle's
                # state is already updated before the stream becomes unavailable
                if (cb) {
                    AutoLock al(stream_lock);
                    remove stream_callbacks{sid};
                    remove stream_ctxs{sid};
                    --active_stream_count;
                }
            }

            # Signal threads waiting on STREAM_ID_BLOCKED after draining
            # responses and decrementing active_stream_count
            if (drain_count > 0 && stream_blocked_waiters > 0) {
                AutoLock al(stream_lock);
                stream_capacity_cond.broadcast();
            }

            bool dispatched;
            {
                AutoLock al(stream_lock);
                dispatched = callback_dispatched;
            }
            if (dispatched) {
                state = STATE_WAIT_READ;
                # Use C++ events when available — they include SOCK_POLLOUT if there
                # are pending sends.  Fall back to SOCK_POLLIN when C++ returned null.
                return poll_info ?? <SocketPollInfo>{
                    "events": SOCK_POLLIN,
                    "socket": sock,
                };
            }

            if (poll_info) {
                # Valid I/O activity even if no response output — reset counter
                empty_read_count = 0;
                return poll_info;
            }

            ++empty_read_count;
            if (empty_read_count > MAX_EMPTY_READS) {
                setError("HTTP3-EMPTY-READ",
                    sprintf("max consecutive empty reads (%d) exceeded", MAX_EMPTY_READS));
                return NOTHING;
            }

            state = STATE_WAIT_READ;
            return <SocketPollInfo>{
                "events": SOCK_POLLIN,
                "socket": sock,
            };
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: logging helper (override to use protocol-specific prefix)
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "Http3ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
