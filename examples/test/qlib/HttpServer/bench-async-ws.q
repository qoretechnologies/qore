#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
#
# WebSocket echo benchmark server for measuring async I/O throughput.
#
# Usage:
#   QORE_MODULE_DIR=qlib qore bench-async-ws.q [options]
#
# Options:
#   --port=N         Listen port (default 0 = auto)
#   --duration=N     Run for N seconds then exit (default: run until killed)
#   --quiet          Suppress info messages
#   --port-file=F    Write the bound port to this file
#
# The server echoes every WebSocket text/binary message back to the sender.

%modern

%requires ../../../../qlib/Util.qm
%requires ../../../../qlib/Mime.qm
%requires ../../../../qlib/Logger.qm
%requires ../../../../qlib/HttpServerUtil.qm
%requires ../../../../qlib/HttpServerAsyncIo
%requires ../../../../qlib/HttpServer.qm
%requires ../../../../qlib/WebSocketUtil.qm
%requires ../../../../qlib/WebSocketHandler.qm

%exec-class WsBenchServer

class EchoWebSocketHandler inherits WebSocketHandler {
    constructor() : WebSocketHandler() {
    }

    WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new EchoWebSocketConnection(self);
    }
}

class EchoWebSocketConnection inherits WebSocketConnection {
    constructor(WebSocketHandler handler) : WebSocketConnection(handler) {
    }

    #! Called when the connection is registered — enable C++ echo mode
    registered() {
        # Enable C++ I/O-thread echo: frames are echoed back in pure C++
        # with zero context switches (no gotMessage callback)
        setEchoMode();
    }

    gotMessage(string msg) {
        # Fallback for non-async paths (not called when echo mode is active)
        send(msg);
    }

    gotMessage(binary msg) {
        send(msg);
    }
}

class WsBenchServer {
    private {
        const Opts = {
            "port":      "port=i",
            "duration":  "duration=i",
            "quiet":     "quiet",
            "port_file": "port-file=s",
            "help":      "help,h",
        };
    }

    constructor() {
        GetOpt g(Opts);
        hash<auto> opt = g.parse3(\ARGV);
        if (opt.help) {
            printf("Usage: %s [--port=N] [--duration=N] [--quiet] [--port-file=F]\n",
                get_script_name());
            exit(0);
        }

        int listen_port = opt.port ?? 0;

        Logger logger("ws-bench", LoggerLevel::getLevelInfo());
        # no appender — suppress all log output during benchmarks

        hash<HttpServerOptionInfo> http_opts = <HttpServerOptionInfo>{
            "logger": logger,
            "debug": False,
        };
        HttpServer server(http_opts);

        EchoWebSocketHandler ws_handler();
        server.setHandler("ws-echo", "/ws", MimeTypeHtml, ws_handler);
        server.setDefaultHandler("ws-echo", ws_handler);

        int bound_port = server.addListener(<HttpListenerOptionInfo>{
            "service": listen_port,
            "node": "127.0.0.1",
        }).port;
        server.waitForAsyncIo();

        # Write port for the benchmark driver
        if (opt.port_file) {
            File f();
            f.open2(opt.port_file, O_CREAT | O_WRONLY | O_TRUNC);
            f.printf("%d\n", bound_port);
        } else {
            printf("LISTEN_PORT=%d\n", bound_port);
            stdout.sync();
        }

        if (opt.duration) {
            if (!opt.quiet) {
                stderr.printf("WS echo server running for %d seconds on port %d...\n",
                    opt.duration, bound_port);
            }
            sleep(opt.duration);
        } else {
            if (!opt.quiet) {
                stderr.printf("WS echo server ready on port %d. Press Ctrl-C to stop.\n",
                    bound_port);
            }
            Counter c(1);
            c.waitForZero();
        }

        delete server;
    }
}
