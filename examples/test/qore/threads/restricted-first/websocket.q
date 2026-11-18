#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# WebSocketClient loaded first by a Program that may not control threads (qorus#867): its reconnect thread starts,
# governed by the caller's sandbox (its thread limit); this Program does not load the module itself

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

# the server; the restricted Program loads WebSocketClient itself
%requires HttpServer
%requires WebSocketHandler

class EchoConnection inherits WebSocketConnection {
    constructor(WebSocketHandler handler) : WebSocketConnection(handler) {
    }

    gotMessage(string msg) {
        send(msg);
    }
}

class EchoHandler inherits WebSocketHandler {
    constructor() : WebSocketHandler(NOTHING, {"logger": new Logger("ws", LoggerLevel::getLevelError())}) {
    }

    WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new EchoConnection(self);
    }
}

const Source = "
# connects n clients, has the host stop its server, and reconnects each: each reconnect starts a thread, which keeps
# retrying until the client is disconnected; returns the result of each reconnect
list<string> sub run(int port, int n, code stop_server) {
    list<object> clients = ();
    list<string> rv = ();
    on_exit map $1.disconnect(), clients;
    for (int i = 0; i < n; ++i) {
        WebSocketClient::WebSocketClient c(sub (*data msg) {}, {\"url\": \"ws://localhost:\" + port,
            \"reconnect\": True, \"connect_timeout\": 1s});
        clients += c;
        c.connect();
    }
    call_function(stop_server);
    foreach object c in (clients) {
        try {
            rv += c.reconnect() ? \"started\" : \"not-started\";
        } catch (hash<ExceptionInfo> ex) {
            rv += ex.err;
        }
    }
    return rv;
}
";

# a restricted Program that loads the module first, with a sandbox allowing max_threads threads
Program sub restricted(int max_threads) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_NO_THREAD_CONTROL);
    p.loadModule("WebSocketClient");
    p.parse(Source, "restricted");
    SandboxManager sm();
    sm.setMaxThreads(max_threads);
    # the client connects to the local server
    sm.addNetworkAllowedIPRange("127.0.0.0/8");
    sm.addNetworkAllowedIPRange("::1/128");
    p.setSandboxManager(sm);
    return p;
}

# runs the clients of a restricted Program against a server of their own, which is stopped before they reconnect
list<string> sub run(Program p) {
    HttpServer server(<HttpServerOptionInfo>{"logger": new Logger("server", LoggerLevel::getLevelError())});
    on_exit server.stop();
    EchoHandler handler();
    server.setHandler("ws", "", NOTHING, handler);
    server.setDefaultHandler("ws", handler);
    int port = server.addListener(<HttpListenerOptionInfo>{"node": "localhost", "service": 0}).port;
    return p.callFunction("run", port, 2, sub () { server.stop(); });
}

# the Programs are kept until the end, so that the threads of their objects end first
Program limited = restricted(1);
Program free = restricted(5);
printf("limited: %s\n", (foldl $1 + "," + $2, run(limited)));
printf("started: %s\n", (foldl $1 + "," + $2, run(free)));
