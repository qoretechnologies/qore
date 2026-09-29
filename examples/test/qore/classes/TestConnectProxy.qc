# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

#! An HTTP proxy for tests: tunnels CONNECT requests and forwards plain HTTP requests
/** Records the request line and the Proxy-Authorization header of every request it receives, and can require
    credentials, answering requests without them with 407.  With a certificate, it is an HTTPS proxy: clients connect
    to it with TLS.

    Include it with %include; the including test must require the Util module.
*/
class TestConnectProxy {
    public {
        #! the requests received: request line, proxy authorization, and whether the client used TLS, in the order
        #! received
        list<hash<auto>> requests = ();
    }

    private {
        Socket listener();
        int port;
        #! the Proxy-Authorization value that is required, if any
        *string required_auth;
        #! the certificate and key of an HTTPS proxy
        *SSLCertificate cert;
        *SSLPrivateKey key;
        #! the addresses of target host names that cannot be resolved, such as a name in a test certificate
        *hash<string, string> host_map;
        bool quit;
        Counter threads();
        Mutex m();
    }

    #! Creates the proxy and starts listening on a free port of 127.0.0.1
    /** @param required_auth the Proxy-Authorization value that is required, if any
        @param cert the certificate of an HTTPS proxy; clients then connect to the proxy with TLS
        @param key the private key of \a cert
        @param host_map the addresses to connect to for target host names, such as a name in a test certificate
    */
    constructor(*string required_auth, *SSLCertificate cert, *SSLPrivateKey key, *hash<string, string> host_map) {
        self.required_auth = required_auth;
        self.cert = cert;
        self.key = key;
        self.host_map = host_map;
        listener.bindINET("127.0.0.1", "0", True);
        listener.listen();
        port = listener.getSocketInfo().port;
        threads.inc();
        background acceptLoop();
    }

    #! Returns the port of the proxy
    int getPort() {
        return port;
    }

    #! Returns a copy of the requests received
    list<hash<auto>> getRequests() {
        AutoLock al(m);
        return requests;
    }

    #! Stops the proxy and waits for its threads
    shutdown() {
        quit = True;
        threads.waitForZero();
        listener.close();
    }

    private acceptLoop() {
        on_exit threads.dec();
        while (!quit) {
            if (!listener.isDataAvailable(50ms)) {
                continue;
            }
            *Socket s = listener.accept();
            if (s) {
                threads.inc();
                background serve(s);
            }
        }
    }

    #! Reads the request head; returns the request line and the headers
    private *hash<auto> readHead(Socket s) {
        string head;
        while (head !~ /\r\n\r\n/) {
            if (quit) {
                return;
            }
            if (!s.isDataAvailable(50ms)) {
                continue;
            }
            *binary b = s.recvBinary(-1, 1s);
            if (!b) {
                return;
            }
            head += b.toString();
        }
        list<string> lines = (head =~ x/^(.*?)\r\n\r\n/s)[0].split("\r\n");
        hash<auto> hdr;
        foreach string line in (lines[1..]) {
            (*string k, *string v) = (line =~ x/^([^:]+):\s*(.*)$/);
            if (k) {
                hdr{k.lwr()} = v;
            }
        }
        return {"line": lines[0], "hdr": hdr, "rest": (head =~ x/^.*?\r\n\r\n(.*)$/s)[0] ?? ""};
    }

    private serve(Socket s) {
        on_exit {
            s.close();
            threads.dec();
        }
        if (cert) {
            try {
                s.setCertificate(cert);
                s.setPrivateKey(key);
                s.upgradeServerToSSL(10s);
            } catch (hash<ExceptionInfo> ex) {
                # the client failed the TLS handshake
                return;
            }
        }
        *hash<auto> req;
        try {
            req = readHead(s);
        } catch (hash<ExceptionInfo> ex) {
            # the client closed the connection
            return;
        }
        if (!req) {
            return;
        }
        {
            AutoLock al(m);
            requests += {
                "line": req.line,
                "proxy-authorization": req.hdr."proxy-authorization",
                "ssl": s.isSecure(),
            };
        }
        if (required_auth && req.hdr."proxy-authorization" != required_auth) {
            s.send("HTTP/1.1 407 Proxy Authentication Required\r\nProxy-Authenticate: Basic realm=\"test\"\r\n"
                "Content-Length: 0\r\n\r\n");
            return;
        }
        *list<*string> m = (req.line =~ x/^CONNECT ([^:]+):([0-9]+) /);
        if (m) {
            Socket target();
            target.connectINET(host_map{m[0]} ?? m[0], m[1].toInt(), 5s);
            s.send("HTTP/1.1 200 Connection established\r\n\r\n");
            relay(s, target);
            return;
        }
        # a plain HTTP request with an absolute URL
        m = (req.line =~ x/^([A-Z]+) http:\/\/([^\/:]+):([0-9]+)(\/[^ ]*) (.*)$/);
        if (!m) {
            s.send("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
            return;
        }
        Socket target();
        target.connectINET(host_map{m[1]} ?? m[1], m[2].toInt(), 5s);
        # the request is forwarded without the proxy's own header
        string fwd = sprintf("%s %s %s\r\n", m[0], m[3], m[4]);
        foreach hash<auto> i in (req.hdr.pairIterator()) {
            if (i.key != "proxy-authorization" && i.key != "proxy-connection") {
                fwd += sprintf("%s: %s\r\n", i.key, i.value);
            }
        }
        fwd += "\r\n" + req.rest;
        target.send(fwd);
        relay(s, target);
    }

    #! Relays data in both directions until either side closes
    private relay(Socket a, Socket b) {
        Counter c(1);
        background sub () {
            on_exit c.dec();
            pipe(b, a);
        }();
        pipe(a, b);
        c.waitForZero();
        b.close();
    }

    private pipe(Socket from, Socket to) {
        try {
            while (!quit) {
                if (!from.isDataAvailable(50ms)) {
                    if (!from.isOpen()) {
                        break;
                    }
                    continue;
                }
                *binary b = from.recvBinary(-1, 1s);
                if (!b) {
                    break;
                }
                to.send(b);
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
