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

    @par continueReading() Pattern
    After dispatching a response, call continueReading() to resume frame reading
    without recreating the underlying C++ poll operation.  This preserves the
    nghttp2 session state (including any already-received frames for other streams).

*/
public class Http2ClientPollOperation inherits AbstractPollOperation {
    public {
        #! State constants
        const STATE_CONNECTING = "connecting";
        const STATE_SSL_UPGRADE = "ssl_upgrade";
        # Note: STATE_PREFACE was removed - the C++ layer handles preface internally
        const STATE_READING = "reading";
        const STATE_WAIT_READ = "wait_read";
        const STATE_RESPONSE_READY = "response_ready";
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

        #! The completed response when ready (from last stream completion)
        *hash<auto> completed_response;

        #! Stream ID of the completed response
        *int completed_stream_id;

        #! Pending stream callbacks: stream_id -> callback
        hash<string, code> stream_callbacks;

        #! Pending stream contexts: stream_id -> context hash
        hash<string, hash<auto>> stream_ctxs;

        #! Number of active streams
        int active_stream_count = 0;

        #! Error info if an error occurred
        *hash<auto> error_info;

        #! Counter for consecutive empty reads
        int empty_read_count = 0;

        #! Target URL info
        hash<auto> url_info;

        #! Whether SSL is required
        bool ssl_required;

        #! Connect timeout
        timeout connect_timeout;

        #! Logger
        *Logger logger;
    }

    #! Creates the poll operation for a new connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *Logger logger) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.ssl_required = url_info.protocol == "https";

        sock = new Socket();

        # Configure socket for HTTP/2
        if (ssl_required) {
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

    #! Returns True when a response is ready to be processed
    bool goalReached() {
        return state == STATE_RESPONSE_READY;
    }

    #! Returns True if the connection is closed
    bool isClosed() {
        return state == STATE_CLOSED;
    }

    #! Returns True if the connection is ready for requests
    bool isReady() {
        return state == STATE_READING || state == STATE_WAIT_READ || state == STATE_RESPONSE_READY;
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
            code callback, *hash<auto> ctx) {
        if (!isReady()) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot submit request in state: %y", state);
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

        # Submit request via the socket's HTTP/2 session
        int stream_id = sock.submitHttp2Request(req_headers, body);
        string sid = string(stream_id);

        # Register callback for this stream
        stream_callbacks{sid} = callback;
        if (ctx) {
            stream_ctxs{sid} = ctx;
        }
        ++active_stream_count;

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
        if (!isReady()) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot send data in state: %y", state);
        }

        string sid = string(stream_id);
        if (!stream_callbacks.hasKey(sid)) {
            throw "HTTP2-STREAM-ERROR", sprintf("stream %d not found", stream_id);
        }

        # Convert to binary if needed
        binary bin_data = data.typeCode() == NT_BINARY ? data : binary(data);

        log(LoggerLevel::DEBUG, "sending %d bytes on stream %d (end_stream=%y)",
            bin_data.size(), stream_id, end_stream);

        # Send via the socket's HTTP/2 session
        sock.sendHttp2StreamData(stream_id, bin_data, end_stream, 0);
    }

    #! Cancels a pending request
    /** @param stream_id the stream ID to cancel

        @throw HTTP2-STREAM-ERROR if the stream ID is not found
    */
    cancelRequest(int stream_id) {
        string sid = string(stream_id);
        if (!stream_callbacks.hasKey(sid)) {
            throw "HTTP2-STREAM-ERROR", sprintf("stream %d not found", stream_id);
        }

        # Send RST_STREAM via the socket's HTTP/2 session
        sock.cancelHttp2Stream(stream_id);

        # Remove callback
        remove stream_callbacks{sid};
        remove stream_ctxs{sid};
        --active_stream_count;

        log(LoggerLevel::DEBUG, "cancelled request: stream_id=%d", stream_id);
    }

    #! Continues the poll operation
    /** @return poll info for the next poll() call, or NOTHING if complete
    */
    *hash<SocketPollInfo> continuePoll() {
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

                case STATE_RESPONSE_READY:
                case STATE_CLOSED:
                    return NOTHING;
            }
        }
        return NOTHING;
    }

    #! Returns the completed response when ready
    auto getOutput() {
        return completed_response;
    }

    #! Returns the stream ID of the completed response
    *int getCompletedStreamId() {
        return completed_stream_id;
    }

    #! Continues reading without creating a new poll operation
    /** Called after a response has been dispatched. The same underlying C++
        SocketHttp2ClientMultiplexPollOperation continues reading frames.
    */
    continueReading() {
        if (state != STATE_RESPONSE_READY) {
            throw "HTTP2-STATE-ERROR", sprintf("cannot continue reading in state: %y", state);
        }
        remove completed_response;
        remove completed_stream_id;
        empty_read_count = 0;
        state = STATE_READING;
    }

    #! Aborts the operation and closes the connection
    abort() {
        if (current_op) {
            current_op.abort();
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
        current_op = sock.startConnect(target);
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
        current_op = sock.startSslConnect();
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
    }

    #! Internal: handles reading state
    private *hash<SocketPollInfo> handleReading() {
        if (!current_op) {
            setError("HTTP2-READ-ERROR", "read operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();
            if (poll_info) {
                return poll_info;
            }

            if (current_op.goalReached()) {
                hash<auto> output = current_op.getOutput();
                if (output) {
                    empty_read_count = 0;

                    # Get the stream ID from the response
                    int stream_id = output.stream_id;
                    string sid = string(stream_id);
                    log(LoggerLevel::DEBUG, "response ready: stream_id=%d status=%d",
                        stream_id, output.status_code);

                    # Dispatch to callback if registered
                    *code cb = stream_callbacks{sid};
                    if (cb) {
                        *hash<auto> stream_ctx = stream_ctxs{sid};
                        remove stream_callbacks{sid};
                        remove stream_ctxs{sid};
                        --active_stream_count;

                        # Call callback with response
                        try {
                            cb(output + {"ctx": stream_ctx}, NOTHING);
                        } catch (hash<ExceptionInfo> ex) {
                            log(LoggerLevel::ERROR, "stream callback exception: %s: %s",
                                ex.err, ex.desc);
                        }
                    }

                    # Store for getOutput()
                    completed_response = output;
                    completed_stream_id = stream_id;
                    state = STATE_RESPONSE_READY;
                    return NOTHING;
                }

                # goalReached but no response: control frames processed
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
            } else {
                setError("HTTP2-READ-ERROR", "read operation failed");
                return NOTHING;
            }
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
        foreach string sid in (keys stream_callbacks) {
            code cb = stream_callbacks{sid};
            *hash<auto> stream_ctx = stream_ctxs{sid};
            try {
                cb(NOTHING, err + {"stream_id": sid.toInt(), "ctx": stream_ctx});
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "stream error callback exception: %s: %s",
                    ex.err, ex.desc);
            }
        }
        remove stream_callbacks;
        remove stream_ctxs;
        active_stream_count = 0;
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            logger.log(level, "Http2ClientPollOperation: " + fmt, argv);
        }
    }
}

} # namespace Http2ClientIo
