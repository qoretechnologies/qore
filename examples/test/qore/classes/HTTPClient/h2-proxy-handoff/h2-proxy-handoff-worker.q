#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# makes HTTP/2 requests to an HTTPS server through a CONNECT proxy that delivers the server's TLS Finished message
# and its first HTTP/2 frames to the client together; prints one result per line
#
# usage: h2-proxy-handoff-worker.q <cert-and-key-file>

%modern

# the worker runs in its own process: it must load the modules of this source tree, not installed ones
%prepend-module-path "${SCRIPT_DIR}/../../../../../../qlib"
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
        # the open connections, closed by shutdown()
        hash<string, Socket> conns;
        int conn_id;
        Mutex m();
        bool down;
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

    #! closes the listener and all connections, which wakes the blocked accept and receive calls, and waits for
    #! all threads to end
    shutdown() {
        {
            m.lock();
            on_exit m.unlock();
            down = True;
            listener.close();
            map $1.close(), conns.iterator();
        }
        threads.waitForZero();
    }

    private acceptLoop() {
        on_exit threads.dec();
        while (True) {
            *Socket s;
            try {
                # blocks until a connection arrives or the listener is closed
                s = listener.accept();
            } catch (hash<ExceptionInfo> ex) {
                # the listener was closed
                break;
            }
            if (!s) {
                # the listener was closed
                break;
            }
            threads.inc();
            background serve(s);
        }
    }

    #! registers a connection to be closed by shutdown(); returns its key, or NOTHING if the proxy is shut down
    private *string register(Socket s) {
        m.lock();
        on_exit m.unlock();
        if (down) {
            return;
        }
        string key = (++conn_id).toString();
        conns{key} = s;
        return key;
    }

    private unregister(string key) {
        m.lock();
        on_exit m.unlock();
        remove conns{key};
    }

    private serve(Socket s) {
        on_exit {
            s.close();
            threads.dec();
        }
        *string s_key = register(s);
        if (!s_key) {
            return;
        }
        on_exit unregister(s_key);
        string head;
        while (head !~ /\r\n\r\n/) {
            *binary b = s.recvBinary(-1, 10s);
            if (!b) {
                return;
            }
            head += b.toString();
        }
        *list<*string> cm = (head =~ x/^CONNECT ([^:]+):([0-9]+) /);
        if (!cm) {
            s.send("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        Socket target();
        target.connectINET(cm[0], cm[1].toInt(), 10s);
        *string target_key = register(target);
        if (!target_key) {
            target.close();
            return;
        }
        on_exit unregister(target_key);
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
            while (True) {
                # blocks until data arrives or the connection is closed (also by shutdown())
                *binary b = from.recvBinary(-1, 20s);
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
