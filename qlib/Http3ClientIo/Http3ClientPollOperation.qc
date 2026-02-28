# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http3ClientPollOperation.qc Poll operation for HTTP/3 client multiplexing over QUIC

/** Http3ClientPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace Http3ClientIo {

#! Poll operation for HTTP/3 client connection multiplexing over QUIC
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
public class Http3ClientPollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_CONNECTING = "connecting";
        const STATE_READING = "reading";
        const STATE_WAIT_READ = "wait_read";
        const STATE_CLOSED = "closed";

        #! Maximum consecutive empty reads before closing
        const MAX_EMPTY_READS = 100;

        #! Maximum completed outputs to drain per continuePoll() call
        const MAX_DRAIN_ITERATIONS = 100;
    }

    private {
        #! The client socket (UDP)
        Socket sock;

        #! Current state (single-writer: only modified by continuePoll/handleReading on I/O thread;
        #! reads from other threads see a consistent value because Qore string assignment
        #! is an atomic reference-counted pointer swap — the reader always sees either
        #! the old or new value, never a torn read)
        string state = STATE_CONNECTING;

        #! Current poll operation (the C++ SocketQuicClientPollOperation)
        AbstractPollOperation current_op;

        #! Pending stream callbacks: stream_id -> callback
        hash<string, code<auto(hash<auto>, *hash<auto>)>> stream_callbacks;

        #! Pending stream contexts: stream_id -> context hash
        hash<string, hash<auto>> stream_ctxs;

        #! Number of active streams (atomic: read without lock for fast-path checks,
        #! always written under stream_lock)
        int active_stream_count = 0;

        #! Lock for stream callback access
        Mutex stream_lock();

        #! Condition signaled when continuePoll() processes packets (may include MAX_STREAMS)
        Condition stream_capacity_cond();

        #! Number of threads waiting on stream_capacity_cond (STREAM_ID_BLOCKED retry);
        #! atomic reads avoid acquiring stream_lock on every continuePoll() when no
        #! threads are blocked
        int stream_blocked_waiters = 0;

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads
        int empty_read_count = 0;

        #! Flag set when a stream callback is dispatched during continuePoll()
        #! Threading model: written by continuePoll() on the I/O thread, read by
        #! checkAndClearCallbackDispatched() from the drivePoll() caller after
        #! continuePoll() returns; Qore's atomic bool assignment makes this safe
        bool callback_dispatched = False;

        #! Target URL info
        hash<auto> url_info;

        #! Connect timeout (enforced by driveToReady() in Http3ClientConnection,
        #! NOT by this poll operation directly — the controller drives the poll loop
        #! with a deadline computed from this value)
        timeout connect_timeout;

        #! Logger
        *Logger logger;

        #! Callback invoked when connection becomes ready
        *code<auto()> ready_callback;

        #! QUIC session ID (set when connection becomes ready)
        int session_id = 0;

        #! True when an async controller drives the I/O loop (continuePoll calls)
        #! Set by Http3ClientConnection.setControllerRef() via setControllerDriven()
        bool controller_driven = False;

        #! True when a GOAWAY has been received from the server
        bool goaway_received = False;
    }

    #! Creates the poll operation for a new QUIC connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode optional SSL certificate verification mode
        @param accept_all_certs if True, accept all server certificates (sets SSL_VERIFY_NONE)
        @param client_cert optional client certificate for mutual TLS (mTLS)
        @param client_pk optional client private key for mutual TLS (mTLS)
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *Logger logger,
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
            # Bracketed IPv6 literal
            family = AF_INET6;
            bind_addr = "::";
        } else if (host =~ /:/) {
            # IPv6 address (contains colons)
            family = AF_INET6;
            bind_addr = "::";
        } else {
            # Try DNS resolution to detect IPv6-only hosts
            try {
                list<hash<auto>> addrs = getaddrinfo(host, NOTHING, AF_UNSPEC, AI_ADDRCONFIG);
                if (addrs) {
                    family = addrs[0].family;
                    bind_addr = family == AF_INET6 ? "::" : "0.0.0.0";
                }
            } catch () {
                # Fall back to IPv4 if resolution fails; connect will report the real error
            }
        }
        sock.bindINET(bind_addr, "0", True, family, SOCK_DGRAM);

        # Start the QUIC connection (handshake + HTTP/3 setup)
        startConnect();
    }

    #! Copying is not supported (holds mutable state: sockets, poll operations, stream callbacks)
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

    #! Returns True if the connection is closed
    /** @note Safe to call from any thread; takes a snapshot of the atomically-assigned state string.
    */
    bool isClosed() {
        string s = state;
        return s == STATE_CLOSED;
    }

    #! Returns True if the connection is ready for requests
    bool isReady() {
        string s = state;
        return s == STATE_READING || s == STATE_WAIT_READ;
    }

    #! Returns True if a GOAWAY has been received from the server
    /** @note Safe to call from any thread; takes a snapshot of the atomically-assigned bool.
    */
    bool isGoawayReceived() {
        return goaway_received;
    }

    #! Returns True if a callback was dispatched during the last continuePoll() call
    /** @note Thread safety: this is a non-atomic read-modify-write.  It is safe
        because only the single driving thread (protected by Http3ClientConnection's
        \c driving flag) calls both continuePoll() (which sets the flag) and this
        method (which reads and clears it).  Do NOT call from multiple threads.
    */
    bool checkAndClearCallbackDispatched() {
        if (callback_dispatched) {
            callback_dispatched = False;
            return True;
        }
        return False;
    }

    #! Sets a callback to be invoked when the connection becomes ready
    setReadyCallback(code<auto()> callback) {
        ready_callback = callback;
    }

    #! Marks this operation as driven by an async controller
    /** When controller-driven, the retry loop in submitRequest() must NOT call
        continuePoll() because the controller thread is the single writer that
        drives I/O.  Instead it waits on stream_capacity_cond for the controller
        to process MAX_STREAMS frames.
    */
    setControllerDriven(bool flag = True) {
        controller_driven = flag;
    }

    #! Returns True if an error occurred
    bool hasError() {
        return exists error_info;
    }

    #! Returns error information if an error occurred
    *hash<auto> getError() {
        return error_info;
    }

    #! Returns the socket
    Socket getSocket() {
        return sock;
    }

    #! Returns the number of active streams
    /** @note Lock-free read; the value is always written under stream_lock
        and Qore int assignment is an atomic 64-bit store, so the reader
        sees either the old or the new value — never a torn read.
    */
    int getActiveStreamCount() {
        return active_stream_count;
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
        @throw HTTP3-CAPACITY-ERROR if the connection is at capacity (checked atomically
        with the C++ submission under stream_lock)

        @par Thread Safety
        In controller-driven mode (setControllerDriven()), multiple threads may call
        submitRequest() concurrently — the stream_lock serializes submissions and the
        controller thread drives I/O exclusively.

        In non-controller (sync) mode, submitRequest() must NOT be called concurrently
        from multiple threads because the STREAM_ID_BLOCKED retry path calls
        continuePoll() directly, which has a single-writer assumption.
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            code<auto(hash<auto>, *hash<auto>)> callback, *hash<auto> ctx, int max_streams) {
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot submit request in state: %y", current_state);
        }

        # Pass only regular headers to the C++ layer.
        # The C++ submitRequest() handles pseudo-headers (:method, :path, :scheme,
        # :authority) from the method/path parameters and the stored host/port.
        # Passing pseudo-headers here would cause duplicates → ERR_MALFORMED_HTTP_HEADER.
        hash<auto> req_headers = {};
        if (headers) {
            foreach string key in (keys headers) {
                if (key[0] != ":") {
                    req_headers{key} = headers{key};
                }
            }
        }
        # Pass "host" header so the C++ layer can use it for :authority
        if (!req_headers.host) {
            req_headers.host = url_info.port == 443
                ? url_info.host
                : sprintf("%s:%d", url_info.host, url_info.port);
        }

        # Submit request AND register callback atomically under stream_lock.
        # The capacity check is inside the lock to eliminate the TOCTOU race
        # between canAcceptStream() (optimistic hint) and actual submission.
        #
        # Retry on STREAM_ID_BLOCKED: the server may not have extended max streams
        # yet.  Uses exponential backoff: 1ms, 2ms, ..., 512ms (total ~1s); wakes
        # early when continuePoll() signals stream_capacity_cond (MAX_STREAMS
        # processed).  The retry limit (10) prevents infinite loops while allowing
        # reasonable recovery from temporary stream ID exhaustion.  Additional
        # safeguards: the connection state check after each wait detects connection
        # closure during retry, and the capacity re-check prevents oversubscription
        # when multiple threads compete for newly available stream slots.
        int stream_id;
        {
            AutoLock al(stream_lock);
            if (active_stream_count >= max_streams) {
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
                            && retries < 10) {
                        ++retries;
                        int backoff_val = 1 << (retries - 1);
                        timeout backoff_ms = backoff_val * 1ms;
                        log(LoggerLevel::DEBUG, "STREAM_ID_BLOCKED retry %d/10 (backoff %dms)",
                            retries, backoff_val);
                        # Release lock before I/O or waiting
                        delete al;
                        if (!controller_driven) {
                            # Sync (non-controller) mode: the calling thread IS the
                            # I/O driver, so drive I/O to process packets that may
                            # contain MAX_STREAMS frames.  Just waiting would deadlock.
                            try {
                                continuePoll();
                            } catch () {
                                # Ignore errors; will retry submitRequest below
                            }
                        }
                        al = new AutoLock(stream_lock);
                        # Wait for the I/O thread (controller or self) to signal
                        # that stream capacity may have changed
                        ++stream_blocked_waiters;
                        stream_capacity_cond.wait(stream_lock, backoff_ms);
                        --stream_blocked_waiters;
                        # Re-check connection state — the connection may have
                        # closed during the unlock window (I/O or wait)
                        string current_state = state;
                        if (current_state != STATE_READING
                                && current_state != STATE_WAIT_READ) {
                            throw "HTTP3-STATE-ERROR",
                                sprintf("connection closed during STREAM_ID_BLOCKED "
                                    "retry (state: %y)", current_state);
                        }
                        # Re-check capacity after re-acquiring lock — another
                        # thread may have consumed the newly available stream slot
                        if (active_stream_count >= max_streams) {
                            throw "HTTP3-CAPACITY-ERROR",
                                sprintf("connection at capacity after retry: %d/%d streams",
                                    active_stream_count, max_streams);
                        }
                        continue;
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

        log(LoggerLevel::DEBUG, "submitted request: method=%y path=%y stream_id=%d", method, path, stream_id);

        return stream_id;
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        string sid = string(stream_id);
        *code cb;
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
        }

        # Notify the callback of cancellation so the caller knows the request was cancelled
        if (cb) {
            try {
                cb(NOTHING, {"err": "HTTP3-CANCELLED", "desc": "request cancelled"});
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::DEBUG, "cancellation callback threw: %s: %s", ex.err, ex.desc);
            }
        }

        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Continues the poll operation
    *hash<SocketPollInfo> continuePoll() {
        callback_dispatched = False;
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

    #! Continues reading without creating a new poll operation
    continueReading() {
        if (state != STATE_WAIT_READ) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Initiates QUIC connection migration to a new network path
    /** Creates a new connected UDP socket, migrates the QUIC session to the new path,
        and swaps the underlying file descriptor. Path validation proceeds in the
        background; if it fails, ngtcp2 automatically falls back to the previous path.

        @throw HTTP3-STATE-ERROR if the connection is not ready
        @throw QUIC-MIGRATION-ERROR if migration fails

        @since %Qore 2.3
    */
    migrate() {
        # State check without stream_lock: safe because the caller
        # (Http3ClientConnection::migrate()) holds driving=True, which prevents
        # concurrent continuePoll() calls that could change state.  The state
        # field uses Qore's atomic string assignment, so the snapshot is consistent.
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP3-STATE-ERROR", sprintf("cannot migrate in state: %y", current_state);
        }

        log(LoggerLevel::DEBUG, "initiating QUIC connection migration");
        current_op.migrateQuicConnection();
        log(LoggerLevel::DEBUG, "QUIC connection migration initiated successfully");
    }

    #! Aborts the operation and closes the connection
    abort() {
        if (current_op) {
            try { current_op.abort(); } catch () {}
            delete current_op;
        }
        # Set state before notifying so callbacks see the connection as closed
        state = STATE_CLOSED;
        notifyPendingStreams({"err": "HTTP3-ABORT", "desc": "connection aborted"});
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

            # QUIC is a long-running multiplexed operation: continuePoll() always
            # returns poll info (never NOTHING) and goalReached() always returns
            # False.  We detect handshake completion by checking the C++ op's
            # state transition to "reading".
            if (current_op.getState() == "reading") {
                log(LoggerLevel::DEBUG, "QUIC connection established, HTTP/3 ready");
                state = STATE_READING;
                session_id = sock.getQuicSessionId();

                # Notify that the connection is now ready
                if (ready_callback) {
                    try {
                        ready_callback();
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "ready callback exception: %s: %s", ex.err, ex.desc);
                    }
                }

                # Return the poll info from the now-reading C++ op
                return poll_info;
            }

            if (poll_info) {
                return poll_info;
            }

            # continuePoll() returned NOTHING and state is not "reading" = error
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

        try {
            auto poll_info = current_op.continuePoll();

            # Check for GOAWAY from server after processing packets
            # Uses lightweight bool check to avoid hash allocation on every I/O cycle
            if (!goaway_received && session_id && sock.isQuicGoawayReceived(session_id)) {
                goaway_received = True;
                log(LoggerLevel::DEBUG, "GOAWAY received from server");
            }

            # Signal threads waiting on STREAM_ID_BLOCKED only when there are
            # actually waiters — avoids acquiring stream_lock on every
            # continuePoll() call in the common (no STREAM_ID_BLOCKED) case
            if (stream_blocked_waiters > 0) {
                AutoLock al(stream_lock);
                stream_capacity_cond.broadcast();
            }

            # Drain ALL completed responses from the C++ layer — a single
            # continuePoll() may process a packet containing multiple streams'
            # final frames.  Without this loop, buffered responses would hang
            # until the next socket event (matching the HTTP/2 drain pattern).
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
                log(LoggerLevel::DEBUG, "response ready: stream_id=%d status=%d",
                    stream_id, output.status_code);

                # Dispatch to callback
                *code cb;
                *hash<auto> stream_ctx;
                {
                    AutoLock al(stream_lock);
                    cb = remove stream_callbacks{sid};
                    stream_ctx = remove stream_ctxs{sid};
                    if (cb) {
                        --active_stream_count;
                    }
                }
                if (cb) {
                    callback_dispatched = True;
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
            }

            if (callback_dispatched) {
                state = STATE_WAIT_READ;
                return <SocketPollInfo>{
                    "events": SOCK_POLLIN,
                    "socket": sock,
                };
            }

            if (poll_info) {
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

    #! Internal: sets error state and notifies pending streams
    private setError(string err, string desc) {
        error_info = {
            "err": err,
            "desc": desc,
        };
        state = STATE_CLOSED;
        log(LoggerLevel::ERROR, "connection error: %s: %s", err, desc);
        notifyPendingStreams(error_info);
    }

    #! Internal: notifies all pending streams of an error
    private notifyPendingStreams(hash<auto> err) {
        *hash<string, code<auto(hash<auto>, *hash<auto>)>> callbacks_copy;
        *hash<string, hash<auto>> ctxs_copy;
        {
            AutoLock al(stream_lock);
            callbacks_copy = stream_callbacks;
            ctxs_copy = stream_ctxs;
            remove stream_callbacks;
            remove stream_ctxs;
            active_stream_count = 0;
        }
        foreach string sid in (keys callbacks_copy) {
            code cb = callbacks_copy{sid};
            *hash<auto> stream_ctx = ctxs_copy{sid};
            try {
                cb(NOTHING, err + {"stream_id": sid.toInt(), "ctx": stream_ctx});
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "stream error callback exception: %s: %s",
                    ex.err, ex.desc);
            }
        }
    }

    #! Internal: logging helper
    private log(int level, string fmt, ...) {
        if (logger) {
            call_function_args(\logger.log(), (level, "Http3ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace Http3ClientIo
