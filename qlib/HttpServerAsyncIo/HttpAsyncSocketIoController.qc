# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpAsyncSocketIoController.qc HTTP-aware async I/O controller

/** HttpAsyncSocketIoController.qc Copyright 2024 - 2026 Qore Technologies, s.r.o.

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

%requires AsyncSocketIo

#! Main namespace
public namespace HttpServerAsyncIo {

#! HTTP connection state
public enum HttpConnectionState : string {
    #! Idle keep-alive connection
    Idle = "idle",
    #! Reading HTTP headers
    ReadHeader = "read_header",
    #! SSL/TLS handshake in progress
    SslHandshake = "ssl_handshake",
    #! Server-Sent Events streaming
    Sse = "sse",
    #! WebSocket bidirectional communication
    WebSocket = "websocket",
    #! HTTP/2 multiplexed connection
    Http2 = "http2",
    #! HTTP/2 Server-Sent Events
    Http2Sse = "http2_sse",
    #! HTTP/2 WebSocket (RFC 8441)
    Http2WebSocket = "http2_websocket",
}

#! HTTP poll operation type
public enum HttpPollType : string {
    #! Accept new connection
    Accept = "accept",
    #! SSL/TLS handshake
    Ssl = "ssl",
    #! Idle keep-alive monitoring
    Idle = "idle",
    #! HTTP header reading
    Header = "header",
    #! Server-Sent Events
    Sse = "sse",
    #! WebSocket
    WebSocket = "websocket",
    #! HTTP/2
    Http2 = "http2",
    #! HTTP/2 SSE
    Http2Sse = "http2_sse",
    #! HTTP/2 WebSocket
    Http2WebSocket = "http2_websocket",
}

#! HTTP version mode
public enum HttpVersionMode : string {
    #! Auto-negotiate (default)
    Auto = "auto",
    #! HTTP/1.0
    Http10 = "1.0",
    #! HTTP/1.1
    Http11 = "1.1",
    #! HTTP/2.0
    Http20 = "2.0",
    #! HTTP/2 cleartext
    H2c = "h2c",
}

#! HTTP-aware async I/O controller for managing HTTP connections
/** This controller manages HTTP connections through their lifecycle:

    1. Accept new connections (with optional SSL)
    2. Read HTTP headers
    3. Dispatch to target callback
    4. Manage idle keep-alive connections
    5. Handle streaming protocols (SSE, WebSocket)

    All I/O polling is delegated to an @ref AsyncSocketIo::AsyncSocketIoController
    "AsyncSocketIoController" instance, using callback-based result delivery for
    efficient state machine transitions without a separate I/O thread.

    Multiple HttpServer instances can share a single controller for efficient
    resource usage across many listeners.

    The controller uses a callback pattern to notify the target about HTTP events.
    Implementations of @ref HttpAsyncTarget handle the actual request processing.

    @since HttpServerAsyncIo 1.0
*/
public class HttpAsyncSocketIoController {
    public {
        #! Maximum concurrent accept operations per listener
        /** Only one accept operation per listener is supported because
            Socket::startPollAccept() does not allow concurrent non-blocking
            operations on the same socket.
        */
        const DefaultAcceptOps = 1;
    }

    private {
        #! Lock for atomic operations
        Mutex m();

        #! Logger
        *LoggerInterface logger;

        #! Shared async I/O controller for all polling operations
        AsyncSocketIoController controller;

        #! True if we own the controller (created internally)
        bool own_controller = False;

        #! Registered listeners: listener unique hash -> listener info
        hash<string, hash<HttpListenerInfo>> listeners;

        #! Active connections: socket unique hash -> connection info
        hash<string, hash<HttpActiveConnectionInfo>> connections;

        #! Listener accept operation counters
        hash<string, int> accept_counts;
        int accept_seq = 0;
    }

    #! Creates the HTTP controller with an explicit I/O controller
    /** @param controller the shared async I/O controller to delegate polling to
        @param logger optional logger interface for debugging
    */
    constructor(AsyncSocketIoController controller, *LoggerInterface logger) {
        @assert(controller, "controller cannot be null");
        self.controller = controller;
        self.logger = logger;
    }

    #! Creates the HTTP controller with its own I/O controller
    /** @param logger optional logger interface for debugging

        Creates an internal AsyncSocketIoController with autostop disabled.
    */
    constructor(*LoggerInterface logger) {
        self.controller = new AsyncSocketIoController(False, logger);
        self.own_controller = True;
        self.logger = logger;
    }

