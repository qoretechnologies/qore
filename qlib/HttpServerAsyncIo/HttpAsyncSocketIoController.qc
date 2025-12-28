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
    }

    private {
        #! Lock for atomic operations
        Mutex m();

        #! The underlying async I/O controller
        AsyncSocketIoController asyncCtrl;

        #! Logger
        *LoggerInterface logger;

        #! Registered listeners: listener unique hash -> listener info
        hash<string, hash<HttpListenerInfo>> listeners;

        #! Active connections: socket unique hash -> connection info
        hash<string, hash<HttpActiveConnectionInfo>> connections;

        #! I/O thread TID
        int tid;

        #! I/O thread counter
        Counter mcnt();

        #! Stop flag
        bool stop_flag = False;

        #! File for inter-thread signaling
        File sem_write;
        ReadOnlyFile sem_read;

        #! Command queue
        Queue cmdq();

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
        {
            AutoLock al(m);
            if (!tid) {
                return;
            }
            stop_flag = True;
            sendCmd(CMD_QUIT);
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
    */
    addListener(Socket listener, HttpAsyncTarget target, bool ssl = False, *data cert, *data key) {
        string uh = listener.uniqueHash();
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
            };
            sendCmd(CMD_ADD_LISTENER, uh);
        }
        log(LoggerLevel::INFO, "Added listener %y (SSL: %y)", uh, ssl);
    }

    #! Removes a listener
    /** @param listener the listener to remove
    */
    removeListener(Socket listener) {
        string uh = listener.uniqueHash();
        {
            AutoLock al(m);
            if (!listeners{uh}) {
                throw "LISTENER-NOT-FOUND", sprintf("Listener %y not registered", uh);
            }
            sendCmd(CMD_REMOVE_LISTENER, uh);
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
            sendCmd(CMD_ADD_IDLE, uh);
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

        # Poll info for the signaling pipe
        hash<SocketPollInfo> sem_poll = <SocketPollInfo>{
            "events": SOCK_POLLIN,
            "socket": sem_read,
        };

        # Active poll operations: socket unique hash -> poll info
        hash<string, hash<HttpPollInfo>> active_polls;

        while (!stop_flag) {
            try {
                list<hash<SocketPollInfo>> poll_list = (sem_poll,);
                date poll_timeout = DefaultPollTimeout;

                {
                    AutoLock al(m);

                    # Start accept operations for listeners without active accepts
                    log(LoggerLevel::DEBUG, "ioThread: checking %d listeners", listeners.size());
                    foreach string key in (keys listeners) {
                        log(LoggerLevel::DEBUG, "ioThread: listener %s, active_polls has key=%y", key, active_polls.hasKey(key));
                        if (!active_polls{key}) {
                            hash<HttpListenerInfo> linfo = listeners{key};
                            try {
                                log(LoggerLevel::DEBUG, "Starting accept op for listener %s", key);
                                HttpAcceptPollOperation op(linfo.listener, linfo.ssl,
                                    linfo.cert, linfo.key);
                                active_polls{key} = <HttpPollInfo>{
                                    "type": "accept",
                                    "op": op,
                                    "listener_key": key,
                                    "linfo": linfo,
                                };
                                log(LoggerLevel::DEBUG, "Accept op created for listener %s", key);
                            } catch (hash<ExceptionInfo> ex) {
                                log(LoggerLevel::ERROR, "Failed to start accept: %s: %s",
                                    ex.err, ex.desc);
                            }
                        }
                    }

                    # Start operations for connections
                    foreach string key in (keys connections) {
                        if (!active_polls{key}) {
                            hash<HttpActiveConnectionInfo> cinfo = connections{key};
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
                        *hash<SocketPollInfo> info = pinfo.op.continuePoll();
                        if (info) {
                            poll_list += info;
                            @debug(log(LoggerLevel::DEBUG, "ioThread: poll %s added poll info, list size=%d", pinfo.type, poll_list.size()));
                        } else {
                            # Operation complete
                            @debug(log(LoggerLevel::DEBUG, "ioThread: poll %s complete", pinfo.type));
                            handlePollComplete(pinfo);
                            to_remove{key} = True;
                        }
                    } catch (hash<ExceptionInfo> ex) {
                        log(LoggerLevel::ERROR, "Poll error for %y: %s: %s", key, ex.err, ex.desc);
                        handlePollError(pinfo, ex);
                        to_remove{key} = True;
                    }
                }

                # Remove completed operations
                map remove active_polls{$1}, keys to_remove;

                # Perform the poll
                Socket::poll(poll_list, poll_timeout);

                # Process commands
                if (processCommands()) {
                    break;
                }

            } catch (hash<ExceptionInfo> ex) {
                log(LoggerLevel::ERROR, "I/O thread error: %s: %s", ex.err, ex.desc);
            }
        }
    }

    #! Handles a completed poll operation
    private handlePollComplete(hash<HttpPollInfo> pinfo) {
        switch (pinfo.type) {
            case "accept": {
                hash<auto> output = pinfo.op.getOutput();
                handleAcceptComplete(pinfo.linfo, output);
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
        }
    }

    #! Handles accept completion
    private handleAcceptComplete(hash<HttpListenerInfo> linfo, hash<auto> output) {
        Socket sock = output.sock;
        string uh = sock.uniqueHash();

        # Create connection info
        hash<HttpConnectionInfo> conn_info = <HttpConnectionInfo>{
            "sock": sock,
            "hdr": output.hdr,
            "header_info": output.header_info,
            "listener": linfo.listener,
            "cx": {
                "peer": sock.getPeerInfo(),
                "socket": sock.getSocketInfo(),
            },
            "keep_alive": False,
            "ssl": output.ssl,
        };

        log(LoggerLevel::DEBUG, "Accept complete: %y (SSL: %y)", uh, output.ssl);

        # Call the target
        try {
            hash<HttpRequestResultInfo> result = linfo.target.onHttpRequest(conn_info);
            handleRequestResult(sock, linfo, conn_info.cx, result, linfo.target);
        } catch (hash<ExceptionInfo> ex) {
            log(LoggerLevel::ERROR, "Error handling request: %s: %s", ex.err, ex.desc);
            linfo.target.onError(sock, ex, linfo.listener, conn_info.cx);
            try {
                sock.close();
            } catch () {}
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

    #! Handles poll error
    private handlePollError(hash<HttpPollInfo> pinfo, hash<ExceptionInfo> ex) {
        switch (pinfo.type) {
            case "accept":
                # Accept errors - log but continue
                log(LoggerLevel::WARN, "Accept error: %s: %s", ex.err, ex.desc);
                break;

            case "idle":
            case "header":
            case "sse":
            case "websocket": {
                hash<HttpActiveConnectionInfo> cinfo = pinfo.cinfo;
                string uh = cinfo.sock.uniqueHash();

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
        try {
            sem_read.read(-1, 0);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err != "FILE-READ-TIMEOUT") {
                rethrow;
            }
        }

        # Process command queue
        try {
            while (hash<auto> cmd = cmdq.get(0ms)) {
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

    #! Sends a command to the I/O thread
    private sendCmd(string cmd) {
        cmdq.push({
            "cmd": cmd,
            "args": argv,
        });
        if (tid) {
            sem_write.write(".");
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

    #! Connection state: "idle", "read_header", "sse", "websocket"
    string state = "idle";

    #! Stream operation (for SSE/WebSocket)
    *AbstractPollOperation stream_op;

    #! Keep-alive timeout
    timeout ttl = DefaultIdleTimeout;
}

#! Internal hashdecl for poll operation tracking
hashdecl HttpPollInfo {
    #! Poll type: "accept", "idle", "header", "sse", "websocket"
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
}

} # namespace Priv
#! @endcond
