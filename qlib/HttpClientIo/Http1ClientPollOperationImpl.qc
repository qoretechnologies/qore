# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file Http1ClientPollOperationImpl.qc HTTP/1.1 client poll operation implementation

/** Http1ClientPollOperationImpl.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! HTTP/1.1 poll operation implementation
/** This poll operation manages an HTTP/1.1 client connection lifecycle:

    1. Connect (optionally upgrading to SSL/TLS)
    2. Send a serialized HTTP/1.1 request
    3. Read and parse the response header
    4. Read the response body (Content-Length, chunked, or connection-close)
    5. Dispatch the response callback and return to idle

    HTTP/1.1 is a serial protocol: only one request is in flight at a time.
    The connection manager creates multiple connections for concurrent requests.

    @par State Machine
    - \c CONNECTING: TCP connect via @ref Qore::Socket::startPollConnect()
    - \c SSL_UPGRADE: TLS handshake via @ref Qore::Socket::startPollUpgradeClientToSSL()
    - \c READING / WAIT_READ: idle or processing a request (with sub-states)
    - \c CLOSED: connection closed or errored

    @par Request Sub-States (within READING)
    - \c idle: no active request
    - \c sending: sending the serialized request
    - \c recv_header: reading the response status line and headers
    - \c recv_body_length: reading a fixed-length body (Content-Length)
    - \c recv_chunk_size: reading a chunk size line (chunked TE)
    - \c recv_chunk_data: reading chunk data
    - \c recv_chunk_crlf: reading the trailing CRLF after the last chunk
    - \c recv_body_close: reading body until the server closes the connection

    @par Thread Safety
    Stream submission (submitRequest) and the I/O loop (continuePoll) are
    synchronized via @ref HttpClientPollOperation::stream_lock.
*/
public class Http1ClientPollOperation inherits HttpClientPollOperation {
    public {
        #! SSL upgrade state
        const STATE_SSL_UPGRADE = "ssl_upgrade";
    }

    private {
        #! Whether SSL is required
        bool ssl_required;

        #! Request sub-state
        string req_state = REQ_IDLE;

        #! Pending request data to send (queued by submitRequest)
        *binary pending_request_data;

        #! Active stream ID (the stream currently being processed)
        *int active_sid;

        #! Active request method (for HEAD / 1xx / 204 / 304 handling)
        *string active_method;

        #! Next stream ID counter (monotonically increasing)
        int next_stream_id = 0;

        #! Response status code
        int response_status_code;

        #! Response headers (lowercase keys)
        *hash<auto> response_headers;

        #! Accumulated response body
        *binary response_body;

        #! Whether the response uses chunked transfer encoding
        bool response_chunked = False;

        #! Content-Length from response headers (-1 = unknown)
        int response_content_length = -1;

        #! Whether the server sent Connection: close
        bool connection_close = False;

        # Request sub-state constants
        const REQ_IDLE = "idle";
        const REQ_SENDING = "sending";
        const REQ_RECV_HEADER = "recv_header";
        const REQ_RECV_BODY_LENGTH = "recv_body_length";
        const REQ_RECV_CHUNK_SIZE = "recv_chunk_size";
        const REQ_RECV_CHUNK_DATA = "recv_chunk_data";
        const REQ_RECV_CHUNK_CRLF = "recv_chunk_crlf";
        const REQ_RECV_BODY_CLOSE = "recv_body_close";
    }

    #! Creates the poll operation for a new HTTP/1.1 connection
    /** @param url_info parsed URL info with host, port, scheme
        @param connect_timeout timeout for connection establishment
        @param logger optional logger for debug output
        @param ssl_verify_mode SSL verification mode (optional)
        @param accept_all_certs if True, accept all SSL certificates including self-signed
    */
    constructor(hash<auto> url_info, timeout connect_timeout = DefaultConnectTimeout, *LoggerInterface logger,
            *int ssl_verify_mode, bool accept_all_certs = False) {
        self.url_info = url_info;
        self.connect_timeout = connect_timeout;
        self.logger = logger;
        self.ssl_required = url_info.protocol == "https";

        sock = new Socket();

        # Configure socket for SSL if needed (no ALPN — HTTP/1.1 is the default)
        if (ssl_required) {
            if (exists ssl_verify_mode) {
                sock.setSslVerifyMode(ssl_verify_mode);
            }
            if (accept_all_certs) {
                sock.acceptAllCertificates(True);
            }
        }

        # Start connection
        startConnect();
    }

