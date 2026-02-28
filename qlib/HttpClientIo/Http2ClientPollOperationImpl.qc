# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http2ClientPollOperationImpl.qc HTTP/2 client poll operation implementation

/** Http2ClientPollOperationImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/2 poll operation implementation
/** This poll operation manages an HTTP/2 client connection lifecycle:

    1. Connect and negotiate HTTP/2 via ALPN
    2. Exchange connection preface (SETTINGS frames)
    3. Submit multiple concurrent requests
    4. Read HTTP/2 frames and dispatch responses by stream ID
    5. Handle GOAWAY for graceful shutdown

    Unlike the server-side Http2PollOperation which processes one request at a time,
    this operation supports multiple concurrent streams on the same connection.

    @par Stream Callbacks
    When a response is complete, the registered callback for that stream ID is
    invoked with the response data. Callbacks are registered via submitRequest().
    After callback dispatch, call continueReading() to resume frame reading
    without recreating the underlying C++ poll operation.
*/
public class Http2ClientPollOperation inherits HttpClientPollOperation {
    public {
        #! SSL upgrade state (HTTP/2-specific; QUIC bundles TLS in the handshake)
        const STATE_SSL_UPGRADE = "ssl_upgrade";

        #! Maximum completed outputs to drain per continuePoll() call
        const MAX_DRAIN_ITERATIONS = 100;
    }

    private {
        #! Whether SSL is required
        bool ssl_required;
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
            if (exists ssl_verify_mode) {
                sock.setSslVerifyMode(ssl_verify_mode);
            }
            if (accept_all_certs) {
                sock.acceptAllCertificates(True);
            }
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
    bool goalReached() {
        return False;
    }

    #! Submits a new request on this connection
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback callback to invoke when response is ready
        @param ctx optional context to pass to callback
        @param max_streams maximum concurrent streams allowed on this connection
        @param streaming if True, enable streaming mode for this request

        @return the stream ID assigned to this request

        @throw HTTP2-STATE-ERROR if the connection is not ready for requests
        @throw HTTP2-CAPACITY-ERROR if the connection is at capacity
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            code callback, *hash<auto> ctx, int max_streams, bool streaming = False) {
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

        # Submit request AND register callback atomically under stream_lock
        # Capacity check is also under stream_lock to prevent TOCTOU races
        int stream_id;
        {
            AutoLock al(stream_lock);
            if (active_stream_count >= max_streams) {
                throw "HTTP2-CAPACITY-ERROR",
                    sprintf("connection at capacity: %d/%d streams",
                        active_stream_count, max_streams);
            }
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
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot send data in state: %y", current_state);
        }

        string sid = string(stream_id);
        {
            AutoLock al(stream_lock);
            if (!stream_callbacks.hasKey(sid)) {
                throw "HTTP2-STREAM-ERROR", sprintf("stream %d not found", stream_id);
            }
        }

        binary bin_data = data.typeCode() == NT_BINARY ? data : binary(data);

        log(LoggerLevel::DEBUG, "sending %d bytes on stream %d (end_stream=%y)",
            bin_data.size(), stream_id, end_stream);

        sock.sendHttp2StreamData(stream_id, bin_data, end_stream);
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

            sock.cancelHttp2Stream(stream_id);

            cb = stream_callbacks{sid};
            remove stream_callbacks{sid};
            remove stream_ctxs{sid};
            --active_stream_count;
        }

        if (cb) {
            try {
                cb({}, {"err": "HTTP2-CANCELLED", "desc": "request cancelled"});
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

                case STATE_SSL_UPGRADE: {
                    return handleSslUpgrade();
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
    private startMultiplex() {
        log(LoggerLevel::DEBUG, "starting HTTP/2 client multiplex operation");
        current_op = sock.startPollHttp2ClientMultiplex();
        state = STATE_READING;

        fireReadyCallback();
    }

    #! Internal: handles reading state
    private *hash<SocketPollInfo> handleReading() {
        if (!current_op) {
            setError("HTTP2-READ-ERROR", "read operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();

            # Process ALL available outputs from the C++ layer in a loop
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
                if (exists output.status_code) {
                    log(LoggerLevel::DEBUG, "response ready: stream_id=%d status=%d",
                        stream_id, output.status_code);
                } else {
                    log(LoggerLevel::DEBUG, "streaming data: stream_id=%d bytes=%d",
                        stream_id, output.body.size());
                }

                # Dispatch to callback if registered
                bool is_end = output.end_stream.toBool();
                *code cb;
                *hash<auto> stream_ctx;
                {
                    AutoLock al(stream_lock);
                    if (is_end) {
                        cb = remove stream_callbacks{sid};
                        stream_ctx = remove stream_ctxs{sid};
                        if (cb) {
                            --active_stream_count;
                        }
                    } else {
                        cb = stream_callbacks{sid};
                        stream_ctx = stream_ctxs{sid};
                    }
                    if (cb) {
                        callback_dispatched = True;
                    }
                }
                if (cb) {

                    try {
                        cb(output + {"ctx": stream_ctx}, NOTHING);
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "stream callback exception: %s: %s",
                            ex.err, ex.desc);
                    }
                } else {
                    log(LoggerLevel::DEBUG, "discarding response for cancelled stream %d", stream_id);
                }
            }

            bool dispatched;
            {
                AutoLock al(stream_lock);
                dispatched = callback_dispatched;
            }
            if (dispatched) {
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

    #! Internal: logging helper (override to use protocol-specific prefix)
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "Http2ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
