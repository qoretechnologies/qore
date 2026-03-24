#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
#
# Benchmark server for measuring async HTTP I/O throughput.
#
# Usage:
#   QORE_MODULE_DIR=qlib qore bench-async-http.q [options]
#
# Options:
#   --port=N         Listen port (default 0 = auto)
#   --duration=N     Run for N seconds then exit (default: run until killed)
#   --sync           Use sync mode (no async I/O) for baseline comparison
#   --quiet          Suppress info messages
#
# The server binds to 127.0.0.1 and prints the port on stdout so the
# benchmark driver can connect.

%modern

%requires ../../../../qlib/Util.qm
%requires ../../../../qlib/Mime.qm
%requires ../../../../qlib/Logger.qm
%requires ../../../../qlib/HttpServerUtil.qm
%requires ../../../../qlib/HttpServerAsyncIo
%requires ../../../../qlib/HttpServer.qm

%exec-class BenchServer

class BenchHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        return makeResponse(200, "OK");
    }
}

class BenchServer {
    private {
        const Opts = {
            "port":      "port=i",
            "duration":  "duration=i",
            "sync":      "sync",
            "quiet":     "quiet",
            "port_file": "port-file=s",
            "help":      "help,h",
        };
    }

    constructor() {
        GetOpt g(Opts);
        hash<auto> opt = g.parse3(\ARGV);
        if (opt.help) {
            printf("Usage: %s [--port=N] [--duration=N] [--sync] [--quiet]\n",
                get_script_name());
            exit(0);
        }

        int listen_port = opt.port ?? 0;

        Logger logger("bench", LoggerLevel::getLevelInfo());
        # no appender — suppress all log output during benchmarks

        hash<HttpServerOptionInfo> http_opts = <HttpServerOptionInfo>{
            "logger": logger,
            "debug": False,
        };
        HttpServer server(http_opts);
        server.setDefaultHandler("bench", new BenchHandler());
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

        string mode = async ? "async" : "sync";
        if (opt.duration) {
            if (!opt.quiet) {
                stderr.printf("Running for %d seconds in %s mode...\n", opt.duration, mode);
            }
            sleep(opt.duration);
        } else {
            if (!opt.quiet) {
                stderr.printf("Server ready on port %d in %s mode. "
                    "Press Ctrl-C to stop.\n", bound_port, mode);
            }
            # Block until killed
            Counter c(1);
            c.waitForZero();
        }

        delete server;
    }
}