    #! Destructor stops the controller
    destructor() {
        stop();
    }

    #! Starts the I/O controller
    start() {
        if (!controller.running()) {
            controller.start();
        }
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "HttpAsyncSocketIoController started");
            }
        }
    }

    #! Stops the HTTP controller
    /** Cancels all HTTP operations by their owners.
        Does NOT stop the shared controller — other users may still be using it.
        If the controller was created internally, it is stopped.
    */
    stop() {
        # Copy keys under lock, then cancel without holding the lock
        # to avoid deadlock with I/O thread callbacks that also acquire m
        *list<string> listener_keys;
        *list<Socket> conn_sockets;
        {
            AutoLock al(m);
            listener_keys = keys listeners;
            conn_sockets = connections ? (map connections{$1}.sock, keys connections) : NOTHING;
            listeners = {};
            connections = {};
            accept_counts = {};
        }
        foreach string lhash in (listener_keys) {
            controller.cancelByOwner(lhash);
        }
        foreach Socket sock in (conn_sockets) {
            try {
                controller.cancel(sock);
            } catch () {
                # ignore - operation may have already completed
            }
        }
        if (own_controller) {
            controller.stop();
        }
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "HttpAsyncSocketIoController stopped");
            }
        }
    }

    #! Returns True if the controller is running
    bool running() {
        return controller.running();
    }

    #! Waits for the I/O thread to stop
    waitStop() {
        controller.waitStop();
    }

    #! Adds an HTTP listener to be managed
    /** @param listener the listener socket
        @param target the target to receive HTTP events
        @param ssl True if this is an HTTPS listener
        @param cert SSL certificate data (required if ssl)
        @param key SSL private key data (required if ssl)
        @param http_version HTTP version mode: "auto" (default), "1.0", "1.1", or "2.0"
    */
    addListener(Socket listener, HttpAsyncTarget target, bool ssl = False, *data cert, *data key,
            string http_version = HttpVersionMode::Auto) {
        @assert(listener, "listener socket cannot be null");
        @assert(target, "target cannot be null");
        @assert(!ssl || (cert && key), "SSL mode requires certificate and key");
        string lhash = listener.uniqueHash();

        {
            AutoLock al(m);
            @assert(!listeners{lhash}, "listener already registered: %s", lhash);
            listeners{lhash} = <HttpListenerInfo>{
                "listener": listener,
                "target": target,
                "ssl": ssl,
                "cert": cert,
                "key": key,
                "http_version": http_version,
            };
        }

        # Submit N concurrent accept operations
        for (int i = 0; i < DefaultAcceptOps; ++i) {
            submitAcceptOp(listener, target, ssl, cert, key, http_version, lhash);
        }

        @debug(log(LoggerLevel::DEBUG, "addListener: registered listener %s with %d accept ops",
            lhash, DefaultAcceptOps));
        log(LoggerLevel::INFO, "Added listener %y (SSL: %y, HTTP version: %y)", lhash, ssl, http_version);
    }

    #! Removes a listener
    /** @param listener the listener to remove
    */
    removeListener(Socket listener) {
        string lhash = listener.uniqueHash();
        @debug(log(LoggerLevel::DEBUG, "removeListener: removing listener %s", lhash));
        controller.cancelByOwner(lhash);
        {
            AutoLock al(m);
            remove listeners{lhash};
            remove accept_counts{lhash};
        }
        log(LoggerLevel::INFO, "Removed listener %y", lhash);
    }

    #! Adds a socket back to idle pool for keep-alive
    /** @param sock the socket to monitor
        @param listener the listener object for callbacks
        @param cx connection context
        @param target the target for callbacks
        @param ttl keep-alive timeout
    */
    addIdleConnection(Socket sock, object listener, hash<auto> cx, HttpAsyncTarget target,
            timeout ttl = DefaultIdleTimeout) {
        string uh = sock.uniqueHash();
        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoLock al(m);
            cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": listener,
                "cx": cx,
                "target": target,
                "state": HttpConnectionState::Idle,
                "ttl": ttl,
            };
        }

        submitConnectionOp(uh, cinfo);
        log(LoggerLevel::DEBUG, "Added idle connection %y", uh);
    }

    #! Gets controller statistics
    hash<auto> getStats() {
        AutoLock al(m);
        return {
            "listeners": listeners.size(),
            "connections": connections.size(),
            "running": controller.running(),
        };
    }

    #! Submits an accept operation to the controller
    private submitAcceptOp(Socket listener, HttpAsyncTarget target, bool ssl,
            *data cert, *data key, string http_version, string owner) {
        int seq;
        {
            AutoLock al(m);
            seq = ++accept_seq;
            accept_counts{owner} = (accept_counts{owner} ?? 0) + 1;
        }
        on_error {
            AutoLock al(m);
            int cnt = accept_counts{owner} ?? 0;
            if (cnt > 0) {
                accept_counts{owner} = cnt - 1;
            }
        }
        string op_key = sprintf("accept:%s:%d", owner, seq);

        # Get the effective HTTP version - respect global HTTP/2 mode at runtime
        string eff_http_version = getEffectiveHttpVersion(http_version);

        HttpAcceptPollOperation op(listener, ssl, cert, key, SSL_VERIFY_NONE,
            True, eff_http_version, False);

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO,
                    "submitAcceptOp: key=%s listener=%s ssl=%y http_version=%s",
                    op_key, owner, ssl, eff_http_version);
            }
        }

        controller.submit(<SocketPollOperationInfo>{
            "sock": listener,
            "spop": op,
            "key": op_key,
            "owner": owner,
            "to": -1s,  # no timeout for accepts
            "callback": code sub (hash<SocketPollResultInfo> result) {
                handleAcceptResult(result, listener, target, ssl, cert, key, http_version, owner);
            },
        });
    }

    #! Handles accept operation result from the controller
    private handleAcceptResult(hash<SocketPollResultInfo> result, Socket listener,
            HttpAsyncTarget target, bool ssl, *data cert, *data key,
            string http_version, string owner) {
        # Decrement accept count
        {
            AutoLock al(m);
            int cnt = accept_counts{owner} ?? 0;
            if (cnt > 0) {
                accept_counts{owner} = cnt - 1;
            }
        }

        if (result.canceled) {
            @debug(log(LoggerLevel::DEBUG, "accept canceled for owner %s", owner));
            return;
        }

        # Replenish: submit a new accept operation (outside the lock to avoid deadlock,
        # since submitAcceptOp() also acquires m)
        bool do_replenish = False;
        {
            AutoLock al(m);
            if (listeners{owner}) {
                int cnt = accept_counts{owner} ?? 0;
                if (cnt < DefaultAcceptOps) {
                    do_replenish = True;
                }
            }
        }
        if (do_replenish) {
            submitAcceptOp(listener, target, ssl, cert, key, http_version, owner);
        }

        if (result.ex) {
            log(LoggerLevel::WARN, "Accept error: %s: %s", result.ex.err, result.ex.desc);
            return;
        }

        hash<auto> output = result.spop.getOutput();
        handleAcceptComplete(<HttpListenerInfo>{
            "listener": listener,
            "target": target,
            "ssl": ssl,
            "cert": cert,
            "key": key,
            "http_version": http_version,
        }, output);
    }

    #! Handles accept completion
    private handleAcceptComplete(hash<HttpListenerInfo> linfo, hash<auto> output) {
        Socket sock = output.sock;
        string uh = sock.uniqueHash();
        # Ensure the accepted socket is closed on any unexpected exception
        on_error try { sock.close(); } catch () {}

        log(LoggerLevel::DEBUG, "Accept complete: %y (SSL: %y, HTTP/2: %y)", uh, output.ssl, output.is_http2);
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO,
                    "Accept complete: sock=%s ssl=%y secure=%y http2=%y",
                    uh, output.ssl, sock.isSecure(), output.is_http2);
            }
        }

        if (output.ssl && !sock.isSecure()) {
            hash<auto> cx = {
                "peer": sock.getPeerInfo(),
                "socket": sock.getSocketInfo(),
            };

            if (linfo.cert) {
                sock.setCertificate(linfo.cert);
            }
            if (linfo.key) {
                sock.setPrivateKey(linfo.key);
            }
            sock.setSslVerifyMode(SSL_VERIFY_NONE);
            sock.acceptAllCertificates(True);

            list<string> alpn;
            if (output.http_version == HttpVersionMode::Auto) {
                alpn = ("h2", "http/1.1");
            } else if (output.http_version == HttpVersionMode::Http20 || output.http_version == "2") {
                alpn = ("h2",);
            } else {
                alpn = ("http/1.1",);
            }
            sock.setAlpnProtocols(alpn);

            SocketPollOperation ssl_op = sock.startPollUpgradeServerToSSL();
            @debug {
                if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                    log(LoggerLevel::INFO, "SSL handshake start: sock=%s alpn=%y", uh, alpn);
                }
            }

            hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoLock al(m);
                cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": linfo.target,
                    "state": HttpConnectionState::SslHandshake,
                    "ssl_op": ssl_op,
                    "http_version": output.http_version,
                    "is_http2": False,
                };
            }

            # Submit SSL handshake operation
            submitSslOp(sock, uh, ssl_op, linfo, output.http_version);
            return;
        }

        # Check if this is an HTTP/2 connection
        if (output.is_http2) {
            log(LoggerLevel::DEBUG, "Starting HTTP/2 connection handling for %y", uh);
            hash<auto> cx = {
                "peer": sock.getPeerInfo(),
                "socket": sock.getSocketInfo(),
            };

            Http2PollOperation h2op(sock);

            hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoLock al(m);
                cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": linfo.target,
                    "state": HttpConnectionState::Http2,
                    "stream_op": h2op,
                    "is_http2": True,
                };
            }

            submitConnectionOp(uh, cinfo);
            return;
        }

        # HTTP/1.x path
        hash<auto> cx = {
            "peer": sock.getPeerInfo(),
            "socket": sock.getSocketInfo(),
        };

        log(LoggerLevel::DEBUG, "Starting HTTP/1.x header reading for %y", uh);

        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoLock al(m);
            cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": linfo.listener,
                "cx": cx,
                "target": linfo.target,
                "state": HttpConnectionState::ReadHeader,
                "http_version": output.http_version,
                "is_http2": False,
            };
        }

        submitConnectionOp(uh, cinfo);
    }

    #! Submits an SSL handshake operation
    private submitSslOp(Socket sock, string uh, AbstractPollOperation ssl_op,
            hash<HttpListenerInfo> linfo, string http_version) {
        string owner = linfo.listener.uniqueHash();
        try {
            controller.submit(<SocketPollOperationInfo>{
                "sock": sock,
                "spop": ssl_op,
                "owner": owner,
                "to": AsyncSocketIoController::DefaultIoOperationTimeout,
                "callback": code sub (hash<SocketPollResultInfo> result) {
                    handleSslResult(result, uh, linfo, http_version);
                },
            });
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit SSL op for %s: %s: %s", uh, ex.err, ex.desc);
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try { sock.close(); } catch () {}
        }
    }

    #! Handles SSL handshake result
    private handleSslResult(hash<SocketPollResultInfo> result, string uh,
            hash<HttpListenerInfo> linfo, string http_version) {
        if (result.canceled) {
            @debug(log(LoggerLevel::DEBUG, "SSL handshake canceled: %s", uh));
            {
                AutoLock al(m);
                remove connections{uh};
            }
            return;
        }

        if (result.ex) {
            log(LoggerLevel::ERROR, "SSL handshake error for %s: %s: %s",
                uh, result.ex.err, result.ex.desc);
            hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoLock al(m);
                cinfo = remove connections{uh};
            }
            if (cinfo) {
                try {
                    cinfo.target.onError(result.sock, result.ex, cinfo.listener, cinfo.cx);
                } catch (hash<ExceptionInfo> ex2) {
                    log(LoggerLevel::ERROR, "onError callback threw during SSL error for %s: %s: %s",
                        uh, ex2.err, ex2.desc);
                }
            }
            try { result.sock.close(); } catch () {}
            return;
        }

        # SSL handshake complete
        Socket sock = result.sock;

        string negotiated = sock.getAlpnProtocol();
        bool is_http2 = (negotiated == "h2");

        if ((http_version == HttpVersionMode::Http20 || http_version == "2") && !is_http2) {
            log(LoggerLevel::WARN, "HTTP/2 required but client negotiated: %y", negotiated ?? "none");
            try { sock.close(); } catch () {}
            {
                AutoLock al(m);
                remove connections{uh};
            }
            return;
        }

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO,
                    "SSL handshake complete: sock=%s negotiated=%y http2=%y",
                    uh, negotiated ?? "none", is_http2);
            }
        }

        try {
            hash<HttpActiveConnectionInfo> cinfo;
            if (is_http2) {
                Http2PollOperation h2op(sock);
                {
                    AutoLock al(m);
                    connections{uh}.state = HttpConnectionState::Http2;
                    connections{uh}.stream_op = h2op;
                    connections{uh}.is_http2 = True;
                    cinfo = connections{uh};
                }
            } else {
                {
                    AutoLock al(m);
                    connections{uh}.state = HttpConnectionState::ReadHeader;
                    connections{uh}.is_http2 = False;
                    cinfo = connections{uh};
                }
            }

            submitConnectionOp(uh, cinfo);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "SSL post-handshake setup failed for %s: %s: %s", uh, ex.err, ex.desc);
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try { result.sock.close(); } catch () {}
        }
    }

    #! Submits a connection operation based on connection state
    private submitConnectionOp(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        AbstractPollOperation op;
        date timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
        string owner = uh;

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "submitConnectionOp: uh=%s state=%y", uh, cinfo.state);
            }
        }

        switch (cinfo.state) {
            case HttpConnectionState::Idle: {
                op = new HttpIdlePollOperation(cinfo.sock, cinfo.ttl);
                timeout = milliseconds(cinfo.ttl);
                break;
            }
            case HttpConnectionState::ReadHeader: {
                op = cinfo.sock.startPollReadHttpHeader();
                break;
            }
            case HttpConnectionState::Http2: {
                if (!cinfo.stream_op) {
                    log(LoggerLevel::ERROR, "HTTP/2 connection %s has no stream_op", uh);
                    return;
                }
                op = cinfo.stream_op;
                timeout = -1s;  # no timeout for HTTP/2 connections
                break;
            }
            case HttpConnectionState::Sse: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = -1s;
                break;
            }
            case HttpConnectionState::WebSocket: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = -1s;
                break;
            }
            case HttpConnectionState::Http2Sse: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = -1s;
                break;
            }
            case HttpConnectionState::Http2WebSocket: {
                if (!cinfo.stream_op) return;
                op = cinfo.stream_op;
                timeout = -1s;
                break;
            }
            default:
                log(LoggerLevel::ERROR, "Unknown connection state %y for %s", cinfo.state, uh);
                return;
        }

        try {
            controller.submit(<SocketPollOperationInfo>{
                "sock": cinfo.sock,
                "spop": op,
                "owner": owner,
                "to": timeout,
                "callback": code sub (hash<SocketPollResultInfo> result) {
                    handleConnectionResult(result, uh, cinfo.state);
                },
            }, True);  # replace=True since connection key may be reused
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Failed to submit connection op for %s (state=%y): %s: %s",
                uh, cinfo.state, ex.err, ex.desc);
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try { cinfo.sock.close(); } catch () {}
        }
    }

    #! Handles connection operation result from the controller
    private handleConnectionResult(hash<SocketPollResultInfo> result, string uh,
            string orig_state) {
        if (result.canceled) {
            @debug(log(LoggerLevel::DEBUG, "connection op canceled: %s state=%y", uh, orig_state));
            return;
        }

        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoLock al(m);
            cinfo = connections{uh};
        }

        if (!cinfo) {
            @debug(log(LoggerLevel::DEBUG, "connection gone for %s during %y callback", uh, orig_state));
            return;
        }

        if (result.ex) {
            # Handle error
            handleConnectionError(uh, cinfo, orig_state, result.ex);
            return;
        }

        # Dispatch based on the state that was active when the operation was submitted
        switch (orig_state) {
            case HttpConnectionState::Idle: {
                HttpIdlePollOperation op = result.spop;
                if (op.timedOut()) {
                    handleIdleTimeout(uh, cinfo);
                } else if (op.hasData()) {
                    handleIdleDataReady(uh, cinfo);
                } else {
                    handleConnectionClosed(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::ReadHeader: {
                hash<auto> output = result.spop.getOutput();
                handleHeaderComplete(uh, cinfo, output);
                break;
            }
            case HttpConnectionState::Sse: {
                HttpSseStreamPollOperation op = result.spop;
                if (op.isDisconnected()) {
                    handleSseDisconnect(uh, cinfo);
                }
                break;
            }
            case HttpConnectionState::WebSocket: {
                HttpWebSocketPollOperation op = result.spop;
                if (op.goalReached()) {
                    handleWebSocketClose(uh, cinfo, op);
                }
                break;
            }
            case HttpConnectionState::Http2: {
                Http2PollOperation h2op = result.spop;
                if (h2op.goalReached()) {
                    handleHttp2RequestReady(uh, cinfo, h2op);
                } else if (h2op.isClosed() || h2op.hasError()) {
                    handleHttp2ConnectionClosed(uh, cinfo, h2op);
                }
                break;
            }
            case HttpConnectionState::Http2Sse: {
                Http2SseStream sse_stream = cast<Http2SseStream>(result.spop);
                if (sse_stream.isDisconnected()) {
                    handleHttp2SseDisconnect(uh, cinfo, sse_stream);
                }
                break;
            }
            case HttpConnectionState::Http2WebSocket: {
                Http2WebSocketStream ws_stream = cast<Http2WebSocketStream>(result.spop);
                if (!ws_stream.isOpen()) {
                    handleHttp2WebSocketClose(uh, cinfo, ws_stream);
                }
                break;
            }
        }
    }

    #! Handles connection errors
    private handleConnectionError(string uh, hash<HttpActiveConnectionInfo> cinfo,
            string state, hash<ExceptionInfo> ex) {
        @debug {
            if (getenv("QORE_ASYNCIO_TRACE")) {
                log(LoggerLevel::INFO,
                    "connection error: state=%y conn=%y err=%s desc=%s",
                    state, uh, ex.err, ex.desc);
            }
        }

        if ((state == HttpConnectionState::Idle || state == HttpConnectionState::ReadHeader)
            && ex.err == "SOCKET-HTTP-ERROR"
            && ex.desc =~ /remote end closed connection/) {
            log(LoggerLevel::DEBUG, "Connection closed by peer during read: %y", uh);
            handleConnectionClosed(uh, cinfo);
            return;
        }

        {
            AutoLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex2) {
            log(LoggerLevel::ERROR, "onError callback threw for %s: %s: %s", uh, ex2.err, ex2.desc);
        }
        try { cinfo.sock.close(); } catch () {}
    }

    #! Handles header read completion
    private handleHeaderComplete(string uh, hash<HttpActiveConnectionInfo> cinfo, hash<auto> output) {
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": cinfo.sock,
            "hdr": output.hdr,
            "header_info": output.info,
            "listener": cinfo.listener,
            "cx": cinfo.cx,
            "keep_alive": True,
            "ssl": cinfo.sock.isSecure(),
        };

        log(LoggerLevel::DEBUG, "Header complete for keep-alive: %y", uh);

        # Remove from our tracking
        {
            AutoLock al(m);
            remove connections{uh};
        }

        # Call the target
        try {
            hash<HttpRequestResultInfo> result = cinfo.target.onHttpRequest(conn_info);
            handleRequestResult(cinfo.sock, cinfo, cinfo.cx, result, cinfo.target);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error handling keep-alive request: %s: %s", ex.err, ex.desc);
            cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
            try { cinfo.sock.close(); } catch () {}
        }
    }

    #! Handles the result of request processing
    private handleRequestResult(Socket sock, auto linfo, hash<auto> cx,
            hash<HttpRequestResultInfo> result, HttpAsyncTarget target) {
        string uh = sock.uniqueHash();

        if (result.close) {
            try { sock.close(); } catch () {}
            return;
        }

        if (result.streaming) {
            hash<HttpActiveConnectionInfo> cinfo;
            {
                AutoLock al(m);
                cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener ?? linfo,
                    "cx": cx,
                    "target": target,
                    "state": result.stream_type,
                    "stream_op": result.stream_operation,
                    "ttl": result.ttl ?? DefaultIdleTimeout,
                };
            }
            submitConnectionOp(uh, cinfo);
            return;
        }

        if (result.return_to_idle) {
            addIdleConnection(sock, linfo.listener ?? linfo, cx, target,
                result.ttl ?? DefaultIdleTimeout);
        }
    }

    #! Handles idle timeout
    private handleIdleTimeout(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::DEBUG, "Idle timeout: %y", uh);

        {
            AutoLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onIdleTimeout(cinfo.sock, cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onIdleTimeout callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles data ready on idle connection
    private handleIdleDataReady(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::DEBUG, "Data ready on idle: %y", uh);

        # Transition to header reading
        {
            AutoLock al(m);
            connections{uh}.state = HttpConnectionState::ReadHeader;
            cinfo = connections{uh};
        }

        submitConnectionOp(uh, cinfo);
    }

    #! Handles connection closed
    private handleConnectionClosed(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::DEBUG, "Connection closed: %y", uh);

        {
            AutoLock al(m);
            remove connections{uh};
        }
    }

    #! Handles SSE disconnect
    private handleSseDisconnect(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        log(LoggerLevel::DEBUG, "SSE disconnect: %y", uh);

        {
            AutoLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onSseDisconnect(cinfo.sock, cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onSseDisconnect callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles WebSocket close
    private handleWebSocketClose(string uh, hash<HttpActiveConnectionInfo> cinfo,
            HttpWebSocketPollOperation op) {
        log(LoggerLevel::DEBUG, "WebSocket close: %y (code: %d)", uh, op.getCloseCode());

        {
            AutoLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onWebSocketClose(cinfo.sock, op.getCloseCode(),
                op.getCloseReason(), cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onWebSocketClose callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles HTTP/2 request ready
    private handleHttp2RequestReady(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2PollOperation h2op) {
        hash<auto> request = h2op.getOutput();

        log(LoggerLevel::DEBUG, "HTTP/2 request ready: %y stream=%d %s %s",
            uh, request.stream_id, request.method, request.path);

        # Check for RFC 8441 WebSocket over HTTP/2 (CONNECT with :protocol=websocket)
        bool is_websocket_connect = request.method == "CONNECT" &&
            request.headers.":protocol" == "websocket";

        # Parse accept-encoding header for compression support
        *list<string> accept_encoding;
        if (request.headers."accept-encoding") {
            accept_encoding = map trim($1.split(";")[0]),
                request.headers."accept-encoding".split(",");
        }

        # Convert HTTP/2 request to HttpConnectionInfo format
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": cinfo.sock,
            "hdr": request.headers + {
                "method": request.method,
                "path": request.path,
                ":authority": request.authority,
                ":scheme": request.scheme,
                "host": request.authority,
                "http_version": "HTTP/2",
            },
            "header_info": {
                "http2": True,
                "stream_id": request.stream_id,
                "is_websocket_connect": is_websocket_connect,
                "push_resource": h2op.getPushResourceCallback(),
                "h2op": h2op,
                "accept-encoding": accept_encoding,
            },
            "listener": cinfo.listener,
            "cx": cinfo.cx,
            "keep_alive": True,
            "ssl": cinfo.sock.isSecure(),
        };

        try {
            hash<HttpRequestResultInfo> result = cinfo.target.onHttpRequest(conn_info);
            handleHttp2RequestResult(h2op, cinfo, request, result, is_websocket_connect);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error handling HTTP/2 request: %s: %s", ex.err, ex.desc);
            cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try { cinfo.sock.close(); } catch () {}
        }
    }

    #! Handles the result of an HTTP/2 request
    private handleHttp2RequestResult(Http2PollOperation h2op, hash<HttpActiveConnectionInfo> cinfo,
            hash<auto> request, hash<HttpRequestResultInfo> result, bool is_websocket_connect = False) {
        string uh = cinfo.sock.uniqueHash();

        if (result.close) {
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try { cinfo.sock.close(); } catch () {}
            return;
        }

        # Handle streaming modes for HTTP/2
        if (result.streaming && result.stream_type) {
            switch (result.stream_type) {
                case "websocket": {
                    if (!is_websocket_connect) {
                        log(LoggerLevel::WARN, "WebSocket streaming requested but not a CONNECT request");
                        break;
                    }
                    log(LoggerLevel::DEBUG, "HTTP/2 WebSocket stream started: %y stream=%d",
                        uh, request.stream_id);

                    h2op.startSendResponse(result.code ?? 200, result.hdr);

                    # Drive the send operation to completion
                    while (True) {
                        *hash<SocketPollInfo> poll_info = h2op.continuePoll();
                        if (!poll_info) {
                            break;
                        }
                        Socket::poll((poll_info,), 100ms);
                    }

                    Http2WebSocketStream ws_stream(h2op, request.stream_id);
                    hash<HttpActiveConnectionInfo> new_cinfo;
                    {
                        AutoLock al(m);
                        connections{uh}.stream_op = result.stream_operation ?? ws_stream;
                        connections{uh}.state = HttpConnectionState::Http2WebSocket;
                        new_cinfo = connections{uh};
                    }
                    submitConnectionOp(uh, new_cinfo);
                    return;
                }

                case "sse": {
                    log(LoggerLevel::DEBUG, "HTTP/2 SSE stream started: %y stream=%d",
                        uh, request.stream_id);
                    Http2SseStream sse_stream(h2op, request.stream_id);
                    hash<HttpActiveConnectionInfo> new_cinfo;
                    {
                        AutoLock al(m);
                        connections{uh}.stream_op = result.stream_operation ?? sse_stream;
                        connections{uh}.state = HttpConnectionState::Http2Sse;
                        new_cinfo = connections{uh};
                    }
                    submitConnectionOp(uh, new_cinfo);
                    return;
                }
            }
        }

        # Prepare for next request
        if (h2op.shouldKeepAlive()) {
            log(LoggerLevel::DEBUG, "HTTP/2 connection %y waiting for next request", uh);
            h2op.startReadNextRequest();
            hash<HttpActiveConnectionInfo> new_cinfo;
            {
                AutoLock al(m);
                connections{uh}.stream_op = h2op;
                connections{uh}.state = HttpConnectionState::Http2;
                new_cinfo = connections{uh};
            }
            submitConnectionOp(uh, new_cinfo);
        } else {
            log(LoggerLevel::DEBUG, "HTTP/2 connection %y closing", uh);
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try { cinfo.sock.close(); } catch () {}
        }
    }

    #! Handles HTTP/2 connection closed
    private handleHttp2ConnectionClosed(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2PollOperation h2op) {
        *hash<auto> error = h2op.getError();
        if (error) {
            log(LoggerLevel::DEBUG, "HTTP/2 connection closed with error: %y (%s: %s)",
                uh, error.err, error.desc);
            try {
                cinfo.target.onError(cinfo.sock, cast<hash<ExceptionInfo>>(error),
                    cinfo.listener, cinfo.cx);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "onError callback threw for %s: %s: %s", uh, ex.err, ex.desc);
            }
        } else {
            log(LoggerLevel::DEBUG, "HTTP/2 connection closed: %y", uh);
        }

        {
            AutoLock al(m);
            remove connections{uh};
        }
    }

    #! Handles HTTP/2 SSE disconnect
    private handleHttp2SseDisconnect(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2SseStream sse_stream) {
        log(LoggerLevel::DEBUG, "HTTP/2 SSE disconnect: %y stream=%d", uh, sse_stream.getStreamId());

        {
            AutoLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onSseDisconnect(cinfo.sock, cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onSseDisconnect callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Handles HTTP/2 WebSocket close
    private handleHttp2WebSocketClose(string uh, hash<HttpActiveConnectionInfo> cinfo,
            Http2WebSocketStream ws_stream) {
        log(LoggerLevel::DEBUG, "HTTP/2 WebSocket close: %y stream=%d (code: %d)",
            uh, ws_stream.getStreamId(), ws_stream.getCloseCode());

        {
            AutoLock al(m);
            remove connections{uh};
        }

        try {
            cinfo.target.onWebSocketClose(cinfo.sock, ws_stream.getCloseCode(),
                ws_stream.getCloseReason(), cinfo.listener, cinfo.cx);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "onWebSocketClose callback threw for %s: %s: %s", uh, ex.err, ex.desc);
        }
    }

    #! Logs a message
    private log(int level, string fmt) {
        if (logger) {
            logger.log(level, vsprintf(fmt, argv));
        }
    }

    #! Returns the effective HTTP version respecting global HTTP/2 mode
    private string getEffectiveHttpVersion(string configured_version) {
        string global_mode = get_global_http2_mode();
        if (global_mode == "disabled"
            && (configured_version == HttpVersionMode::Auto
                || configured_version == HttpVersionMode::Http20
                || configured_version == "2")) {
            return HttpVersionMode::Http11;
        }
        return configured_version;
    }
}

} # namespace HttpServerAsyncIo

#! @cond nodoc
namespace Priv {

#! Internal hashdecl for listener info
hashdecl HttpListenerInfo {
    #! The listener socket
    Socket listener;

    #! Target for callbacks
    HttpAsyncTarget target;

    #! True for SSL listeners
    bool ssl = False;

    #! SSL certificate
    *data cert;

    #! SSL private key
    *data key;

    #! HTTP version mode
    string http_version = HttpVersionMode::Auto;
}

#! Internal hashdecl for active connection info
hashdecl HttpActiveConnectionInfo {
    #! The socket
    Socket sock;

    #! The listener object
    object listener;

    #! Connection context
    hash<auto> cx;

    #! Target for callbacks
    HttpAsyncTarget target;

    #! Connection state
    string state = HttpConnectionState::Idle;

    #! SSL handshake poll operation (for "ssl_handshake" state)
    *AbstractPollOperation ssl_op;

    #! Stream operation (for SSE/WebSocket/HTTP2)
    *AbstractPollOperation stream_op;

    #! Keep-alive timeout
    timeout ttl = DefaultIdleTimeout;

    #! HTTP version mode
    string http_version = HttpVersionMode::Auto;

    #! True if this is an HTTP/2 connection
    bool is_http2 = False;
}

} # namespace Priv
#! @endcond
