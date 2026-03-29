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
        #! Proxy CONNECT send state
        const STATE_PROXY_CONNECT_SEND = "proxy_connect_send";
        #! Proxy CONNECT recv state
        const STATE_PROXY_CONNECT_RECV = "proxy_connect_recv";

        #! Maximum completed outputs to drain per continuePoll() call
        const MAX_DRAIN_ITERATIONS = 100;

        #! Maximum empty reads during h2c probe before declaring failure
        /** Much lower than MAX_EMPTY_READS to quickly detect servers that
            don't support HTTP/2 prior knowledge on plain HTTP connections.
        */
        const H2C_PROBE_MAX_EMPTY_READS = 5;
    }

    private {
        #! Whether SSL is required
        bool ssl_required;

        #! Proxy info (host, port, protocol, ssl)
        *hash<auto> proxy_info;

        #! True when HTTPS target through proxy (requires CONNECT tunnel)
        bool proxy_tunnel = False;

        #! Whether the H2 protocol has been confirmed (via ALPN or SETTINGS exchange)
        /** For HTTPS, H2 is confirmed by ALPN negotiation during the TLS handshake.
            For plain HTTP (h2c prior knowledge), H2 is only confirmed after the
            server responds with valid H2 data (SETTINGS frame). Until confirmed,
            the readyCallback is deferred so that waitForReady() blocks.
        */
        bool h2_confirmed = False;

        #! Number of successful continuePoll() cycles in READING state
        /** Used for h2c confirmation: the first cycle sends our preface (POLLOUT);
            a second successful cycle with poll_info means the server responded
            with valid H2 data.
        */
        int reading_cycle_count = 0;
    }

    #! Creates the poll operation for a new connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *LoggerInterface logger,
            *int ssl_verify_mode, bool accept_all_certs = False,
            *hash<auto> proxy_info) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.ssl_required = url_info.protocol == "https";
        self.proxy_info = proxy_info;

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

    #! Prevent copying — shared socket/mutex/callbacks would corrupt state
    private copy() {
        throw "COPY-ERROR", "Http2ClientPollOperation cannot be copied";
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http2_client_multiplex";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Hook: for plain HTTP (h2c), requires confirmation that the server supports HTTP/2
    /** For HTTPS, h2_confirmed is set during startMultiplex() (ALPN confirmed H2).
        For plain HTTP, h2_confirmed is set when the first valid H2 output is received.
    */
    private bool isReadyImpl() {
        return h2_confirmed;
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
            HttpResponseCallback callback, *hash<auto> ctx, int max_streams, bool streaming = False) {
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
            if (max_streams && active_stream_count >= max_streams) {
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

        binary bin_data = data.typeCode() == NT_BINARY ? data : binary(data);

        log(LoggerLevel::DEBUG, "sending %d bytes on stream %d (end_stream=%y)",
            bin_data.size(), stream_id, end_stream);

        # Send directly via C++ — the stream may still be open for writing even
        # after the server sent END_STREAM (half-closed remote per RFC 7540 §5.1).
        # The stream_callbacks check was removed because callbacks track response
        # delivery, not stream existence.  If the stream doesn't exist at the
        # nghttp2 level, sendHttp2StreamData() will raise an appropriate error.
        sock.sendHttp2StreamData(stream_id, bin_data, end_stream);
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

                case STATE_PROXY_CONNECT_SEND: {
                    return handleProxyConnectSend();
                }

                case STATE_PROXY_CONNECT_RECV: {
                    return handleProxyConnectRecv();
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
        string target;
        if (proxy_info) {
            target = sprintf("%s:%d", proxy_info.host, proxy_info.port);
            proxy_tunnel = ssl_required;
            log(LoggerLevel::DEBUG, "connecting to proxy %s (target=%s:%d ssl=%y tunnel=%y)",
                target, url_info.host, url_info.port, ssl_required, proxy_tunnel);
        } else {
            target = sprintf("%s:%d", url_info.host, url_info.port);
            log(LoggerLevel::DEBUG, "connecting to %s (ssl=%y)", target, ssl_required);
        }
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
                if (proxy_tunnel) {
                    # HTTPS through proxy: send CONNECT request first
                    startProxyConnect();
                } else if (ssl_required) {
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

    #! Internal: builds and sends a CONNECT request to the proxy
    private startProxyConnect() {
        string target = sprintf("%s:%d", url_info.host, url_info.port);
        string connect_req = sprintf("CONNECT %s HTTP/1.1\r\nHost: %s\r\n\r\n", target, target);
        log(LoggerLevel::DEBUG, "sending CONNECT %s to proxy", target);
        current_op = sock.startPollSend(binary(connect_req));
        state = STATE_PROXY_CONNECT_SEND;
    }

    #! Internal: handles proxy CONNECT send state
    private *hash<SocketPollInfo> handleProxyConnectSend() {
        if (!current_op) {
            setError("HTTP2-CONNECT-ERROR", "proxy CONNECT send not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();
            if (poll_info) {
                return poll_info;
            }

            if (current_op.goalReached()) {
                log(LoggerLevel::DEBUG, "proxy CONNECT sent, reading response");
                current_op = sock.startPollReadHttpHeader();
                state = STATE_PROXY_CONNECT_RECV;
                return handleProxyConnectRecv();
            } else {
                setError("HTTP2-CONNECT-ERROR", "proxy CONNECT send failed");
                return NOTHING;
            }
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: handles proxy CONNECT response
    private *hash<SocketPollInfo> handleProxyConnectRecv() {
        if (!current_op) {
            setError("HTTP2-CONNECT-ERROR", "proxy CONNECT recv not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();
            if (poll_info) {
                return poll_info;
            }

            if (current_op.goalReached()) {
                hash<auto> output = current_op.getOutput();
                hash<auto> hdr = output.hdr;
                int status = hdr.status_code;
                if (status < 200 || status >= 300) {
                    setError("HTTP2-CONNECT-ERROR",
                        sprintf("proxy CONNECT failed with HTTP %d: %s", status,
                            hdr.status_message ?? "unknown"));
                    return NOTHING;
                }
                log(LoggerLevel::DEBUG, "proxy CONNECT tunnel established (status=%d)", status);
                # Tunnel established — start TLS+ALPN through the tunnel
                startSslUpgrade();
                return continuePoll();
            } else {
                setError("HTTP2-CONNECT-ERROR", "proxy CONNECT response read failed");
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

        if (ssl_required) {
            # HTTPS: ALPN already confirmed H2 during TLS handshake
            h2_confirmed = True;
            fireReadyCallback();
        }
        # Plain HTTP (h2c): defer readyCallback until we see valid H2 output
        # from the server, confirming it supports HTTP/2 prior knowledge
    }

    #! Internal: handles reading state
    private *hash<SocketPollInfo> handleReading() {
        if (!current_op) {
            setError("HTTP2-READ-ERROR", "read operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();

            # h2c confirmation: after the initial preface send (cycle 0), if we get
            # a successful poll cycle with valid poll_info, the server has responded
            # with valid H2 data (SETTINGS frame).  This confirms h2c support.
            if (!h2_confirmed && poll_info) {
                ++reading_cycle_count;
                if (reading_cycle_count > 1) {
                    h2_confirmed = True;
                    log(LoggerLevel::DEBUG, "h2c confirmed: server supports HTTP/2 prior knowledge");
                    fireReadyCallback();
                }
            }

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
                *HttpResponseCallback cb;
                *hash<auto> stream_ctx;
                {
                    AutoLock al(stream_lock);
                    cb = stream_callbacks{sid};
                    stream_ctx = stream_ctxs{sid};
                    if (cb) {
                        callback_dispatched = True;
                    }
                    # Remove stream BEFORE the callback so that
                    # active_stream_count is already decremented by the time the
                    # callback resolves the caller's Promise — otherwise
                    # request() can return while getActiveStreamCount() still
                    # reports the old value (race in cleanupIdleConnections).
                    # cb and stream_ctx are captured above, so the callback can
                    # still be invoked after removal.
                    if (is_end && cb) {
                        remove stream_callbacks{sid};
                        remove stream_ctxs{sid};
                        --active_stream_count;
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
                # Use C++ events when available — they include SOCK_POLLOUT if there
                # are pending sends (e.g., WINDOW_UPDATE after receiving data).
                # Fall back to SOCK_POLLIN only when C++ returned null (connection closing).
                return poll_info ?? <SocketPollInfo>{
                    "events": SOCK_POLLIN,
                    "socket": sock,
                };
            }

            if (poll_info) {
                # Valid I/O activity (e.g., WINDOW_UPDATE, PING) even if no
                # response output — reset the empty read counter
                empty_read_count = 0;
                return poll_info;
            }

            ++empty_read_count;
            # Use a much shorter limit during h2c probing — if the server
            # doesn't respond with valid H2 frames quickly, it likely doesn't
            # support HTTP/2 prior knowledge
            int max_reads = h2_confirmed ? MAX_EMPTY_READS : H2C_PROBE_MAX_EMPTY_READS;
            if (empty_read_count > max_reads) {
                setError("HTTP2-EMPTY-READ",
                    sprintf("max consecutive empty reads (%d) exceeded%s", max_reads,
                        h2_confirmed ? "" : " (h2c probe failed — server likely doesn't support HTTP/2)"));
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
