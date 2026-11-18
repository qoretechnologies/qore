#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# ServerSentEventClient loaded first by a Program that may not control threads (qorus#867): its event thread starts,
# governed by the caller's sandbox (its thread limit); this Program does not load the module itself

# Copyright 2026 Qore Technologies, s.r.o.

%new-style
%require-types
%strict-args

# the server; the restricted Program loads ServerSentEventClient itself
%requires HttpServer
%requires ServerSentEventHandler

#! sends one event to each connection
class OneEventHandler inherits ServerSentEventHandler {
    constructor() : ServerSentEventHandler({"logger": new Logger("sse", LoggerLevel::getLevelError())}) {
    }

    connectionRegisteredImpl(ServerSentEventHandler::ServerSentEventConnection conn) {
        conn.send(<SseMessageInfo>{"id": 1, "data": {"a": 1}});
    }
}

const Source = "
# receives the first data event
class FirstData inherits DataProvider::Observer {
    public {
        Counter first(1);
    }
    update(string event_id, hash<auto> data_) {
        if (event_id == EVENT_SSE_DATA && first.getCount()) {
            first.dec();
        }
    }
}
# connects n clients, each with its event thread; returns the result of each connection
list<string> sub run(int port, int n) {
    list<object> clients = ();
    list<string> rv = ();
    on_exit map $1.disconnect(), clients;
    for (int i = 0; i < n; ++i) {
        ServerSentEventClient::ServerSentEventClientIo c({\"url\": \"sse://localhost:\" + port, \"data\": \"json\"});
        FirstData obs();
        c.registerObserver(obs);
        try {
            c.connect();
            clients += c;
            rv += obs.first.waitForZero(30s) ? \"no-data\" : \"started\";
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
    p.loadModule("ServerSentEventClient");
    p.parse(Source, "restricted");
    SandboxManager sm();
    sm.setMaxThreads(max_threads);
    # the client connects to the local server
    sm.addNetworkAllowedIPRange("127.0.0.0/8");
    sm.addNetworkAllowedIPRange("::1/128");
    p.setSandboxManager(sm);
    return p;
}

HttpServer server(<HttpServerOptionInfo>{"logger": new Logger("server", LoggerLevel::getLevelError())});
on_exit server.stop();
OneEventHandler handler();
server.setHandler("sse", "", NOTHING, handler);
server.setDefaultHandler("sse", handler);
int port = server.addListener(<HttpListenerOptionInfo>{"node": "localhost", "service": 0}).port;
# the Programs are kept until the end, so that the threads of their objects end first
Program limited = restricted(1);
Program free = restricted(5);
printf("limited: %s\n", (foldl $1 + "," + $2, limited.callFunction("run", port, 2)));
printf("started: %s\n", (foldl $1 + "," + $2, free.callFunction("run", port, 2)));
