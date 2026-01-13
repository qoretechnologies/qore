# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file HttpAsyncSocketIoController.qc HTTP-aware async I/O controller

/** HttpAsyncSocketIoController.qc Copyright 2024 Qore Technologies, s.r.o.

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

#! HTTP-aware async I/O controller for managing HTTP connections
/** This controller manages HTTP connections through their lifecycle:

    1. Accept new connections (with optional SSL)
    2. Read HTTP headers
    3. Dispatch to target callback
    4. Manage idle keep-alive connections
    5. Handle streaming protocols (SSE, WebSocket)

    Multiple HttpServer instances can share a single controller for efficient
    resource usage across many listeners.

    The controller uses a callback pattern to notify the target about HTTP events.
    Implementations of @ref HttpAsyncTarget handle the actual request processing.

    @since HttpServerAsyncIo 1.0
*/
public class HttpAsyncSocketIoController {
    public {
        #! Default poll timeout
        const DefaultPollTimeout = 100ms;

        #! Polling event map
        const PollEventMap = {
            SOCK_POLLIN: "read",
            SOCK_POLLOUT: "write",
            SOCK_POLLIN | SOCK_POLLOUT: "read and write",
        };

        #! Maximum concurrent accept operations per listener
        const DefaultAcceptOps = 2;
    }

    private {
        #! Lock for atomic operations
        Mutex m();

        #! Logger
        *LoggerInterface logger;

        #! Registered listeners: listener unique hash -> listener info
        hash<string, hash<HttpListenerInfo>> listeners;

        #! Active connections: socket unique hash -> connection info
        hash<string, hash<HttpActiveConnectionInfo>> connections;

        #! Listener accept operation counters
        hash<string, int> accept_counts;
        int accept_seq = 0;

        #! I/O thread TID
        int tid;

        #! I/O thread counter
        Counter mcnt();

        #! Stop flag
        bool stop_flag = False;

        #! File for inter-thread signaling
        File sem_write;
        ReadOnlyFile sem_read;

        #! True if a signal is pending on the command pipe
        bool sem_pending = False;

        #! Command queue
        Queue cmdq();

        #! Persistent event loop for efficient kqueue/epoll/poll
        EventLoop loop();

        #! Map of poll key -> currently registered socket
        hash<string, Socket> registered_sockets;

        #! Map of socket unique hash -> current poll events
        hash<string, int> registered_events;

        #! Map of socket unique hash -> reference count (number of operations using this socket)
        hash<string, int> socket_refcounts;

        #! Commands
        const CMD_ADD_LISTENER = "add_listener";
        const CMD_REMOVE_LISTENER = "remove_listener";
        const CMD_ADD_IDLE = "add_idle";
        const CMD_QUIT = "quit";
    }

    #! Creates the controller
    /** @param logger optional logger interface for debugging
    */
    constructor(*LoggerInterface logger) {
        self.logger = logger;
        # Create internal signaling pipe
        hash<PipeInfo> pipe = File::getPipe();
        sem_read = pipe.read;
        sem_write = pipe.write;
        # Register semaphore pipe in persistent EventLoop
        loop.add(sem_read, SOCK_POLLIN);
    }

    #! Destructor stops the controller
    destructor() {
        stop();
    }

    #! Starts the I/O thread
    start() {
        AutoLock al(m);
        if (tid) {
            throw "CONTROLLER-START-ERROR", sprintf("I/O thread already running in TID %d", tid);
        }
        stop_flag = False;
        mcnt.inc();
        on_error mcnt.dec();
        tid = background ioThread();
    }

    #! Stops the I/O thread
    stop() {
        bool signal = False;
        {
            AutoLock al(m);
            if (!tid) {
                return;
            }
            stop_flag = True;
            signal = sendCmdLocked(CMD_QUIT);
        }
        if (signal) {
            sem_write.write(".");
        }
        mcnt.waitForZero();
    }

    #! Returns True if the controller is running
    bool running() {
        return mcnt.getCount() > 0;
    }

    #! Waits for the I/O thread to stop
    waitStop() {
        mcnt.waitForZero();
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
            string http_version = "auto") {
        string uh = listener.uniqueHash();
        bool signal = False;
        {
            AutoLock al(m);
            if (listeners{uh}) {
                throw "LISTENER-EXISTS", sprintf("Listener %y already registered", uh);
            }
            listeners{uh} = <HttpListenerInfo>{
                "listener": listener,
                "target": target,
                "ssl": ssl,
                "cert": cert,
                "key": key,
                "http_version": http_version,
            };
            signal = sendCmdLocked(CMD_ADD_LISTENER, uh);
        }
        if (signal) {
            sem_write.write(".");
        }
        log(LoggerLevel::INFO, "Added listener %y (SSL: %y, HTTP version: %y)", uh, ssl, http_version);
    }

