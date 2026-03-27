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

    @par Stream Callbacks
    When a response is complete, the registered callback for that stream ID is
    invoked with the response data. Callbacks are registered via submitRequest().
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
            *SSLCertificate client_cert, *SSLPrivateKey client_pk) :
            Http3ClientPollOperationBase(
                Http3ClientPollOperation::createAndConfigureSocket(url_info, ssl_verify_mode,
                    accept_all_certs, client_cert, client_pk),
                url_info.host,
                url_info.port) {
        h3_url_info = url_info;
        h3_logger = logger;
    }

    #! Copying is not supported
    private copy() {
        throw "COPY-ERROR", "Http3ClientPollOperation objects cannot be copied";
    }

    #! Submits a new HTTP/3 request on this connection
    /** @param method HTTP method (GET, POST, etc.)
        @param path request path
        @param headers request headers
        @param body request body (optional)
        @param callback callback to invoke when response is ready
        @param ctx optional context to pass to callback
        @param max_streams maximum concurrent streams allowed on this connection
        @param streaming if True, submit without END_STREAM (for bidirectional streaming);
        the caller sends body data incrementally via sendStreamData()

        @return the stream ID assigned to this request

        @throw HTTP3-STATE-ERROR if the connection is not ready for requests
        @throw HTTP3-CAPACITY-ERROR if the connection is at capacity
    */
    int submitRequest(string method, string path, *hash<auto> headers, *data body,
            HttpResponseCallback callback, *hash<auto> ctx, int max_streams,
            bool streaming = False) {
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

        # Phase 3: register callback (C++ acquires stream_lock internally)
        *hash<auto> reg_result = self.registerStream(stream_id, callback, ctx);
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

        # Deliver any buffered responses outside the lock
        if (reg_result.buffered) {
            string sid = string(stream_id);
            foreach hash<auto> resp in (reg_result.buffered) {
                try {
                    callback(resp + {"ctx": ctx}, NOTHING);
                } catch (hash<ExceptionInfo> ex) {
                    log(LoggerLevel::ERROR, "buffered callback exception: %s: %s",
                        ex.err, ex.desc);
                }
                if (resp.end_stream.toBool()) {
                    *code<nothing(hash<auto>, *hash<auto>)> removed_cb = self.cancelStream(stream_id);
                    # callback already dispatched inline; no need to call removed_cb
                }
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
        *code<nothing(hash<auto>, *hash<auto>)> cb = self.cancelStream(stream_id);
        if (!cb) {
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

        try {
            cb({}, {"err": "HTTP3-CANCELLED", "desc": "request cancelled"});
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::DEBUG, "cancellation callback threw: %s: %s", ex.err, ex.desc);
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
