#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

/*
    Copyright (C) 2026 Qore Technologies, s.r.o., all rights reserved

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

    Minimal HTTP/2 async server example that triggers the async 500 fallback
    when a handler returns without sending a response.
*/

%modern

%requires HttpServerAsyncIo
%requires HttpServer
%requires Logger
%requires Util
%requires WebUtil

%exec-class Http2Async500Server

class NoResponseHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        return <HttpResponseInfo>{
            "code": 200,
            "reply_sent": True,
        };
    }
}

class SlowUpstreamHandler inherits AbstractHttpRequestHandler {
    private {
        int delay_ms;
    }

    constructor(int delay_ms) {
        self.delay_ms = delay_ms;
    }

    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        sleep(delay_ms);
        return makeResponse(200, "upstream-ok", {"Content-Type": "text/plain"});
    }
}

class UpstreamTimeoutHandler inherits AbstractHttpRequestHandler {
    private {
        string upstream_url;
        int upstream_timeout_ms;
    }

    constructor(string upstream_url, int timeout_ms) {
        self.upstream_url = upstream_url;
        self.upstream_timeout_ms = timeout_ms;
    }

    hash<HttpResponseInfo> handleRequest(hash<auto> cx, hash<auto> hdr, *data body) {
        try {
            HTTPClient hc({
                "url": upstream_url,
                "http_version": "2.0",
                "timeout": upstream_timeout_ms,
            });
            hc.acceptAllCertificates(True);
            hc.send("", "GET", "/slow", NOTHING, True);
        } catch (hash<ExceptionInfo> ex) {
            printf("upstream error: %s: %s\n", ex.err, ex.desc);
            return <HttpResponseInfo>{
                "code": 200,
                "reply_sent": True,
            };
        }

        return makeResponse(200, "upstream-ok", {"Content-Type": "text/plain"});
    }
}