    #! Removes a listener
    /** @param listener the listener to remove
    */
    removeListener(Socket listener) {
        string uh = listener.uniqueHash();
        bool signal = False;
        {
            AutoLock al(m);
            if (!listeners{uh}) {
                throw "LISTENER-NOT-FOUND", sprintf("Listener %y not registered", uh);
            }
            signal = sendCmdLocked(CMD_REMOVE_LISTENER, uh);
        }
        if (signal) {
            sem_write.write(".");
        }
        log(LoggerLevel::INFO, "Removed listener %y", uh);
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
        bool signal = False;
        {
            AutoLock al(m);
            connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": listener,
                "cx": cx,
                "target": target,
                "state": "idle",
                "ttl": ttl,
            };
            signal = sendCmdLocked(CMD_ADD_IDLE, uh);
        }
        if (signal) {
            sem_write.write(".");
        }
        log(LoggerLevel::DEBUG, "Added idle connection %y", uh);
    }

    #! Gets controller statistics
    hash<auto> getStats() {
        AutoLock al(m);
        return {
            "listeners": listeners.size(),
            "connections": connections.size(),
            "running": tid ? True : False,
        };
    }

    #! Main I/O thread
    private ioThread() {
        log(LoggerLevel::INFO, "HTTP async I/O controller started");
        on_exit {
            {
                AutoLock al(m);
                remove tid;
            }
            log(LoggerLevel::INFO, "HTTP async I/O controller stopped");
            mcnt.dec();
        }

        # Active poll operations: socket unique hash -> poll info
        hash<string, hash<HttpPollInfo>> active_polls;

        hash<string, bool> ready_sockets;
        while (!stop_flag) {
            try {
                date poll_timeout = DefaultPollTimeout;

                {
                    AutoLock al(m);

                    # Start accept operations for listeners (allow multiple in-flight accepts)
                    log(LoggerLevel::DEBUG, "ioThread: checking %d listeners", listeners.size());
                    foreach string key in (keys listeners) {
                        int cnt = accept_counts{key} ?? 0;
                        log(LoggerLevel::DEBUG, "ioThread: listener %s, accept_ops=%d", key, cnt);
                        while (cnt < DefaultAcceptOps) {
                            hash<HttpListenerInfo> linfo = listeners{key};
                            try {
                                log(LoggerLevel::DEBUG, "Starting accept op for listener %s", key);
                                HttpAcceptPollOperation op(linfo.listener, linfo.ssl,
                                    linfo.cert, linfo.key, SSL_VERIFY_NONE, True, linfo.http_version, False);
                                @debug {
                                    if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                                        log(LoggerLevel::INFO,
                                            "ioThread: accept op created: listener=%s ssl=%y http_version=%s",
                                            key, linfo.ssl, linfo.http_version);
                                    }
                                }
                                string accept_key = sprintf("accept:%s:%d", key, ++accept_seq);
                                active_polls{accept_key} = <HttpPollInfo>{
                                    "type": "accept",
                                    "op": op,
                                    "listener_key": key,
                                    "linfo": linfo,
                                };
                                cnt = ++accept_counts{key};
                                log(LoggerLevel::DEBUG,
                                    "Accept op created for listener %s (HTTP version: %s, count=%d)",
                                    key, linfo.http_version, cnt);
                            } catch (hash<ExceptionInfo> ex) {
                                log(LoggerLevel::ERROR, "Failed to start accept: %s: %s",
                                    ex.err, ex.desc);
                                break;
                            }
                        }
                    }

                    # Start operations for connections
                    foreach string key in (keys connections) {
                        if (!active_polls{key}) {
                            hash<HttpActiveConnectionInfo> cinfo = connections{key};
                            # Check if socket is still open
                            if (!cinfo.sock.isOpen()) {
                                remove connections{key};
                                continue;
                            }
                            switch (cinfo.state) {
                                case "idle": {
                                    HttpIdlePollOperation op(cinfo.sock, cinfo.ttl);
                                    active_polls{key} = <HttpPollInfo>{
                                        "type": "idle",
                                        "op": op,
                                        "conn_key": key,
                                        "cinfo": cinfo,
                                    };
                                    break;
                                }
                                case "read_header": {
                                    auto spop = cinfo.sock.startPollReadHttpHeader();
                                    active_polls{key} = <HttpPollInfo>{
                                        "type": "header",
                                        "op": spop,
                                        "conn_key": key,
                                        "cinfo": cinfo,
                                    };
                                    break;
                                }
                                case "ssl_handshake": {
                                    if (cinfo.ssl_op) {
                                        active_polls{key} = <HttpPollInfo>{
                                            "type": "ssl",
                                            "op": cinfo.ssl_op,
                                            "conn_key": key,
                                            "cinfo": cinfo,
                                        };
                                    }
                                    break;
                                }
                                case "sse": {
                                    if (cinfo.stream_op) {
                                        active_polls{key} = <HttpPollInfo>{
                                            "type": "sse",
                                            "op": cinfo.stream_op,
                                            "conn_key": key,
                                            "cinfo": cinfo,
                                        };
                                    }
                                    break;
                                }
                                case "websocket": {
                                    if (cinfo.stream_op) {
                                        active_polls{key} = <HttpPollInfo>{
                                            "type": "websocket",
                                            "op": cinfo.stream_op,
                                            "conn_key": key,
                                            "cinfo": cinfo,
                                        };
                                    }
                                    break;
                                }
                                case "http2": {
                                    if (cinfo.stream_op) {
                                        active_polls{key} = <HttpPollInfo>{
                                            "type": "http2",
                                            "op": cinfo.stream_op,
                                            "conn_key": key,
                                            "cinfo": cinfo,
                                        };
                                    }
                                    break;
                                }

                                case "http2_sse": {
                                    if (cinfo.stream_op) {
                                        active_polls{key} = <HttpPollInfo>{
                                            "type": "http2_sse",
                                            "op": cinfo.stream_op,
                                            "conn_key": key,
                                            "cinfo": cinfo,
                                        };
                                    }
                                    break;
                                }

                                case "http2_websocket": {
                                    if (cinfo.stream_op) {
                                        active_polls{key} = <HttpPollInfo>{
                                            "type": "http2_websocket",
                                            "op": cinfo.stream_op,
                                            "conn_key": key,
                                            "cinfo": cinfo,
                                        };
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }

                # Continue all poll operations
                hash<string, bool> to_remove;
                log(LoggerLevel::DEBUG, "ioThread: continuing %d active polls", active_polls.size());
                foreach string key in (keys active_polls) {
                    hash<HttpPollInfo> pinfo = active_polls{key};
                    try {
                        if (pinfo.poll_info && pinfo.poll_info.socket
                            && !(ready_sockets{pinfo.poll_info.socket.uniqueHash()} ?? False)) {
                            @debug {
                                if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                                    log(LoggerLevel::INFO,
                                        "ioThread: skip continuePoll for %s; socket=%s",
                                        pinfo.type, sprintf("%s %s", pinfo.poll_info.socket.className(),
                                            pinfo.poll_info.socket.uniqueHash()[0..8]));
                                }
                            }
                            # Socket already registered in EventLoop; skip continuePoll
                            continue;
                        }
                        *hash<SocketPollInfo> info = pinfo.op.continuePoll();
                        if (info) {
                            pinfo.poll_info = info;
                            active_polls{key} = pinfo;
                            @debug(log(LoggerLevel::DEBUG, "ioThread: poll %s needs events=%d", pinfo.type, info.events));
                        } else {
                            # Operation complete
                            @debug(log(LoggerLevel::DEBUG, "ioThread: poll %s complete", pinfo.type));
                            handlePollComplete(pinfo);
                            to_remove{key} = True;
                        }
                    } catch (hash<ExceptionInfo> ex) {
                        if ((pinfo.type == "idle" || pinfo.type == "header")
                            && ex.err == "SOCKET-HTTP-ERROR"
                            && ex.desc =~ /remote end closed connection/) {
                            log(LoggerLevel::DEBUG, "Poll closed for %y: %s: %s", key, ex.err, ex.desc);
                        } else {
                            log(LoggerLevel::ERROR, "Poll error for %y: %s: %s", key, ex.err, ex.desc);
                        }
                        handlePollError(pinfo, ex);
                        to_remove{key} = True;
                    }
                }

                # Remove completed operations and unregister from EventLoop
                foreach string key in (keys to_remove) {
                    updateEventLoopRegistration(key, NOTHING);
                    remove active_polls{key};
                }

                # Update EventLoop registrations for all active polls
                foreach string key in (keys active_polls) {
                    hash<HttpPollInfo> pinfo = active_polls{key};
                    if (pinfo.poll_info && pinfo.poll_info.socket) {
                        updateEventLoopRegistration(key, pinfo.poll_info.socket, pinfo.poll_info.events);
                    }
                }

                # Perform the poll using persistent EventLoop (kqueue on macOS, epoll on Linux)
                *list<hash<SocketPollInfo>> ready_list = loop.poll(poll_timeout);
                ready_sockets = {};
                if (ready_list) {
                    foreach hash<SocketPollInfo> info in (ready_list) {
                        if (info.socket) {
                            ready_sockets{info.socket.uniqueHash()} = True;
                        }
                    }
                }
                @debug {
                    if (getenv("QORE_HTTP_ACCEPT_TRACE")) {
                        log(LoggerLevel::INFO, "ioThread: poll ready_list: %y", ready_list ? (map $1 + {
                            "events": PollEventMap{$1.events} ?? "unknown",
                            "socket": sprintf("%s %s", $1.socket.className(),
                                $1.socket.uniqueHash()[0..8]),
                        }, ready_list) : "(empty)");
                    }
                }

                # Process commands
                if (processCommands()) {
                    break;
                }

            } catch (hash<ExceptionInfo> ex) {
                # Handle SOCKET-NOT-OPEN gracefully
                if (ex.err == "SOCKET-NOT-OPEN") {
                    @debug(log(LoggerLevel::DEBUG, "Socket closed during poll: %s", ex.desc));
                } else {
                    log(LoggerLevel::ERROR, "I/O thread error: %s: %s", ex.err, ex.desc);
                }
            }
        }
    }

    #! Handles a completed poll operation
    private handlePollComplete(hash<HttpPollInfo> pinfo) {
        switch (pinfo.type) {
            case "accept": {
                if (pinfo.listener_key) {
                    int cnt = accept_counts{pinfo.listener_key} ?? 0;
                    if (cnt > 0) {
                        accept_counts{pinfo.listener_key} = cnt - 1;
                    }
                }
                hash<auto> output = pinfo.op.getOutput();
                handleAcceptComplete(pinfo.linfo, output);
                break;
            }

            case "ssl": {
                handleSslHandshakeComplete(pinfo);
                break;
            }

            case "idle": {
                HttpIdlePollOperation op = pinfo.op;
                if (op.timedOut()) {
                    handleIdleTimeout(pinfo);
                } else if (op.hasData()) {
                    handleIdleDataReady(pinfo);
                } else {
                    # Connection closed
                    handleConnectionClosed(pinfo);
                }
                break;
            }

            case "header": {
                hash<auto> output = pinfo.op.getOutput();
                handleHeaderComplete(pinfo, output);
                break;
            }

            case "sse": {
                HttpSseStreamPollOperation op = pinfo.op;
                if (op.isDisconnected()) {
                    handleSseDisconnect(pinfo);
                }
                break;
            }

            case "websocket": {
                HttpWebSocketPollOperation op = pinfo.op;
                if (op.goalReached()) {
                    handleWebSocketClose(pinfo, op);
                }
                break;
            }

            case "http2": {
                Http2PollOperation h2op = pinfo.op;
                if (h2op.goalReached()) {
                    handleHttp2RequestReady(pinfo, h2op);
                } else if (h2op.isClosed() || h2op.hasError()) {
                    handleHttp2ConnectionClosed(pinfo, h2op);
                }
                break;
            }

            case "http2_sse": {
                Http2SseStream sse_stream = cast<Http2SseStream>(pinfo.op);
                if (sse_stream.isDisconnected()) {
                    handleHttp2SseDisconnect(pinfo, sse_stream);
                }
                break;
            }

            case "http2_websocket": {
                Http2WebSocketStream ws_stream = cast<Http2WebSocketStream>(pinfo.op);
                if (!ws_stream.isOpen()) {
                    handleHttp2WebSocketClose(pinfo, ws_stream);
                }
                break;
            }
        }
    }

    #! Handles accept completion
    private handleAcceptComplete(hash<HttpListenerInfo> linfo, hash<auto> output) {
        Socket sock = output.sock;
        string uh = sock.uniqueHash();

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
            if (output.http_version == "auto") {
                alpn = ("h2", "http/1.1");
            } else if (output.http_version == "2.0" || output.http_version == "2") {
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

            {
                AutoLock al(m);
                connections{uh} = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": linfo.target,
                    "state": "ssl_handshake",
                    "ssl_op": ssl_op,
                    "http_version": output.http_version,
                    "is_http2": False,
                };
            }
            return;
        }

        # Check if this is an HTTP/2 connection
        if (output.is_http2) {
            # For HTTP/2, we need to set up the connection for frame processing
            log(LoggerLevel::DEBUG, "Starting HTTP/2 connection handling for %y", uh);
            hash<auto> cx = {
                "peer": sock.getPeerInfo(),
                "socket": sock.getSocketInfo(),
            };

            # Create HTTP/2 poll operation
            Http2PollOperation h2op(sock);

            # Register this connection
            {
                AutoLock al(m);
                connections{uh} = <HttpActiveConnectionInfo>{
                    "sock": sock,
                    "listener": linfo.listener,
                    "cx": cx,
                    "target": linfo.target,
                    "state": "http2",
                    "stream_op": h2op,
                    "is_http2": True,
                };
            }
            return;
        }

        # HTTP/1.x path: the accept poll operation doesn't read HTTP headers yet,
        # so we need to start header reading for new connections
        hash<auto> cx = {
            "peer": sock.getPeerInfo(),
            "socket": sock.getSocketInfo(),
        };

        log(LoggerLevel::DEBUG, "Starting HTTP/1.x header reading for %y", uh);

        # Register connection for header reading
        {
            AutoLock al(m);
            connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": linfo.listener,
                "cx": cx,
                "target": linfo.target,
                "state": "read_header",
                "http_version": output.http_version,
                "is_http2": False,
            };
        }
    }

    #! Handles SSL handshake completion
    private handleSslHandshakeComplete(hash<HttpPollInfo> pinfo) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        cinfo.ssl_op = NOTHING;

        string negotiated = cinfo.sock.getAlpnProtocol();
        bool is_http2 = (negotiated == "h2");

        if ((cinfo.http_version == "2.0" || cinfo.http_version == "2") && !is_http2) {
            log(LoggerLevel::WARN, "HTTP/2 required but client negotiated: %y", negotiated ?? "none");
            try {
                cinfo.sock.close();
            } catch () {}
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

        if (is_http2) {
            Http2PollOperation h2op(cinfo.sock);
            {
                AutoLock al(m);
                connections{uh}.state = "http2";
                connections{uh}.stream_op = h2op;
                connections{uh}.is_http2 = True;
            }
            return;
        }

        {
            AutoLock al(m);
            connections{uh}.state = "read_header";
            connections{uh}.is_http2 = False;
        }
    }

    #! Handles header read completion
    private handleHeaderComplete(hash<HttpPollInfo> pinfo, hash<auto> output) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

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
            try {
                cinfo.sock.close();
            } catch () {}
        }
    }

    #! Handles the result of request processing
    private handleRequestResult(Socket sock, auto linfo, hash<auto> cx,
            hash<HttpRequestResultInfo> result, HttpAsyncTarget target) {
        string uh = sock.uniqueHash();

        if (result.close) {
            # Close the connection
            try {
                sock.close();
            } catch () {}
            return;
        }

        if (result.streaming) {
            # Set up streaming
            AutoLock al(m);
            connections{uh} = <HttpActiveConnectionInfo>{
                "sock": sock,
                "listener": linfo.listener ?? linfo,
                "cx": cx,
                "target": target,
                "state": result.stream_type,
                "stream_op": result.stream_operation,
                "ttl": result.ttl ?? DefaultIdleTimeout,
            };
            return;
        }

        if (result.return_to_idle) {
            # Add back to idle pool
            addIdleConnection(sock, linfo.listener ?? linfo, cx, target,
                result.ttl ?? DefaultIdleTimeout);
        }
    }

    #! Handles idle timeout
    private handleIdleTimeout(hash<HttpPollInfo> pinfo) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "Idle timeout: %y", uh);

        {
            AutoLock al(m);
            remove connections{uh};
        }

        cinfo.target.onIdleTimeout(cinfo.sock, cinfo.listener, cinfo.cx);
    }

    #! Handles data ready on idle connection
    private handleIdleDataReady(hash<HttpPollInfo> pinfo) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "Data ready on idle: %y", uh);

        # Transition to header reading
        {
            AutoLock al(m);
            connections{uh}.state = "read_header";
        }
    }

    #! Handles connection closed
    private handleConnectionClosed(hash<HttpPollInfo> pinfo) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "Connection closed: %y", uh);

        {
            AutoLock al(m);
            remove connections{uh};
        }
    }

    #! Handles SSE disconnect
    private handleSseDisconnect(hash<HttpPollInfo> pinfo) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "SSE disconnect: %y", uh);

        {
            AutoLock al(m);
            remove connections{uh};
        }

        cinfo.target.onSseDisconnect(cinfo.sock, cinfo.listener, cinfo.cx);
    }

    #! Handles WebSocket close
    private handleWebSocketClose(hash<HttpPollInfo> pinfo, HttpWebSocketPollOperation op) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "WebSocket close: %y (code: %d)", uh, op.getCloseCode());

        {
            AutoLock al(m);
            remove connections{uh};
        }

        cinfo.target.onWebSocketClose(cinfo.sock, op.getCloseCode(),
            op.getCloseReason(), cinfo.listener, cinfo.cx);
    }

    #! Handles HTTP/2 request ready
    private handleHttp2RequestReady(hash<HttpPollInfo> pinfo, Http2PollOperation h2op) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();
        hash<auto> request = h2op.getOutput();

        log(LoggerLevel::DEBUG, "HTTP/2 request ready: %y stream=%d %s %s",
            uh, request.stream_id, request.method, request.path);

        # Check for RFC 8441 WebSocket over HTTP/2 (CONNECT with :protocol=websocket)
        bool is_websocket_connect = request.method == "CONNECT" &&
            request.headers.":protocol" == "websocket";

        # Parse accept-encoding header for compression support
        *list<string> accept_encoding;
        if (request.headers."accept-encoding") {
            # Parse comma-separated list, strip quality values (e.g., "gzip, br;q=1.0")
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
                # Map :authority to host for HTTP/1.x compatibility (many handlers expect host header)
                "host": request.authority,
                # Set HTTP version for logging and version checks
                "http_version": "HTTP/2",
            },
            "header_info": {
                "http2": True,
                "stream_id": request.stream_id,
                "is_websocket_connect": is_websocket_connect,
                "push_resource": h2op.getPushResourceCallback(),
                "h2op": h2op,  # Pass for startSendPushedResponse()
                "accept-encoding": accept_encoding,
            },
            "listener": cinfo.listener,
            "cx": cinfo.cx,
            "keep_alive": True,  # HTTP/2 is always keep-alive
            "ssl": cinfo.sock.isSecure(),
        };

        # Call the target to handle the request
        try {
            hash<HttpRequestResultInfo> result = cinfo.target.onHttpRequest(conn_info);
            handleHttp2RequestResult(h2op, cinfo, request, result, is_websocket_connect);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error handling HTTP/2 request: %s: %s", ex.err, ex.desc);
            cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
            # Close the connection on error
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try {
                cinfo.sock.close();
            } catch () {}
        }
    }

    #! Handles the result of an HTTP/2 request
    private handleHttp2RequestResult(Http2PollOperation h2op, hash<HttpActiveConnectionInfo> cinfo,
            hash<auto> request, hash<HttpRequestResultInfo> result, bool is_websocket_connect = False) {
        string uh = cinfo.sock.uniqueHash();

        if (result.close) {
            # Close the connection
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try {
                cinfo.sock.close();
            } catch () {}
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

                    # RFC 8441: Send 200 OK response for WebSocket CONNECT
                    # The response must be sent before WebSocket data can be exchanged
                    h2op.startSendResponse(result.code ?? 200, result.hdr);

                    # Drive the send operation to completion
                    while (True) {
                        *hash<SocketPollInfo> poll_info = h2op.continuePoll();
                        if (!poll_info) {
                            break;
                        }
                        Socket::poll((poll_info,), 100ms);
                    }

                    # Create HTTP/2 WebSocket stream handler
                    Http2WebSocketStream ws_stream(h2op, request.stream_id);
                    {
                        AutoLock al(m);
                        connections{uh}.stream_op = result.stream_operation ?? ws_stream;
                        connections{uh}.state = "http2_websocket";
                    }
                    return;
                }

                case "sse": {
                    log(LoggerLevel::DEBUG, "HTTP/2 SSE stream started: %y stream=%d",
                        uh, request.stream_id);
                    # Create HTTP/2 SSE stream handler
                    Http2SseStream sse_stream(h2op, request.stream_id);
                    {
                        AutoLock al(m);
                        connections{uh}.stream_op = result.stream_operation ?? sse_stream;
                        connections{uh}.state = "http2_sse";
                    }
                    return;
                }
            }
        }

        # For HTTP/2, we keep the connection alive for more requests
        # The response should already have been sent by the handler

        # Prepare for next request
        if (h2op.shouldKeepAlive()) {
            log(LoggerLevel::DEBUG, "HTTP/2 connection %y waiting for next request", uh);
            h2op.startReadNextRequest();
            {
                AutoLock al(m);
                connections{uh}.stream_op = h2op;
                connections{uh}.state = "http2";
            }
        } else {
            log(LoggerLevel::DEBUG, "HTTP/2 connection %y closing", uh);
            {
                AutoLock al(m);
                remove connections{uh};
            }
            try {
                cinfo.sock.close();
            } catch () {}
        }
    }

    #! Handles HTTP/2 connection closed
    private handleHttp2ConnectionClosed(hash<HttpPollInfo> pinfo, Http2PollOperation h2op) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        *hash<auto> error = h2op.getError();
        if (error) {
            log(LoggerLevel::DEBUG, "HTTP/2 connection closed with error: %y (%s: %s)",
                uh, error.err, error.desc);
            cinfo.target.onError(cinfo.sock, cast<hash<ExceptionInfo>>(error),
                cinfo.listener, cinfo.cx);
        } else {
            log(LoggerLevel::DEBUG, "HTTP/2 connection closed: %y", uh);
        }

        {
            AutoLock al(m);
            remove connections{uh};
        }
    }

    #! Handles HTTP/2 SSE disconnect
    private handleHttp2SseDisconnect(hash<HttpPollInfo> pinfo, Http2SseStream sse_stream) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "HTTP/2 SSE disconnect: %y stream=%d", uh, sse_stream.getStreamId());

        {
            AutoLock al(m);
            remove connections{uh};
        }

        # Notify the target of SSE disconnect
        cinfo.target.onSseDisconnect(cinfo.sock, cinfo.listener, cinfo.cx);
    }

    #! Handles HTTP/2 WebSocket close
    private handleHttp2WebSocketClose(hash<HttpPollInfo> pinfo, Http2WebSocketStream ws_stream) {
        hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
        string uh = cinfo.sock.uniqueHash();

        log(LoggerLevel::DEBUG, "HTTP/2 WebSocket close: %y stream=%d (code: %d)",
            uh, ws_stream.getStreamId(), ws_stream.getCloseCode());

        {
            AutoLock al(m);
            remove connections{uh};
        }

        # Notify the target of WebSocket close
        cinfo.target.onWebSocketClose(cinfo.sock, ws_stream.getCloseCode(),
            ws_stream.getCloseReason(), cinfo.listener, cinfo.cx);
    }

    #! Handles poll error
    private handlePollError(hash<HttpPollInfo> pinfo, hash<ExceptionInfo> ex) {
        switch (pinfo.type) {
            case "accept":
                if (pinfo.listener_key) {
                    int cnt = accept_counts{pinfo.listener_key} ?? 0;
                    if (cnt > 0) {
                        accept_counts{pinfo.listener_key} = cnt - 1;
                    }
                }
                # Accept errors - log but continue
                log(LoggerLevel::WARN, "Accept error: %s: %s", ex.err, ex.desc);
                break;

            case "ssl":
            case "idle":
            case "header":
            case "sse":
            case "websocket":
            case "http2":
            case "http2_sse":
            case "http2_websocket": {
                hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
                string uh = cinfo.sock.uniqueHash();

                @debug {
                    if (getenv("QORE_ASYNCIO_TRACE")) {
                        log(LoggerLevel::INFO,
                            "poll error: type=%y state=%y conn=%y err=%s desc=%s",
                            pinfo.type, cinfo.state, uh, ex.err, ex.desc);
                    }
                }

                if ((pinfo.type == "idle" || pinfo.type == "header")
                    && ex.err == "SOCKET-HTTP-ERROR"
                    && ex.desc =~ /remote end closed connection/) {
                    log(LoggerLevel::DEBUG, "Connection closed by peer during read: %y", uh);
                    handleConnectionClosed(pinfo);
                    break;
                }

                {
                    AutoLock al(m);
                    remove connections{uh};
                }

                cinfo.target.onError(cinfo.sock, ex, cinfo.listener, cinfo.cx);
                try {
                    cinfo.sock.close();
                } catch () {}
                break;
            }
        }
    }

    #! Processes commands from the queue
    private bool processCommands() {
        # Drain the signaling pipe
        while (True) {
            try {
                binary data = sem_read.readShortBinary(4096, 0);
                if (!data.size()) {
                    break;
                }
            } catch (hash<ExceptionInfo> ex) {
                if (ex.err != "FILE-READ-TIMEOUT") {
                    rethrow;
                }
                break;
            }
        }
        # No need to resignal - with "always signal" on enqueue, the pipe
        # is already written to when commands are added. Resignaling here
        # would cause double-signaling and busy loops.

        # Process command queue
        try {
            while (hash<auto> cmd = cmdq.get(-1)) {
                switch (cmd.cmd) {
                    case CMD_QUIT:
                        return True;

                    case CMD_ADD_LISTENER:
                    case CMD_REMOVE_LISTENER:
                    case CMD_ADD_IDLE:
                        # These are processed in the main loop automatically
                        break;
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "QUEUE-TIMEOUT") {
                rethrow;
            }
        }

        return False;
    }

    #! Sends a command to the I/O thread (caller must hold m)
    private bool sendCmdLocked(string cmd) {
        @assert(m.trylock() == -1);
        cmdq.push({
            "cmd": cmd,
            "args": argv,
        });
        # Always signal the I/O thread to avoid race conditions with sem_pending
        return tid ? True : False;
    }


    #! Updates EventLoop registration for a socket
    /** Uses reference counting to handle multiple operations sharing the same socket
        (e.g., multiple concurrent accept operations on the same listener socket)
    */
    private updateEventLoopRegistration(string key, *Socket socket, *int events) {
        *Socket prev_sock = registered_sockets{key};
        string prev_sock_hash = prev_sock ? prev_sock.uniqueHash() : "";
        string new_sock_hash = socket ? socket.uniqueHash() : "";

        if (!socket) {
            # Remove registration for this key
            if (prev_sock) {
                remove registered_sockets{key};
                # Decrement reference count
                int refcount = socket_refcounts{prev_sock_hash} ?? 0;
                if (refcount <= 1) {
                    # Last reference - actually remove from EventLoop
                    try {
                        loop.del(prev_sock);
                    } catch (hash<ExceptionInfo> ex) {
                        # Ignore errors - socket may be closed
                    }
                    remove registered_events{prev_sock_hash};
                    remove socket_refcounts{prev_sock_hash};
                } else {
                    # Other operations still using this socket
                    socket_refcounts{prev_sock_hash} = refcount - 1;
                }
            }
            return;
        }

        int prev_events = registered_events{new_sock_hash} ?? 0;

        if (new_sock_hash == prev_sock_hash) {
            # Same socket - check if events changed
            if (events != prev_events) {
                loop.modify(socket, events);
                registered_events{new_sock_hash} = events;
            }
        } else {
            # Different socket or new registration
            if (prev_sock) {
                # Decrement refcount for previous socket
                int old_refcount = socket_refcounts{prev_sock_hash} ?? 0;
                if (old_refcount <= 1) {
                    try {
                        loop.del(prev_sock);
                    } catch (hash<ExceptionInfo> ex) {
                        # Ignore
                    }
                    remove registered_events{prev_sock_hash};
                    remove socket_refcounts{prev_sock_hash};
                } else {
                    socket_refcounts{prev_sock_hash} = old_refcount - 1;
                }
            }
            # Add/update new socket
            int new_refcount = socket_refcounts{new_sock_hash} ?? 0;
            if (new_refcount == 0) {
                # First registration for this socket
                loop.add(socket, events);
                socket_refcounts{new_sock_hash} = 1;
            } else {
                # Socket already registered by another operation
                socket_refcounts{new_sock_hash} = new_refcount + 1;
                if (events != prev_events) {
                    loop.modify(socket, events);
                }
            }
            registered_sockets{key} = socket;
            registered_events{new_sock_hash} = events;
        }
    }

    #! Logs a message
    private log(int level, string fmt) {
        if (logger) {
            logger.log(level, vsprintf(fmt, argv));
        }
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

    #! HTTP version mode: "auto", "1.0", "1.1", or "2.0"
    string http_version = "auto";
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

    #! Connection state: "idle", "read_header", "ssl_handshake", "sse", "websocket", "http2", "http2_sse", "http2_websocket"
    string state = "idle";

    #! SSL handshake poll operation (for "ssl_handshake" state)
    *AbstractPollOperation ssl_op;

    #! Stream operation (for SSE/WebSocket/HTTP2)
    *AbstractPollOperation stream_op;

    #! Keep-alive timeout
    timeout ttl = DefaultIdleTimeout;

    #! HTTP version mode: "auto", "1.0", "1.1", or "2.0"
    string http_version = "auto";

    #! True if this is an HTTP/2 connection
    bool is_http2 = False;
}

#! Internal hashdecl for poll operation tracking
hashdecl HttpPollInfo {
    #! Poll type: "accept", "ssl", "idle", "header", "sse", "websocket", "http2", "http2_sse", "http2_websocket"
    string type;

    #! The poll operation
    AbstractPollOperation op;

    #! Listener key (for accept operations)
    *string listener_key;

    #! Connection key (for connection operations)
    *string conn_key;

    #! Listener info (for accept operations)
    *hash<HttpListenerInfo> linfo;

    #! Connection info (for connection operations)
    *hash<HttpActiveConnectionInfo> cinfo;

    #! Last poll info for this operation
    *hash<SocketPollInfo> poll_info;
}

} # namespace Priv
#! @endcond
