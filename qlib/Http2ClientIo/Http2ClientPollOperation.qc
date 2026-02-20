# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientPollOperation.qc Poll operation for HTTP/2 client multiplexing

/** Http2ClientPollOperation.qc Copyright 2026 Qore Technologies, s.r.o.

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
public namespace Http2ClientIo {

#! Poll operation for HTTP/2 client connection multiplexing
/** This poll operation manages an HTTP/2 client connection lifecycle:

    1. Connect and negotiate HTTP/2 via ALPN
    2. Exchange connection preface (SETTINGS frames)
    3. Submit multiple concurrent requests
    4. Read HTTP/2 frames and dispatch responses by stream ID
    5. Handle GOAWAY for graceful shutdown

    This class wraps the low-level Socket HTTP/2 client multiplex poll operations
    to provide a higher-level interface for the connection manager.

    Unlike the server-side Http2PollOperation which processes one request at a time,
    this operation supports multiple concurrent streams on the same connection.

    @par Stream Callbacks
    When a response is complete, the registered callback for that stream ID is
    invoked with the response data. Callbacks are registered via submitRequest().
    After callback dispatch, call continueReading() to resume frame reading
    without recreating the underlying C++ poll operation.

*/
public class Http2ClientPollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_CONNECTING = "connecting";
        const STATE_SSL_UPGRADE = "ssl_upgrade";
        # Note: STATE_PREFACE was removed - the C++ layer handles preface internally
        const STATE_READING = "reading";
        const STATE_WAIT_READ = "wait_read";
        const STATE_CLOSED = "closed";

        #! Maximum consecutive empty reads before closing
        const MAX_EMPTY_READS = 100;
    }

    private {
        #! The client socket
        Socket sock;

        #! Current state
        string state = STATE_CONNECTING;

        #! Current poll operation (connect, ssl, or h2 multiplex)
        AbstractPollOperation current_op;

        #! Pending stream callbacks: stream_id -> callback
        hash<string, code> stream_callbacks;

        #! Pending stream contexts: stream_id -> context hash
        hash<string, hash<auto>> stream_ctxs;

        #! Number of active streams
        int active_stream_count = 0;

        #! Lock for stream callback access (protects stream_callbacks, stream_ctxs, active_stream_count)
        Mutex stream_lock();

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads
        int empty_read_count = 0;

        #! Flag set when a stream callback is dispatched during continuePoll()
        /** Allows callers to skip blocking on Socket::poll() when a response
            was already delivered to a callback during the same continuePoll() call.
        */
        bool callback_dispatched = False;

        #! Target URL info
        hash<auto> url_info;

        #! Whether SSL is required
        bool ssl_required;

        #! Connect timeout
        timeout connect_timeout;

        #! Logger
        *Logger logger;

        #! Callback invoked when connection becomes ready (HTTP/2 handshake complete)
        *code ready_callback;
    }

    #! Creates the poll operation for a new connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *Logger logger,
            *int ssl_verify_mode, bool accept_all_certs = False) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.ssl_required = url_info.protocol == "https";

        sock = new Socket();

        # Configure socket for HTTP/2
        if (ssl_required) {
            # Apply SSL verification options
            if (exists ssl_verify_mode) {
                sock.setSslVerifyMode(ssl_verify_mode);
            }
            if (accept_all_certs) {
                sock.acceptAllCertificates(True);
            }
            # Set ALPN protocols to negotiate HTTP/2
            sock.setAlpnProtocols(("h2",));
        }

        # Start connection
        startConnect();
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http2_client_multiplex";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Always returns False; this is a long-running multiplexed operation
    /** Responses are dispatched via stream callbacks registered with submitRequest(),
        not via the goalReached()/getOutput() pattern.
    */
    bool goalReached() {
        return False;
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        return state == STATE_CLOSED;
    }

    #! Returns True if the connection is ready for requests
    bool isReady() {
        # Capture state once to avoid TOCTOU: the I/O thread can change state between
        # two reads (e.g., WAIT_READ → READING), causing both comparisons to fail
        string s = state;
        return s == STATE_READING || s == STATE_WAIT_READ;
    }

    #! Returns True if a callback was dispatched during the last continuePoll() call, then clears the flag
    /** This allows callers to avoid blocking on Socket::poll() when a response
        was already delivered to a stream callback. Without this check, the caller
        would block for the full poll timeout even though the response is already
        available to the waiting thread.
    */
    bool checkAndClearCallbackDispatched() {
        if (callback_dispatched) {
            callback_dispatched = False;
            return True;
        }
        return False;
    }

    #! Sets a callback to be invoked when the connection becomes ready
    /** The callback is invoked once when the HTTP/2 handshake completes and the
        connection is ready for requests. This allows callers to be notified
        without polling.

        @param callback the callback to invoke (takes no arguments)
    */
    setReadyCallback(code callback) {
        ready_callback = callback;
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
    int getActiveStreamCount() {
        AutoLock al(stream_lock);
        return active_stream_count;
    }

    #! Submits a new request on this connection
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback callback to invoke when response is ready: sub(hash<auto> response, *hash<auto> error)
        @param ctx optional context to pass to callback

        @return the stream ID assigned to this request

        @throw HTTP2-STATE-ERROR if the connection is not ready for requests
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            code callback, *hash<auto> ctx, bool streaming = False) {
        # Capture state once to avoid TOCTOU between isReady() and the error message
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot submit request in state: %y", current_state);
        }

        # Build request headers
        hash<auto> req_headers = headers ?? {};
        if (!req_headers.":method") {
            req_headers.":method" = method;
        }
        if (!req_headers.":path") {
            req_headers.":path" = path;
        }
        if (!req_headers.":scheme") {
            req_headers.":scheme" = ssl_required ? "https" : "http";
        }
        if (!req_headers.":authority") {
            req_headers.":authority" = url_info.port == (ssl_required ? 443 : 80)
                ? url_info.host
                : sprintf("%s:%d", url_info.host, url_info.port);
        }

        # Submit request AND register callback atomically under stream_lock.
        # This prevents a race where the I/O thread processes the response in
        # handleReading() before the callback is registered, which would cause
        # the response to be discarded as an orphaned cancelled-stream response.
        int stream_id;
        {
            AutoLock al(stream_lock);
            stream_id = sock.submitHttp2Request(req_headers, body, streaming);
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

    #! Sends data on a stream
    /** @param stream_id the stream ID to send data on
        @param data the data to send (can be string or binary)
        @param end_stream if True, signals end of stream

        @throw HTTP2-STREAM-ERROR if the stream ID is not found
        @throw HTTP2-STATE-ERROR if not in ready state
    */
    sendStreamData(int stream_id, data data, bool end_stream = False) {
        # Capture state once to avoid TOCTOU between isReady() and the error message
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot send data in state: %y", current_state);
        }

        string sid = string(stream_id);
        # Advisory check for better error messages; nghttp2 handles the actual stream state
        # validation in sendHttp2StreamData() and will raise an error if the stream was
        # cancelled between this check and the send (TOCTOU race handled gracefully)
        {
            AutoLock al(stream_lock);
            if (!stream_callbacks.hasKey(sid)) {
                throw "HTTP2-STREAM-ERROR", sprintf("stream %d not found", stream_id);
            }
        }

        # Convert to binary if needed
        binary bin_data = data.typeCode() == NT_BINARY ? data : binary(data);

        log(LoggerLevel::DEBUG, "sending %d bytes on stream %d (end_stream=%y)",
            bin_data.size(), stream_id, end_stream);

        # Send via the socket's HTTP/2 session
        sock.sendHttp2StreamData(stream_id, bin_data, end_stream);
    }

    #! Cancels a pending request
    /** @param stream_id the stream ID to cancel

        @throw HTTP2-STREAM-ERROR if the stream ID is not found
    */
    cancelRequest(int stream_id) {
        string sid = string(stream_id);
        {
            AutoLock al(stream_lock);
            if (!stream_callbacks.hasKey(sid)) {
                # Stream may have already completed and been removed - not an error
                log(LoggerLevel::DEBUG, "cancel ignored: stream %d not found (already completed)", stream_id);
                return;
            }

            # Send RST_STREAM via the socket's HTTP/2 session
            sock.cancelHttp2Stream(stream_id);

            # Remove callback
            remove stream_callbacks{sid};
            remove stream_ctxs{sid};
            --active_stream_count;
        }

        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
        callback_dispatched = False;
        while (True) {
            switch (state) {
                case STATE_CONNECTING: {
                    return handleConnecting();
                }

                case STATE_SSL_UPGRADE: {
                    return handleSslUpgrade();
                }

                case STATE_READING: {
                    return handleReading();
                }

                case STATE_WAIT_READ: {
                    # Socket has new data available; resume reading
                    log(LoggerLevel::DEBUG, "resuming read after wait");
                    state = STATE_READING;
                    continue;
                }

                case STATE_CLOSED:
                    return NOTHING;
            }
        }
        return NOTHING;
    }

    #! Continues reading without creating a new poll operation
    /** Called after a callback has been dispatched to resume frame reading.
        The same underlying C++ SocketHttp2ClientMultiplexPollOperation continues
        reading frames, preserving the nghttp2 session state.
    */
    continueReading() {
        if (state != STATE_WAIT_READ) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Aborts the operation and closes the connection
    abort() {
        if (current_op) {
            current_op.abort();
            # Clear current_op reference to allow C++ object to be destroyed
            delete current_op;
        }
        state = STATE_CLOSED;

        # Notify all pending streams of error
        hash<auto> err = {
            "err": "HTTP2-ABORT",
            "desc": "connection aborted",
        };
        notifyPendingStreams(err);

        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }

    #! Internal: starts the connection
    private startConnect() {
        string target = sprintf("%s:%d", url_info.host, url_info.port);
        log(LoggerLevel::DEBUG, "connecting to %s (ssl=%y)", target, ssl_required);
        current_op = sock.startPollConnect(target);
        state = STATE_CONNECTING;
    }

    #! Internal: handles connecting state
    private *hash<SocketPollInfo> handleConnecting() {
        if (!current_op) {
            setError("HTTP2-CONNECT-ERROR", "connect operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();
            if (poll_info) {
                return poll_info;
            }

            if (current_op.goalReached()) {
                log(LoggerLevel::DEBUG, "TCP connection established");
                if (ssl_required) {
                    startSslUpgrade();
                } else {
                    startMultiplex();
                }
                return continuePoll();
            } else {
                setError("HTTP2-CONNECT-ERROR", "connection failed");
                return NOTHING;
            }
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: starts SSL upgrade
    private startSslUpgrade() {
        log(LoggerLevel::DEBUG, "starting SSL upgrade");
        current_op = sock.startPollUpgradeClientToSSL();
        state = STATE_SSL_UPGRADE;
    }

    #! Internal: handles SSL upgrade state
    private *hash<SocketPollInfo> handleSslUpgrade() {
        if (!current_op) {
            setError("HTTP2-SSL-ERROR", "SSL operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();
            if (poll_info) {
                return poll_info;
            }

            if (current_op.goalReached()) {
                # Verify HTTP/2 was negotiated
                *string alpn = sock.getAlpnProtocol();
                if (alpn != "h2") {
                    setError("HTTP2-ALPN-ERROR",
                        sprintf("HTTP/2 not negotiated via ALPN: got %y", alpn));
                    return NOTHING;
                }
                log(LoggerLevel::DEBUG, "SSL connection established, ALPN=%y", alpn);
                startMultiplex();
                return continuePoll();
            } else {
                setError("HTTP2-SSL-ERROR", "SSL upgrade failed");
                return NOTHING;
            }
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: starts the HTTP/2 multiplex operation
    #! The C++ layer handles the HTTP/2 preface exchange internally
    private startMultiplex() {
        log(LoggerLevel::DEBUG, "starting HTTP/2 client multiplex operation");
        current_op = sock.startPollHttp2ClientMultiplex();
        # Go directly to READING state - the C++ operation handles preface internally
        # and allows requests to be submitted even before preface completes
        state = STATE_READING;

        # Notify that the connection is now ready for requests
        if (ready_callback) {
            try {
                ready_callback();
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "ready callback exception: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Internal: handles reading state
    private *hash<SocketPollInfo> handleReading() {
        if (!current_op) {
            setError("HTTP2-READ-ERROR", "read operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();

            # For HTTP/2 client multiplex, the operation is long-running and never
            # sets goalReached=True. Instead, responses are available via getOutput()
            # even when poll_info is returned (indicating more work to do).
            # Check for output BEFORE returning poll_info.
            *hash<auto> output = current_op.getOutput();
            if (output) {
                empty_read_count = 0;

                # Get the stream ID from the response
                int stream_id = output.stream_id;
                string sid = string(stream_id);
                log(LoggerLevel::DEBUG, "response ready: stream_id=%d status=%d",
                    stream_id, output.status_code);

                # Dispatch to callback if registered (get callback under lock)
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
                    # Signal that a callback was dispatched so callers can avoid
                    # blocking on Socket::poll() when the response is already available
                    callback_dispatched = True;

                    # Call callback outside lock to avoid deadlocks
                    try {
                        cb(output + {"ctx": stream_ctx}, NOTHING);
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "stream callback exception: %s: %s",
                            ex.err, ex.desc);
                    }
                    # Callback handled the response - continue reading for more frames
                    state = STATE_WAIT_READ;
                    return <SocketPollInfo>{
                        "events": SOCK_POLLIN,
                        "socket": sock,
                    };
                }

                # No callback found - stream was cancelled after response arrived;
                # discard the orphaned response and continue reading to avoid a
                # livelock where STATE_RESPONSE_READY is never consumed
                log(LoggerLevel::DEBUG, "discarding response for cancelled stream %d", stream_id);
                state = STATE_WAIT_READ;
                return <SocketPollInfo>{
                    "events": SOCK_POLLIN,
                    "socket": sock,
                };
            }

            # No response ready yet - check if we need to wait for more data
            if (poll_info) {
                return poll_info;
            }

            # No poll_info and no output - connection may be processing control frames
            ++empty_read_count;
            if (empty_read_count > MAX_EMPTY_READS) {
                setError("HTTP2-EMPTY-READ",
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
        # Get a copy of callbacks under lock, then notify outside lock
        *hash<string, code> callbacks_copy;
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
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "Http2ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace Http2ClientIo
