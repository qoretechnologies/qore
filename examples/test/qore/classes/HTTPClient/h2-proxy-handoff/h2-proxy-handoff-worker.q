#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# makes HTTP/2 requests to an HTTPS server through a CONNECT proxy that delivers the server's TLS Finished message
# and its first HTTP/2 frames to the client together; prints one result per line
#
# usage: h2-proxy-handoff-worker.q <cert-and-key-file>

%modern
%requires HttpServer
%requires Logger
%requires Mime
%requires Util

# TLS 1.2: the server finishes its handshake first, so it can send application data (its HTTP/2 SETTINGS frame)
# right after its Finished message, and the proxy can deliver both in one read
qore_set_library_options(QLO_DISABLE_TLS_13);

#! answers whether the request was made with HTTP/2
class Http2Handler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        return makeResponse(200, sprintf("http2:%y", cx."header-info".http2 ?? False),
            {"Content-Type": MimeTypeText});
    }
}

#! a CONNECT proxy that holds the data from the target server from the end of the server's handshake (its
#! ChangeCipherSpec record) until the first application data record, and relays them together
class CoalescingProxy {
    private {
        Socket listener();
        int port;
        Counter threads();
        bool quit;
    }

    constructor() {
        listener.bindINET("127.0.0.1", "0", True);
        listener.listen();
        port = listener.getSocketInfo().port;
        threads.inc();
        background acceptLoop();
    }

    int getPort() {
        return port;
    }

    shutdown() {
        quit = True;
        listener.close();
        threads.waitForZero();
    }

    private acceptLoop() {
        on_exit threads.dec();
        while (!quit) {
            try {
                if (!listener.isDataAvailable(50ms)) {
                    continue;
                }
                Socket s = listener.accept();
                threads.inc();
                background serve(s);
            } catch (hash<ExceptionInfo> ex) {
                # the listener was closed
                break;
            }
        }
    }

    private serve(Socket s) {
        on_exit {
            s.close();
            threads.dec();
        }
        string head;
        while (head !~ /\r\n\r\n/) {
            *binary b = s.recvBinary(-1, 10s);
            if (!b) {
                return;
            }
            head += b.toString();
        }
        *list<*string> m = (head =~ x/^CONNECT ([^:]+):([0-9]+) /);
        if (!m) {
            s.send("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        Socket target();
        target.connectINET(m[0], m[1].toInt(), 10s);
        s.send("HTTP/1.1 200 Connection established\r\n\r\n");
        Counter c(1);
        background sub () {
            on_exit c.dec();
            pipe(s, target, False);
        }();
        pipe(target, s, True);
        c.waitForZero();
        target.close();
    }

    #! relays data; with @a coalesce, holds the data from the first ChangeCipherSpec record on until an application
    #! data record follows it
    private pipe(Socket from, Socket to, bool coalesce) {
        binary held;
        # bytes of the stream seen so far that are not yet split into TLS records
        binary pending;
        bool holding;
        try {
            while (!quit) {
                if (!from.isDataAvailable(50ms)) {
                    if (!from.isOpen()) {
                        break;
                    }
                    continue;
                }
                *binary b = from.recvBinary(-1, 10s);
                if (!b) {
                    break;
                }
                if (!coalesce) {
                    to.send(b);
                    continue;
                }
                # split the stream into TLS records: 1 byte type, 2 bytes version, 2 bytes length
                pending += b;
                bool flush;
                while (pending.size() >= 5) {
                    int type = pending[0];
                    int len = (pending[3] << 8) | pending[4];
                    if (pending.size() < 5 + len) {
                        break;
                    }
                    binary record = pending.substr(0, 5 + len);
                    pending = pending.substr(5 + len);
                    if (type == 0x14) {
                        # ChangeCipherSpec: the server's Finished follows; hold everything from here on
                        holding = True;
                    }
                    held += record;
                    if (holding && type == 0x17) {
                        # the first application data: deliver it with the end of the handshake
                        flush = True;
                        coalesce = False;
                    }
                }
                if (!holding || flush) {
                    to.send(held);
                    held = binary();
                }
                if (!coalesce && pending) {
                    to.send(pending);
                    pending = binary();
                }
            }
        } catch (hash<ExceptionInfo> ex) {
            # a closed connection ends the relay
        }
        try {
            to.shutdown();
        } catch (hash<ExceptionInfo> ex) {
        }
    }
}

string cert = ReadOnlyFile::readTextFile(ARGV[0]);
HttpServer server(<HttpServerOptionInfo>{
    "logger": new Logger("server", LoggerLevel::getLevelError()),
});
on_exit server.stop();
server.setDefaultHandler("h2", new Http2Handler());
int tls_port = server.addListener(<HttpListenerOptionInfo>{
    "node": "127.0.0.1",
    "service": 0,
    "cert": new SSLCertificate(cert),
    "key": new SSLPrivateKey(cert),
}).port;
server.waitForAsyncIo();

CoalescingProxy proxy();
on_exit proxy.shutdown();

# a new connection for each request, so that each one goes through the handoff
for (int i = 0; i < 3; ++i) {
    HTTPClient hc({
        "url": sprintf("https://localhost:%d", tls_port),
        "proxy": sprintf("http://127.0.0.1:%d", proxy.getPort()),
        "http_version": "2",
        "timeout": 20s,
        "connect_timeout": 20s,
    });
    try {
        printf("%d: %s\n", i, hc.get("/"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%d: %s: %s\n", i, ex.err, ex.desc);
    }
    hc.disconnect();
}
