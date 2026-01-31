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
    #! Combined idle + header read (HTTP/1.x keep-alive optimization)
    KeepAlive = "keep_alive",
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
    #! Combined idle + header read (HTTP/1.x keep-alive)
    KeepAlive = "keep_alive",
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
        /** Multiple concurrent accept poll operations are supported on the
            same listener socket, reducing connection backlog during high
            connection rates.
        */
        const DefaultAcceptOps = 2;
    }

    private {
        #! Lock for atomic operations
        Mutex m();

        #! Logger
        *LoggerInterface logger;

        #! I/O controllers; accepts run on controllers[0], connections are sharded via round-robin
        list<AsyncSocketIoController> controllers;

        #! True if we own the controllers (created internally)
        bool own_controllers = False;

        #! Round-robin index for assigning connections to I/O threads
        int next_controller_idx = 0;

        #! Thread pool for request handler dispatch
        ThreadPool handler_pool;

        #! Thread pool configuration for recreation after stop()
        hash<auto> pool_opts;

        #! Shutdown flag; prevents handler tasks from submitting new operations during stop()
        bool stopping = False;

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
        @param opts optional configuration options:
        - \c max_handler_threads: maximum number of handler threads (default 0 = unlimited)
        - \c min_idle_handler_threads: minimum idle handler threads (default 0)
        - \c max_idle_handler_threads: maximum idle handler threads (default 0)
    */
    constructor(AsyncSocketIoController controller, *LoggerInterface logger,
            hash<auto> opts = {}) {
        @assert(controller, "controller cannot be null");
        self.controllers = (controller,);
        self.logger = logger;
        self.pool_opts = opts;
        self.handler_pool = new ThreadPool(
            opts.max_handler_threads ?? 0,
            opts.min_idle_handler_threads ?? 0,
            opts.max_idle_handler_threads ?? 0,
        );
    }

    #! Creates the HTTP controller with its own I/O controllers
    /** @param logger optional logger interface for debugging
        @param opts optional configuration options:
        - \c max_handler_threads: maximum number of handler threads (default 0 = unlimited)
        - \c min_idle_handler_threads: minimum idle handler threads (default 0)
        - \c max_idle_handler_threads: maximum idle handler threads (default 0)
        - \c io_threads: number of I/O threads (default 1); each thread runs its own
          event loop, allowing connections to be processed in parallel across cores

        Creates internal AsyncSocketIoController instances with autostop disabled.
    */
    constructor(*LoggerInterface logger, hash<auto> opts = {}) {
        int io_threads = opts.io_threads ?? 1;
        @assert(io_threads > 0, "io_threads must be positive");
        for (int i = 0; i < io_threads; ++i) {
            self.controllers += new AsyncSocketIoController(False, logger);
        }
        self.own_controllers = True;
        self.logger = logger;
        self.pool_opts = opts;
        self.handler_pool = new ThreadPool(
            opts.max_handler_threads ?? 0,
            opts.min_idle_handler_threads ?? 0,
            opts.max_idle_handler_threads ?? 0,
        );
    }

    #! Destructor stops the controller
    destructor() {
        stop();
    }

    #! Starts the I/O controllers
    start() {
        foreach AsyncSocketIoController c in (controllers) {
            if (!c.running()) {
                c.start();
            }
        }
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "HttpAsyncSocketIoController started (%d I/O threads)",
                    controllers.size());
            }
        }
    }

    #! Stops the HTTP controller
    /** Cancels all HTTP operations by their owners.
        Does NOT stop the shared controllers — other users may still be using them.
        If the controllers were created internally, they are stopped.
    */
    stop() {
        # Set stopping flag first — prevents handler tasks from submitting new operations
        # Copy keys under lock, then cancel without holding the lock
        # to avoid deadlock with I/O thread callbacks that also acquire m
        *list<string> listener_keys;
        *list<hash<auto>> conn_cancel_info;
        {
            AutoLock al(m);
            stopping = True;
            listener_keys = keys listeners;
            if (connections) {
                conn_cancel_info = map (
                    {"sock": connections{$1}.sock} + {"controller_idx": connections{$1}.controller_idx}
                ), keys connections;
            }
            listeners = {};
            connections = {};
            accept_counts = {};
        }
        # Accept operations are always on controllers[0]
        foreach string lhash in (listener_keys) {
            controllers[0].cancelByOwner(lhash);
        }
        foreach hash<auto> ci in (conn_cancel_info) {
            try {
                controllers[ci.controller_idx].cancel(ci.sock);
            } catch () {
                # ignore - operation may have already completed
            }
        }
        # Wait for in-flight handler tasks to complete; the stopping flag prevents
        # them from submitting new operations to the controllers
        handler_pool.stopWait();
        # Recreate the pool so the controller can be restarted
        handler_pool = new ThreadPool(
            pool_opts.max_handler_threads ?? 0,
            pool_opts.min_idle_handler_threads ?? 0,
            pool_opts.max_idle_handler_threads ?? 0,
        );
        # Second pass: cancel any operations that handler tasks managed to submit
        # before observing the stopping flag (narrow race window)
        {
            AutoLock al(m);
            if (connections) {
                conn_cancel_info = map (
                    {"sock": connections{$1}.sock} + {"controller_idx": connections{$1}.controller_idx}
                ), keys connections;
                connections = {};
            } else {
                conn_cancel_info = NOTHING;
            }
        }
        foreach hash<auto> ci in (conn_cancel_info) {
            try {
                controllers[ci.controller_idx].cancel(ci.sock);
            } catch () {}
        }
        if (own_controllers) {
            foreach AsyncSocketIoController c in (controllers) {
                c.stop();
            }
        }
        # Reset stopping flag so the controller can be restarted
        {
            AutoLock al(m);
            stopping = False;
        }
        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "HttpAsyncSocketIoController stopped");
            }
        }
    }

    #! Returns True if the controller is running
    bool running() {
        return controllers[0].running();
    }

    #! Waits for all I/O threads to stop
    waitStop() {
        foreach AsyncSocketIoController c in (controllers) {
            c.waitStop();
        }
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
        controllers[0].cancelByOwner(lhash);
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
            timeout ttl = DefaultIdleTimeout, *int controller_idx,
            *HttpKeepAlivePollOperation reuse_op) {
        string uh = sock.uniqueHash();
        hash<HttpActiveConnectionInfo> cinfo;
        {
            AutoLock al(m);
            if (stopping) {
                try { sock.close(); } catch () {}
                return;
            }
            if (!exists controller_idx) {
                controller_idx = next_controller_idx;
                next_controller_idx = (next_controller_idx + 1) % controllers.size();
            }
            # Opt 5: reuse existing keep-alive operation to avoid EventLoop remove/add
            *HttpKeepAlivePollOperation ka_op = reuse_op;
            if (ka_op) {
                ka_op.resetForIdle();
            } else {
                ka_op = new HttpKeepAlivePollOperation(sock, ttl);
            }
            cinfo = connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": listener,
                "cx": cx,
                "target": target,
                "state": HttpConnectionState::KeepAlive,
                "keep_alive_op": ka_op,
                "ttl": ttl,
                "controller_idx": controller_idx,
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
            "io_threads": controllers.size(),
            "running": controllers[0].running(),
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

        controllers[0].submit(<SocketPollOperationInfo>{
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

        # Assign this connection to an I/O thread via round-robin
        int cidx;
        {
            AutoLock al(m);
            cidx = next_controller_idx;
            next_controller_idx = (next_controller_idx + 1) % controllers.size();
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
                    log(LoggerLevel::INFO, "SSL handshake start: sock=%s alpn=%y io_thread=%d", uh, alpn, cidx);
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
                    "controller_idx": cidx,
                };
            }

            # Submit SSL handshake operation on the assigned I/O thread
            submitSslOp(sock, uh, ssl_op, linfo, output.http_version, cidx);
            return;
        }

        # Check if this is an HTTP/2 connection
        if (output.is_http2) {
            log(LoggerLevel::DEBUG, "Starting HTTP/2 connection handling for %y (io_thread=%d)", uh, cidx);
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
                    "controller_idx": cidx,
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

        log(LoggerLevel::DEBUG, "Starting HTTP/1.x header reading for %y (io_thread=%d)", uh, cidx);

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
                "controller_idx": cidx,
            };
        }

        submitConnectionOp(uh, cinfo);
    }

    #! Submits an SSL handshake operation
    private submitSslOp(Socket sock, string uh, AbstractPollOperation ssl_op,
            hash<HttpListenerInfo> linfo, string http_version, int controller_idx = 0) {
        {
            AutoLock al(m);
            if (stopping) {
                remove connections{uh};
                try { sock.close(); } catch () {}
                return;
            }
        }
        string owner = linfo.listener.uniqueHash();
        try {
            controllers[controller_idx].submit(<SocketPollOperationInfo>{
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

    #! Submits a connection operation, skipping the stopping check (caller already verified)
    private submitConnectionOpUnlocked(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        AbstractPollOperation op;
        date timeout = AsyncSocketIoController::DefaultIoOperationTimeout;
        string owner = uh;

        @debug {
            if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                log(LoggerLevel::INFO, "submitConnectionOpUnlocked: uh=%s state=%y", uh, cinfo.state);
            }
        }

        switch (cinfo.state) {
            case HttpConnectionState::Idle: {
                op = new HttpIdlePollOperation(cinfo.sock, cinfo.ttl);
                timeout = milliseconds(cinfo.ttl);
                break;
            }
            case HttpConnectionState::KeepAlive: {
                op = cinfo.keep_alive_op;
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
                timeout = -1s;
                break;
            }
            default:
                log(LoggerLevel::ERROR, "Unknown connection state %y for %s", cinfo.state, uh);
                return;
        }

        try {
            controllers[cinfo.controller_idx].submit(<SocketPollOperationInfo>{
                "sock": cinfo.sock,
                "spop": op,
                "owner": owner,
                "to": timeout,
                "callback": code sub (hash<SocketPollResultInfo> result) {
                    handleConnectionResult(result, uh, cinfo.state);
                },
            }, True);
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

    #! Submits a connection operation based on connection state
    private submitConnectionOp(string uh, hash<HttpActiveConnectionInfo> cinfo) {
        # Check shutdown flag — handler tasks may call this after stop() begins
        {
            AutoLock al(m);
            if (stopping) {
                remove connections{uh};
                try { cinfo.sock.close(); } catch () {}
                return;
            }
        }

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
            case HttpConnectionState::KeepAlive: {
                # Opt 1: combined idle + header operation
                # Use negative timeout to disable the outer AsyncSocketIoController timeout;
                # the operation manages its own deadlines (idle TTL + header read timeout)
                op = cinfo.keep_alive_op;
                timeout = -1s;
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
            controllers[cinfo.controller_idx].submit(<SocketPollOperationInfo>{
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
            case HttpConnectionState::KeepAlive: {
                # Opt 1: combined idle + header poll operation
                HttpKeepAlivePollOperation op = result.spop;
                if (op.timedOut()) {
                    handleIdleTimeout(uh, cinfo);
                } else if (op.headersReady()) {
                    # Headers fully read — dispatch directly without round-trip
                    hash<auto> output = op.getOutput();
                    handleHeaderComplete(uh, cinfo, output);
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

        if ((state == HttpConnectionState::Idle || state == HttpConnectionState::KeepAlive
                || state == HttpConnectionState::ReadHeader)
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

        # Opt 2: check if handler can run inline in I/O thread
        if (cinfo.target.canHandleInline(conn_info)) {
            try {
                hash<HttpRequestResultInfo> result = cinfo.target.onHttpRequest(conn_info);
                handleRequestResult(cinfo.sock, cinfo, cinfo.cx, result, cinfo.target);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error handling inline request: %s: %s", ex.err, ex.desc);
                cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
                try { cinfo.sock.close(); } catch () {}
            }
            return;
        }

        # Dispatch to thread pool so the I/O thread returns immediately
        handler_pool.submit(sub () {
            try {
                hash<HttpRequestResultInfo> result = cinfo.target.onHttpRequest(conn_info);
                handleRequestResult(cinfo.sock, cinfo, cinfo.cx, result, cinfo.target);
            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "Error handling keep-alive request: %s: %s", ex.err, ex.desc);
                cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
                try { cinfo.sock.close(); } catch () {}
            }
        });
    }

    #! Handles the result of request processing
    private handleRequestResult(Socket sock, auto linfo, hash<auto> cx,
            hash<HttpRequestResultInfo> result, HttpAsyncTarget target) {
        string uh = sock.uniqueHash();
        # Preserve the I/O thread assignment from the original connection
        int cidx = linfo.controller_idx ?? 0;

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
                    "controller_idx": cidx,
                };
            }
            submitConnectionOp(uh, cinfo);
            return;
        }

        if (result.return_to_idle) {
            # Opt 5: reuse the keep-alive operation for persistent socket registration
            *HttpKeepAlivePollOperation reuse_op = linfo.keep_alive_op;
            addIdleConnection(sock, linfo.listener ?? linfo, cx, target,
                result.ttl ?? DefaultIdleTimeout, cidx, reuse_op);
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

        # Opt 4: merge state update + stopping check into a single lock scope
        {
            AutoLock al(m);
            if (stopping) {
                remove connections{uh};
                try { cinfo.sock.close(); } catch () {}
                return;
            }
            connections{uh}.state = HttpConnectionState::ReadHeader;
            cinfo = connections{uh};
        }

        submitConnectionOpUnlocked(uh, cinfo);
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
    /** @par h2op ownership model
        Http2PollOperation is NOT thread-safe — it has no internal locking.  However,
        concurrent access cannot occur because ownership is transferred sequentially:

        1. The I/O thread owns h2op while polling (via submitConnectionOp).
        2. When goalReached(), the I/O controller removes h2op from polling and delivers
           the completion callback.  The I/O thread calls this method, which extracts the
           request data and submits a handler task.  At this point h2op is NOT registered
           with any I/O controller — no thread is polling it.
        3. The handler_pool task now has exclusive ownership: it calls onHttpRequest(),
           then handleHttp2RequestResult() which may call h2op.startSendResponse(),
           h2op.continuePoll(), and finally h2op.startReadNextRequest().
        4. handleHttp2RequestResult() calls submitConnectionOp() to hand h2op back to the
           I/O controller.  Ownership returns to the I/O thread.

        Only one request per HTTP/2 connection is in flight at a time.  The next request
        is not read until the handler task completes and re-submits h2op.  This means
        HTTP/2 multiplexing of concurrent streams on the same connection is serialized
        at this layer, which is acceptable since the handler thread pool provides
        cross-connection parallelism.
    */
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

        # Dispatch to thread pool — h2op ownership transfers to the worker thread
        # (see ownership model documentation above)
        handler_pool.submit(sub () {
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
        });
    }

    #! Handles the result of an HTTP/2 request
    /** Called on a handler_pool worker thread with exclusive ownership of h2op.
        After processing, ownership is returned to the I/O thread via submitConnectionOp().
        See handleHttp2RequestReady() for the full ownership model.
    */
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

    #! Keep-alive poll operation (for KeepAlive state, HTTP/1.x optimization)
    *HttpKeepAlivePollOperation keep_alive_op;

    #! Keep-alive timeout
    timeout ttl = DefaultIdleTimeout;

    #! HTTP version mode
    string http_version = HttpVersionMode::Auto;

    #! True if this is an HTTP/2 connection
    bool is_http2 = False;

    #! Index into the controllers list for this connection's I/O thread
    int controller_idx = 0;
}

} # namespace Priv
#! @endcond