    #! Prevent copying — shared socket/mutex/callbacks would corrupt state
    private copy() {
        throw "COPY-ERROR", "Http1ClientPollOperation cannot be copied";
    }

    #! Returns the goal of the operation
    string getGoal() {
        return "http1_client";
    }

    #! Returns the current state
    string getState() {
        return state;
    }

    #! Always returns False; this is a long-running connection operation
    bool goalReached() {
        return False;
    }

    #! Submits a new request on this connection
    /** Builds the HTTP/1.1 wire-format request, assigns a stream ID, and
        queues the request for the I/O loop to send.

        @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback callback to invoke when response is ready
        @param ctx optional context to pass to callback
        @param max_streams maximum concurrent streams (should be 1 for HTTP/1.1)

        @return the stream ID assigned to this request

        @throw HTTP1-STATE-ERROR if the connection is not ready for requests
        @throw HTTP1-CAPACITY-ERROR if the connection already has a request in flight
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback, *hash<auto> ctx, int max_streams) {
        string current_state = state;
        if (current_state != STATE_READING && current_state != STATE_WAIT_READ) {
            throw "HTTP1-STATE-ERROR", sprintf("cannot submit request in state: %y", current_state);
        }

        # Build HTTP/1.1 request line
        string host = url_info.port == (ssl_required ? 443 : 80)
            ? url_info.host
            : sprintf("%s:%d", url_info.host, url_info.port);

        string req_str = sprintf("%s %s HTTP/1.1\r\n", method, path);
        req_str += sprintf("Host: %s\r\n", host);

        # Add request headers (skip HTTP/2 pseudo-headers)
        bool has_content_length = False;
        if (headers) {
            foreach hash<auto> entry in (headers.pairIterator()) {
                if (entry.key[0] == ":") {
                    continue;
                }
                req_str += sprintf("%s: %s\r\n", entry.key, entry.value);
                if (entry.key.lwr() == "content-length") {
                    has_content_length = True;
                }
            }
        }

        # Prepare body and add Content-Length if missing
        *binary body_data;
        if (exists body) {
            body_data = body.typeCode() == NT_BINARY ? body : binary(body);
            if (!has_content_length) {
                req_str += sprintf("Content-Length: %d\r\n", body_data.size());
            }
        }

        req_str += "\r\n";

        # Combine request line + headers + body into a single binary buffer
        binary request_data = binary(req_str);
        if (body_data) {
            request_data += body_data;
        }

        # Assign stream ID and register callback atomically under stream_lock
        int stream_id;
        {
            AutoLock al(stream_lock);
            if (max_streams && active_stream_count >= max_streams) {
                throw "HTTP1-CAPACITY-ERROR",
                    sprintf("connection at capacity: %d/%d streams",
                        active_stream_count, max_streams);
            }
            stream_id = ++next_stream_id;
            string sid = string(stream_id);
            stream_callbacks{sid} = callback;
            if (ctx) {
                stream_ctxs{sid} = ctx;
            }
            ++active_stream_count;
            pending_request_data = request_data;
            active_sid = stream_id;
            active_method = method;
        }

        log(LoggerLevel::DEBUG, "submitted request: method=%y path=%y stream_id=%d (%d bytes)",
            method, path, stream_id, request_data.size());

        return stream_id;
    }

    #! Cancels a pending request
    cancelRequest(int stream_id) {
        string sid = string(stream_id);
        *HttpResponseCallback cb;
        {
            AutoLock al(stream_lock);
            if (!stream_callbacks.hasKey(sid)) {
                log(LoggerLevel::DEBUG, "cancel ignored: stream %d not found (already completed)",
                    stream_id);
                return;
            }

            cb = stream_callbacks{sid};
            remove stream_callbacks{sid};
            remove stream_ctxs{sid};
            --active_stream_count;
            if (active_sid == stream_id) {
                remove active_sid;
                remove active_method;
                remove pending_request_data;
                req_state = REQ_IDLE;
            }
        }

        if (cb) {
            try {
                cb({}, {"err": "HTTP1-CANCELLED", "desc": "request cancelled"});
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
    abort() {
        state = STATE_CLOSED;
        if (current_op) {
            try { current_op.abort(); } catch () {}
        }

        hash<auto> err = {
            "err": "HTTP1-ABORT",
            "desc": "connection aborted",
        };
        notifyPendingStreams(err);

        try {
            sock.close();
        } catch () {
            # Ignore close errors
        }
    }

    #! Internal: starts the TCP connection
    private startConnect() {
        string target = sprintf("%s:%d", url_info.host, url_info.port);
        log(LoggerLevel::DEBUG, "connecting to %s (ssl=%y)", target, ssl_required);
        current_op = sock.startPollConnect(target);
        state = STATE_CONNECTING;
    }

    #! Internal: handles connecting state
    private *hash<SocketPollInfo> handleConnecting() {
        if (!current_op) {
            setError("HTTP1-CONNECT-ERROR", "connect operation not initialized");
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
                    startReady();
                }
                return continuePoll();
            } else {
                setError("HTTP1-CONNECT-ERROR", "connection failed");
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
            setError("HTTP1-SSL-ERROR", "SSL operation not initialized");
            return NOTHING;
        }

        try {
            auto poll_info = current_op.continuePoll();
            if (poll_info) {
                return poll_info;
            }

            if (current_op.goalReached()) {
                log(LoggerLevel::DEBUG, "SSL connection established");
                startReady();
                return continuePoll();
            } else {
                setError("HTTP1-SSL-ERROR", "SSL upgrade failed");
                return NOTHING;
            }
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: transitions to ready state after connection establishment
    private startReady() {
        log(LoggerLevel::DEBUG, "HTTP/1.1 connection ready");
        remove current_op;
        state = STATE_READING;
        req_state = REQ_IDLE;
        fireReadyCallback();
    }

    #! Internal: main reading/processing state handler
    /** Dispatches to the appropriate sub-state handler based on the current
        request processing phase.
    */
    private *hash<SocketPollInfo> handleReading() {
        try {
            switch (req_state) {
                case REQ_IDLE:
                    return handleIdle();
                case REQ_SENDING:
                    return handleSending();
                case REQ_RECV_HEADER:
                    return handleRecvHeader();
                case REQ_RECV_BODY_LENGTH:
                    return handleRecvBodyLength();
                case REQ_RECV_CHUNK_SIZE:
                    return handleRecvChunkSize();
                case REQ_RECV_CHUNK_DATA:
                    return handleRecvChunkData();
                case REQ_RECV_CHUNK_CRLF:
                    return handleRecvChunkCrlf();
                case REQ_RECV_BODY_CLOSE:
                    return handleRecvBodyClose();
            }
            # Should never reach here
            setError("HTTP1-INTERNAL-ERROR", sprintf("unknown request sub-state: %y", req_state));
            return NOTHING;
        } catch (hash<ExceptionInfo> ex) {
            setError(ex.err, ex.desc);
            return NOTHING;
        }
    }

