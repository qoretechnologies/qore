#!/usr/bin/env qore
# sandboxed code loads the HttpServer module first, at run time: trusted code under a sandbox that allows
# everything (which a host uses only to interrupt or limit its code), or restricted code; the initialization of the
# module claims the dispatch capability of the HTTP server in either case, so that the server's stream requests are
# served (qore#6843, qore#7577): the claim refuses restricted callers only, and the initialization of the module's
# static members does not run under the loader's sandbox; run by ../HttpServerDispatchCapability.qtest with
# "permissive" or "restricted" as its argument, with a module path that has the HttpServer module

%modern
%no-child-restrictions

SandboxManager sm();
if (ARGV[0] == "permissive") {
    sm.setFilesystemDefaultPolicy(True);
    sm.setNetworkDefaultPolicy(True);
    sm.setUncontrolledApiPolicy(True);
} else {
    # the module files, and the listener and the client of the server
    foreach string dir in (ENV.QORE_MODULE_DIR.split(":")) {
        sm.addFilesystemAllowedPath(dir, QSEC_READ);
    }
    sm.setNetworkDefaultPolicy(True);
}
Program p(PO_NO_CHILD_PO_RESTRICTIONS);
p.setSandboxManager(sm);
p.parse("%modern\nsub load() { load_module('HttpServer'); load_module('Logger'); }", "load");
p.callFunction("load");
printf("HttpServer loaded: %y\n", exists get_module_hash().HttpServer);
# the server, in a Program of the same sandbox, with the module loaded
Program s(PO_NO_CHILD_PO_RESTRICTIONS);
s.setSandboxManager(sm);
s.parse("%modern
%requires HttpServer
%requires Logger

class PlainHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        return <HttpResponseInfo>{'code': 200, 'body': 'plain'};
    }
}

class StreamRequest inherits AbstractStreamRequest {
    constructor(HttpListenerInterface listener, AbstractHttpRequestHandler handler, Socket s, hash<auto> cx,
            hash<auto> hdr, auto body) : AbstractStreamRequest(listener, handler, s, cx, hdr, body) {
    }

    private nothing recvImpl(hash<auto> v) {
    }

    private hash<HttpResponseInfo> getResponseHeaderMessageImpl() {
        return <HttpResponseInfo>{'code': 200, 'body': 'stream'};
    }
}

class StreamHandler inherits AbstractHttpRequestHandler {
    constructor() : AbstractHttpRequestHandler(NOTHING, True) {
    }

    private AbstractStreamRequest getStreamRequestImpl(HttpListenerInterface listener, Socket s, hash<auto> cx,
            hash<auto> hdr, *data body) {
        return new StreamRequest(listener, self, s, cx, hdr, body);
    }
}

int sub status(int port, string path) {
    Socket s();
    s.connect('127.0.0.1:' + port);
    on_exit s.close();
    s.send(\"POST /\" + path + \" HTTP/1.1\\r\\nHost: x\\r\\nContent-Length: 5\\r\\nConnection: close\\r\\n\\r\\nhello\");
    return s.readHTTPHeader(10s).status_code;
}

string sub run() {
    HttpServer server(<HttpServerOptionInfo>{'logger': new Logger('server', LoggerLevel::getLevelFatal())});
    on_exit server.stop();
    server.setHandler('plain', '^plain', NOTHING, new PlainHandler());
    server.setHandler('stream', '^stream', NOTHING, new StreamHandler());
    int port = server.addListener(<HttpListenerOptionInfo>{'node': '127.0.0.1', 'service': 0}).port;
    return sprintf('plain: %d, stream: %d', status(port, 'plain'), status(port, 'stream'));
}
", "server");
printf("%s: %s\n", ARGV[0], s.callFunction("run"));
