#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The server of WebSocketProgramEnd.qtest: an HttpServer with a WebSocket echo handler whose main program returns
# while the connection of its client is open, as qdbg-server's does when the program that it debugs ends.  Reads a
# line from its standard input before its main program returns.  Prints its port, "main returned" when the main
# program has returned, "connection closed" when a connection's connectionClosed() runs and "server destroyed" when
# the HttpServer is destroyed.
#
# usage: ws-program-end-server.q [release|release-in-request]
#   release: the main program releases the server before it returns, while the connection is open
#   release-in-request: as release, while the handler thread of the request that opened the connection still runs it
#     (the handler's start holds the request until the main program has released the server), so that the request
#     releases the last reference to the server

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
    private {
        #! if set, the request that opens a connection waits for it to reach zero once the connection is set up
        *Counter hold;
    }

    constructor(*Counter hold) {
        self.hold = hold;
    }

    WebSocketConnection getConnectionImpl(hash<auto> cx, hash<auto> hdr, string cid) {
        return new EchoConnection(self);
    }

    bool startImpl(softstring lid, hash<auto> cx, hash<auto> hdr, Qore::Socket sock) {
        bool rv = WebSocketHandler::startImpl(lid, cx, hdr, sock);
        # the connection is open and served; the request that opened it is still running in the server
        if (hold) {
            hold.waitForZero();
        }
        return rv;
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
        *Counter hold = ARGV[0] == "release-in-request" ? new Counter(1) : NOTHING;
        EchoHandler handler(hold);
        server.setHandler("ws", "", MimeTypeHtml, handler);
        server.setDefaultHandler("ws", handler);
        int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
        out(sprintf("port %d", port));
        # a line from the client tells the main program to return; its connection is open then
        stdin.readLine();
        if (ARGV[0] == "release") {
            # the program releases the server while the connection is open
            server = NOTHING;
        } else if (hold) {
            # the program releases the server while the request that opened the connection still runs in it, so the
            # request holds the last reference to the server; the request ends once this has been written
            server = NOTHING;
            out("main returned");
            hold.dec();
            return;
        }
        out("main returned");
    }

    #! Writes a line of output at once
    static out(string line) {
        stdout.printf("%s\n", line);
        stdout.sync();
    }
}
