#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# opens a number of keep-alive connections to an HttpServer, makes one more request on each of them and prints the
# scans that the requests made as "root=<name> scans=<n> nodes=<n>" lines; run by HttpServerScanSize.qtest with
# QORE_SCAN_STATS set

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires Util
%requires Logger
%requires HttpServerUtil
%requires HttpServerAsyncIo
%requires HttpServer

#! one request's object, in a cycle with the handler that made it
/** An application object that refers to the handler serving it, and that the handler refers to, is in the
    handler's recursive set: a scan of it walks everything in a cycle with the handler.
*/
class ScanSizeRequest {
    public {
        ScanSizeHandler handler;
        auto value;
    }

    constructor(ScanSizeHandler handler) {
        self.handler = handler;
    }
}

#! a handler that refers to its server, which puts the server and the handler in one recursive set
class ScanSizeHandler inherits AbstractHttpRequestHandler {
    public {
        *HttpServer server;
        *ScanSizeRequest last;
    }

    hash<auto> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        ScanSizeRequest req(self);
        last = req;
        # a write to the request object is scanned when the local releases it
        req.value = new ScanSizeRequest(self);
        return {"code": 200, "body": "ok"};
    }
}

const Timeout = 10s;

#! sends a request on the connection and reads the response
sub exchange(Socket s) {
    s.send("GET /scan HTTP/1.1\r\nHost: localhost\r\n\r\n");
    hash<auto> hdr = s.readHTTPHeader(Timeout.durationMilliseconds());
    if (hdr.status_code != 200) {
        throw "SCAN-SIZE-ERROR", sprintf("unexpected response: %y", hdr);
    }
    s.recv(hdr."content-length".toInt(), Timeout.durationMilliseconds());
}

#! returns the scan accounting by root
hash<string, hash<ScanStatsInfo>> sub get_stats() {
    return map {$1.root: $1}, get_scan_stats();
}

int connections = ARGV[0].toInt();
if (connections < 1) {
    stderr.printf("usage: %s <connections>\n", get_script_name());
    exit(1);
}

Logger logger("scan-size", LoggerLevel::getLevelInfo());
HttpServer server(<HttpServerOptionInfo>{"logger": logger});
ScanSizeHandler handler();
handler.server = server;
server.setHandler("scan", "/scan", NOTHING, handler);
int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
if (!server.waitForAsyncIo(Timeout)) {
    throw "SCAN-SIZE-ERROR", "async I/O not ready";
}

list<Socket> socks = ();
{
    on_exit {
        map $1.close(), socks;
        server.stop();
        # breaks the cycle between the server and the handler
        remove handler.server;
        remove handler.last;
    }

    for (int i = 0; i < connections; ++i) {
        Socket s();
        s.connect("127.0.0.1:" + port, Timeout);
        exchange(s);
        socks += s;
    }

    hash<string, hash<ScanStatsInfo>> before = get_stats();
    # every request is made while all connections are open
    map exchange($1), socks;
    hash<string, hash<ScanStatsInfo>> after = get_stats();

    printf("requests=%d\n", connections);
    foreach hash<ScanStatsInfo> i in (after.iterator()) {
        int scans = i.scans - (before{i.root}.scans ?? 0);
        if (scans) {
            printf("root=%s scans=%d nodes=%d\n", i.root, scans, i.nodes - (before{i.root}.nodes ?? 0));
        }
    }
}