    #! Internal: handles idle sub-state (no active request)
    /** Checks for a pending request to send. If none, polls for POLLIN to
        detect server-initiated connection close.
    */
    private *hash<SocketPollInfo> handleIdle() {
        # Check for pending request
        *binary req_data;
        {
            AutoLock al(stream_lock);
            req_data = pending_request_data;
            remove pending_request_data;
        }

        if (req_data) {
            # Start sending the request
            log(LoggerLevel::DEBUG, "starting request send (%d bytes)", req_data.size());
            current_op = sock.startPollSend(req_data);
            req_state = REQ_SENDING;
            return handleSending();
        }

        # No pending request — wait for wake or socket events
        state = STATE_WAIT_READ;
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sock,
        };
    }

    #! Internal: handles request sending sub-state
    private *hash<SocketPollInfo> handleSending() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            log(LoggerLevel::DEBUG, "request sent, reading response header");
            current_op = sock.startPollReadHttpHeader();
            req_state = REQ_RECV_HEADER;
            return handleRecvHeader();
        }

        setError("HTTP1-SEND-ERROR", "send failed");
        return NOTHING;
    }

    #! Internal: handles response header reading sub-state
    private *hash<SocketPollInfo> handleRecvHeader() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            # getOutput() returns {hdr: {...}, info: {...}}
            hash<auto> output = current_op.getOutput();
            hash<auto> hdr = output.hdr;

            response_status_code = hdr.status_code;

            # Build response headers hash with lowercase keys
            response_headers = {};
            foreach string key in (keys hdr) {
                if (key == "status_code" || key == "status_message" || key == "http_version") {
                    continue;
                }
                response_headers{key.lwr()} = hdr{key};
            }

            # Determine body transfer type
            response_chunked = False;
            response_content_length = -1;
            connection_close = False;
            remove response_body;

            *string te = response_headers."transfer-encoding";
            if (te && te.lwr() =~ /chunked/) {
                response_chunked = True;
            } else {
                *string cl = response_headers."content-length";
                if (exists cl) {
                    response_content_length = cl.toInt();
                }
            }

            *string conn_hdr = response_headers.connection;
            if (conn_hdr && conn_hdr.lwr() == "close") {
                connection_close = True;
            }

            log(LoggerLevel::DEBUG, "response header: status=%d chunked=%y content_length=%d close=%y",
                response_status_code, response_chunked, response_content_length, connection_close);

            # Determine if the response has a body
            # HEAD responses, 1xx, 204, and 304 have no body regardless of headers
            *string method;
            {
                AutoLock al(stream_lock);
                method = active_method;
            }
            bool no_body = (method == "HEAD")
                || (response_status_code >= 100 && response_status_code < 200)
                || response_status_code == 204
                || response_status_code == 304;

            if (no_body) {
                return dispatchResponse();
            }

            # Start body reading based on transfer type
            if (response_chunked) {
                response_body = binary();
                current_op = sock.startPollRecvUntilBytesString("\r\n");
                req_state = REQ_RECV_CHUNK_SIZE;
                return handleRecvChunkSize();
            } else if (response_content_length > 0) {
                current_op = sock.startPollRecvBinary(response_content_length);
                req_state = REQ_RECV_BODY_LENGTH;
                return handleRecvBodyLength();
            } else if (response_content_length == 0) {
                # Explicit zero-length body
                return dispatchResponse();
            } else if (connection_close) {
                # No Content-Length, not chunked, but Connection: close — read until EOF
                response_body = binary();
                current_op = sock.startPollRecvData();
                req_state = REQ_RECV_BODY_CLOSE;
                return handleRecvBodyClose();
            } else {
                # No Content-Length, not chunked, not Connection: close — treat as no body
                return dispatchResponse();
            }
        }

        setError("HTTP1-RECV-ERROR", "header read failed");
        return NOTHING;
    }

    #! Internal: handles fixed-length body reading sub-state
    private *hash<SocketPollInfo> handleRecvBodyLength() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            response_body = current_op.getOutput();
            return dispatchResponse();
        }

        setError("HTTP1-RECV-ERROR", "body read failed");
        return NOTHING;
    }

    #! Internal: handles chunk size reading sub-state
    private *hash<SocketPollInfo> handleRecvChunkSize() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            string line = current_op.getOutput();
            # Remove trailing \r\n
            line = line.substr(0, line.size() - 2);
            # Parse hex size (chunk extensions after semicolon are ignored)
            int semi = line.find(";");
            if (semi >= 0) {
                line = line.substr(0, semi);
            }
            int chunk_size = line.toInt(16);

            log(LoggerLevel::DEBUG, "chunk size: %d", chunk_size);

            if (chunk_size == 0) {
                # Last chunk — read trailing CRLF (or trailers followed by CRLF)
                current_op = sock.startPollRecvUntilBytesString("\r\n");
                req_state = REQ_RECV_CHUNK_CRLF;
                return handleRecvChunkCrlf();
            }

            # Read chunk data + trailing CRLF (chunk_size + 2 bytes)
            current_op = sock.startPollRecvBinary(chunk_size + 2);
            req_state = REQ_RECV_CHUNK_DATA;
            return handleRecvChunkData();
        }

        setError("HTTP1-RECV-ERROR", "chunk size read failed");
        return NOTHING;
    }

    #! Internal: handles chunk data reading sub-state
    private *hash<SocketPollInfo> handleRecvChunkData() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            binary data = current_op.getOutput();
            # Remove trailing CRLF (last 2 bytes)
            if (data.size() > 2) {
                response_body += data.substr(0, data.size() - 2);
            }

            # Read next chunk size
            current_op = sock.startPollRecvUntilBytesString("\r\n");
            req_state = REQ_RECV_CHUNK_SIZE;
            return handleRecvChunkSize();
        }

        setError("HTTP1-RECV-ERROR", "chunk data read failed");
        return NOTHING;
    }

    #! Internal: handles trailing CRLF after last chunk
    private *hash<SocketPollInfo> handleRecvChunkCrlf() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            # Chunked transfer complete
            return dispatchResponse();
        }

        setError("HTTP1-RECV-ERROR", "chunk trailer read failed");
        return NOTHING;
    }

    #! Internal: handles body reading until connection close
    private *hash<SocketPollInfo> handleRecvBodyClose() {
        auto poll_info = current_op.continuePoll();
        if (poll_info) {
            return poll_info;
        }

        if (current_op.goalReached()) {
            auto data = current_op.getOutput();
            if (data && data.size() > 0) {
                response_body += data;
                # Read more data
                current_op = sock.startPollRecvData();
                return handleRecvBodyClose();
            }
            # Empty read — server closed connection; dispatch what we have
            return dispatchResponse();
        }

        # Read operation ended (likely EOF) — dispatch accumulated body
        return dispatchResponse();
    }

    #! Internal: dispatches the response to the registered callback and returns to idle
    private *hash<SocketPollInfo> dispatchResponse() {
        empty_read_count = 0;

        int stream_id;
        *HttpResponseCallback cb;
        *hash<auto> stream_ctx;
        {
            AutoLock al(stream_lock);
            stream_id = active_sid;
            string sid = string(stream_id);
            cb = stream_callbacks{sid};
            stream_ctx = stream_ctxs{sid};
            if (cb) {
                callback_dispatched = True;
            }
        }

        # Convert binary body to string for text-based content types so that
        # higher-level deserialization (JSON, XML, etc.) works correctly.
        # startPollRecvBinary() always returns binary, but text-based bodies
        # need to be strings for parse_json(), parse_xml(), etc.
        auto body = response_body;
        if (body && body.typeCode() == NT_BINARY) {
            *string ct = response_headers."content-type";
            if (ct && (ct =~ /^text\// || ct =~ /\/(json|xml|yaml|html)/
                    || ct =~ /\+(json|xml)/ || ct =~ /charset=/)) {
                body = body.toString();
            }
        }

        # Build response hash compatible with the HttpClientIo output format
        hash<auto> resp = {
            "status_code": response_status_code,
            "headers": response_headers,
            "body": body,
            "stream_id": stream_id,
            "end_stream": True,
            "protocol": "h1",
        };

        log(LoggerLevel::DEBUG, "dispatching response: stream_id=%d status=%d body_size=%d",
            stream_id, response_status_code, response_body ? response_body.size() : 0);

        if (cb) {
            try {
                cb(resp + {"ctx": stream_ctx}, NOTHING);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "stream callback exception: %s: %s", ex.err, ex.desc);
            }
        }

        # Clean up stream AFTER callback (same pattern as HTTP/2)
        {
            AutoLock al(stream_lock);
            string sid = string(stream_id);
            remove stream_callbacks{sid};
            remove stream_ctxs{sid};
            --active_stream_count;
            remove active_sid;
            remove active_method;
        }

        # Reset response accumulator
        req_state = REQ_IDLE;
        remove current_op;
        resetResponseState();

        # Check if server requested connection close
        if (connection_close) {
            log(LoggerLevel::DEBUG, "server requested connection close");
            state = STATE_CLOSED;
            return NOTHING;
        }

        # Return to idle — check for next pending request or wait
        state = STATE_WAIT_READ;
        return <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sock,
        };
    }

    #! Internal: resets response accumulator state between requests
    private resetResponseState() {
        remove response_headers;
        response_status_code = 0;
        remove response_body;
        response_chunked = False;
        response_content_length = -1;
        connection_close = False;
    }

    #! Internal: logging helper
    private log(int level, string fmt) {
        if (logger) {
            call_function_args(\logger.log(), (level, "Http1ClientPollOperation: " + fmt) + argv);
        }
    }
}

} # namespace HttpClientIo
