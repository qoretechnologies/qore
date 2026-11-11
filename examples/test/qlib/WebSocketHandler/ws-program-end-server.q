#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The server of WebSocketProgramEnd.qtest: an HttpServer with a WebSocket echo handler whose main program returns
# while the connection of its client is open, as qdbg-server's does when the program that it debugs ends.  Reads a
# line from its standard input before its main program returns.  Prints its port, "main returned" when the main
# program has returned, "connection closed" when a connection's connectionClosed() runs and "server destroyed" when
# the HttpServer is destroyed.
#
# usage: ws-program-end-server.q [release]
#   release: the main program releases the server before it returns, while the connection is open

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../qlib"
%requires HttpServer
%requires WebSocketHandler
%requires Mime

%exec-class WsProgramEndServer

class EchoConnection inherits WebSocketConnection {
    constructor(WebSocketHandler h) : WebSocketConnection(h) {
    }

    gotMessage(string msg) {
        send(msg);
    }

    connectionClosed() {
        WsProgramEndServer::out("connection closed");
    }
}

class EchoHandler inherits WebSocketHandler {
    WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new EchoConnection(self);
    }
}

class EchoServer inherits HttpServer {
    constructor(hash<HttpServerOptionInfo> opts) : HttpServer(opts) {
    }

    destructor() {
        WsProgramEndServer::out("server destroyed");
    }
}

class WsProgramEndServer {
    private {
        *EchoServer server;
    }

    constructor() {
        server = new EchoServer(<HttpServerOptionInfo>{});
        EchoHandler handler();
        server.setHandler("ws", "", MimeTypeHtml, handler);
        server.setDefaultHandler("ws", handler);
        int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
        out(sprintf("port %d", port));
        # a line from the client tells the main program to return; its connection is open then
        stdin.readLine();
        if (ARGV[0] == "release") {
            # the program releases the server while the connection is open
            server = NOTHING;
        }
        out("main returned");
    }

    #! Writes a line of output at once
    static out(string line) {
        stdout.printf("%s\n", line);
        stdout.sync();
    }
}