class Http2Async500Server {
    private {
        HttpServer server;
        HttpServer upstream;
        int port;
        int upstream_port;
        string file_root;
        string upstream_url;
        string base_url;
        int parallel;
        string client_path;
        bool run_client;
        int upstream_timeout_ms;
        int upstream_delay_ms;

        const CertPem = "-----BEGIN CERTIFICATE-----
MIIFNjCCBB6gAwIBAgISA1ELHYM0GFfi2BdvZfzLGJMLMA0GCSqGSIb3DQEBCwUA
MDIxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQswCQYDVQQD
EwJSMzAeFw0yMjA4MDcwOTAwMDdaFw0yMjExMDUwOTAwMDZaMCIxIDAeBgNVBAMT
F2hxLnFvcmV0ZWNobm9sb2dpZXMuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8A
MIIBCgKCAQEAlw84m+9dCjeD4s8YYVZ3gbv8YkYQGyyaojgdJuarTyJPYnGzhMf7
PF+Y2398k+8ydAnOTwJXRcaU36f0hHDvTagP1f0EFg3kWlplzsrtrDI/HZBRH0W2
+54YE6FhPEtEkkO1aKy+VXb8QMQKSmsUZS+IxSe+69tWFI60tW92eq53tFAwEWN+
0oBeJBCASRQa7bq1J/BgZhlBSyUQ6Zf+wqUSfmVEf7tsFujyNZ2dfxHtPtLxRspA
7yhgnQQaXHemyEWeZWjIbToazG8SvutmcwIFK/TtE0anWdTR+Hn92mxH9mxnnyns
Ag+RlqxnEpBDIb1ufwCFUhMXBwRiiZIM1QIDAQABo4ICVDCCAlAwDgYDVR0PAQH/
BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAMBgNVHRMBAf8E
AjAAMB0GA1UdDgQWBBSj2nUL9X+zXI7p0fFhg03WZAS0xDAfBgNVHSMEGDAWgBQU
LrMXt1hWy65QCUDmH6+dixTCxjBVBggrBgEFBQcBAQRJMEcwIQYIKwYBBQUHMAGG
FWh0dHA6Ly9yMy5vLmxlbmNyLm9yZzAiBggrBgEFBQcwAoYWaHR0cDovL3IzLmku
bGVuY3Iub3JnLzAiBgNVHREEGzAZghdocS5xb3JldGVjaG5vbG9naWVzLmNvbTBM
BgNVHSAERTBDMAgGBmeBDAECATA3BgsrBgEEAYLfEwEBATAoMCYGCCsGAQUFBwIB
FhpodHRwOi8vY3BzLmxldHNlbmNyeXB0Lm9yZzCCAQYGCisGAQQB1nkCBAIEgfcE
gfQA8gB3AN+lXqtogk8fbK3uuF9OPlrqzaISpGpejjsSwCBEXCpzAAABgne/YgYA
AAQDAEgwRgIhAP/ERcskhKhF7M8VIejtrwEtDXJoX1IXec//r64jkOlUAiEAhheZ
VMT5cZ2uoPGoD6+SuQY/CYBuHdXNR/pUC3SGOQEAdwApeb7wnjk5IfBWc59jpXfl
vld9nGAK+PlNXSZcJV3HhAAAAYJ3v2H5AAAEAwBIMEYCIQCmiChg/6dLhE3TfGum
JR7k8s7ibmqw2KVJI3oUR/ogBAIhAIXIIhyqb3W/34ATNT9dIPCGFKpghQw82G8w
ANnChDgYMA0GCSqGSIb3DQEBCwUAA4IBAQAovvC8AiF7+uNLJCEXMe3VeI4Ne+l1
mqCRRujz7ijlr6VgNxZt+i/kx1HJKrSKnQZRQ48xWAipMVYfXxH3u20p4RkkW2nj
jjIVQvQvlFhDjLaJR74PYopp0lPuBW9RKg+C+l3vvjxjkin/MOBX2apGOvC4LJwb
2s6f8cArBRvdhA7nwEmlP3+aqxxkp9STZYpxuKR4F9fRGtg0Y39Db+3XkYp7Y/hV
DBVpHegoty2VFErehkJUmgdNoLdTuC8gHgA3p5bCbApyVGjBuO+QpSWG/3WiBDfm
1IpGT8P2OhFruQHryHEKmY6f5huZV2Y0gFmqBwRiR1ToF5gFveTUJM3d
-----END CERTIFICATE-----";

        const KeyPem = "-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCXDzib710KN4Pi
zxhhVneBu/xiRhAbLJqiOB0m5qtPIk9icbOEx/s8X5jbf3yT7zJ0Cc5PAldFxpTf
p/SEcO9NqA/V/QQWDeRaWmXOyu2sMj8dkFEfRbb7nhgToWE8S0SSQ7VorL5VdvxA
xApKaxRlL4jFJ77r21YUjrS1b3Z6rne0UDARY37SgF4kEIBJFBrturUn8GBmGUFL
JRDpl/7CpRJ+ZUR/u2wW6PI1nZ1/Ee0+0vFGykDvKGCdBBpcd6bIRZ5laMhtOhrM
bxK+62ZzAgUr9O0TRqdZ1NH4ef3abEf2bGefKewCD5GWrGcSkEMhvW5/AIVSExcH
BGKJkgzVAgMBAAECggEAPhvyCJtYQ9UjkuPXgF4O8PacBMQN5z5lrgEoa1A4a2cO
AMoDJ7sZ327m6Ij4bdLRichmXTH3NCc8GuFxterBWcqaCD/pqC+6DjRQ27+wDTbz
oHIwCI2feME94QRfeGzyGrlgI1OzRmyPtwljuclhL2Fl+Loo08zxDa7HOjpEGphz
wkD2gQy5S7Mi5ekBZMhy8EklpzdKlOFdvhjo7vQu8vY8xJZ6jN65RV+RBscbmZQl
llK1uW/8UyRIAUK9sM6Ozxbiz7QQELHpSvY3JIiZC5A6rBk2DGqOu8ouzDrQuHsB
LHWjzceu6UcJY0tCeOUkVDnoxbj2UcINEAIABIPN2QKBgQDHHbrwdW8qRecLw8y2
FJDO/6+8VmRKtj8B7CeOYuEZet2UvnPssisCovu0jnMa2ysF/L4YQAWa5XY0bB+E
R/NRzeh4i0jIsW70h2qePTK+XRAGhCCKiCcb7nkED72TTplQ5wgRA/oWPLOrys34
f+YqCI7BVrbWzgDyrhEdRAXSbwKBgQDCNuMlLcGTTi04OIQFDZmzjdvx9KvHvksz
zff9Y0Rb1eUsXF2oG3S55umgeUcG9Nk2KigJzO6WAmTT6rcbn4tsRsnkLdqcEPmg
P1wbBikW3Bz935k7yPcx05bVshLrJsK0Gxa8YYE6hh/PbdiQFtHf2r4uYu1kxC01
518U8oPm+wKBgDfyJIpXlKp+BZMKqsQmNyHSOaBjbb6IQl/Z6KtbIQA1w3h9orjI
vsj43lw3AiRznD0MbKUHqAuDmZjVIG3cgYNkpYLpL8QkBpbyTYS0kUNnho8uJK6H
3uU8NghsG8n99ZoDsAKH6YbB+4Gzc/f0h8kbqnCsWqc0LpQBUJG2gSRFAoGBAI3P
4kBNjuFu3hoFOnEuIyM23HlqPNyXGPZ02TXOfCXKo5Kmx0Ru9+aes80XgUOVGd4x
Hhc56qTijpkm9BlZgEbJ0bWpvczjoELgwPKCpxIoG4tM7+j1r3pUk/jqFGJcZSN5
/DoFwITpVuTxwoZEA2+/m8rnNYy0qoaHsafsBWBtAoGAAPYkINDjhAslu2gQlsZi
7eA4coUg5eNUI+YYy4xonJAU488AsrcAWr5nGEids4ZM5PqKkJezG+6gctaYnjjZ
IQrghzR1AC0Y3XDSuJYGRcZeUF2XZmOjcXNm0oMxeMEeEuuNLOTZ2pp7aUcYIOfV
OKKqgBjNAlYDdPhOy8O8Agk=
-----END PRIVATE KEY-----";
    }

    constructor() {
        GetOpt g((
            "dir": "d,dir=s",
            "client": "c,client",
            "parallel": "p,parallel=i",
            "path": "path=s",
            "upstream-timeout": "upstream-timeout=i",
            "upstream-delay": "upstream-delay=i",
            "help": "h,help",
        ));
        hash opt = g.parse3(\ARGV);

        if (opt.help) {
            usage();
        }

        file_root = opt.dir ?? getcwd();
        if (!is_readable(file_root)) {
            stderr.printf("ERROR: %y: is not readable\n", file_root);
            exit(2);
        }

        parallel = opt.parallel ?? 9;
        client_path = opt.path ?? "/500";
        run_client = opt.client ?? False;
        upstream_timeout_ms = opt."upstream-timeout" ?? 1000;
        upstream_delay_ms = opt."upstream-delay" ?? 5000;

        Logger logger("http2-async-500", LoggerLevel::getLevelInfo());
        logger.addAppender(new StdoutAppender());

        hash<HttpServerOptionInfo> opts = <HttpServerOptionInfo>{
            "logger": logger,
            "debug": True,
            "async_mode": True,
        };
        server = new HttpServer(opts);
        server.setHandler("no-response", "/500", NOTHING, new NoResponseHandler());
        server.setDefaultHandler("no-response", new NoResponseHandler());
        WebUtil::FileHandler fh(file_root);
        server.setHandler("files", "/files", NOTHING, fh);

        upstream = new HttpServer(opts);
        upstream.setHandler("slow", "/slow", NOTHING, new SlowUpstreamHandler(upstream_delay_ms));

        upstream_port = upstream.addListener(<HttpListenerOptionInfo>{
            "service": 0,
            "cert": new SSLCertificate(CertPem),
            "key": new SSLPrivateKey(KeyPem),
        }).port;
        upstream_url = "https://localhost:" + upstream_port;

        server.setHandler("timeout", "/timeout", NOTHING,
            new UpstreamTimeoutHandler(upstream_url, upstream_timeout_ms));

        port = server.addListener(<HttpListenerOptionInfo>{
            "service": 0,
            "cert": new SSLCertificate(CertPem),
            "key": new SSLPrivateKey(KeyPem),
        }).port;
        base_url = "https://localhost:" + port;

        upstream.waitForAsyncIo();
        server.waitForAsyncIo();
        printf("HTTP/2 async server: %s\n", base_url);
        printf("Example 500: curl -k --http2 %s/500\n", base_url);
        printf("Example file: curl -k --http2 %s/files/README.md\n", base_url);
        printf("Example timeout: curl -k --http2 %s/timeout\n", base_url);
        printf("Upstream slow: %s/slow (delay %d ms)\n", upstream_url, upstream_delay_ms);

        if (run_client) {
            runClient();
            shutdown();
            return;
        }

        installShutdownHandlers();
        server.waitStop();
    }

    private installShutdownHandlers() {
        set_signal_handler(SIGINT, \shutdownSignalHandler());
        set_signal_handler(SIGTERM, \shutdownSignalHandler());
    }

    private shutdownSignalHandler(int sig) {
        remove_signal_handler(SIGINT);
        remove_signal_handler(SIGTERM);
        shutdown();
    }

    synchronized private shutdown() {
        server.stopNoWait();
        upstream.stopNoWait();
    }

    private runClient() {
        printf("Running client: %d parallel requests to %s\n", parallel, client_path);
        HTTPClient h2({
            "url": base_url,
            "http_version": "2.0",
            "error_passthru": True,
            "timeout": 30s,
        });
        h2.acceptAllCertificates(True);
        try {
            h2.send("", "GET", client_path, NOTHING, True);
        } catch (hash<ExceptionInfo> ex) {
            printf("client warmup error: %s: %s\n", ex.err, ex.desc);
        }

        Counter cnt(parallel);
        Mutex m();
        int ok = 0;
        int err = 0;

        for (int i = 0; i < parallel; ++i) {
            background sub() {
                on_exit cnt.dec();
                try {
                    h2.send("", "GET", client_path, NOTHING, True);
                    m.lock();
                    on_exit m.unlock();
                    ok += 1;
                } catch (hash<ExceptionInfo> ex) {
                    m.lock();
                    on_exit m.unlock();
                    err += 1;
                    printf("client error: %s: %s\n", ex.err, ex.desc);
                }
            }();
        }

        cnt.waitForZero();
        printf("Client done: ok=%d err=%d\n", ok, err);
    }

    static usage() {
        stderr.printf("usage: %s [options]\n"
                      "options:\n"
                      " -c,--client       run internal HTTP/2 client and exit\n"
                      " -d,--dir=ARG      file root for /files (default: current directory)\n"
                      " -p,--parallel=ARG number of parallel client requests (default: 9)\n"
                      " --path=ARG        client path (default: /500)\n"
                      " --upstream-delay=ARG   upstream /slow delay in ms (default: 5000)\n"
                      " --upstream-timeout=ARG upstream client timeout in ms (default: 1000)\n"
                      " -h,--help         this help text\n",
                      get_script_name());
        exit(1);
    }
}
