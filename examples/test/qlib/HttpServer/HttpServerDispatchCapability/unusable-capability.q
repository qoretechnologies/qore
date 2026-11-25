#!/usr/bin/env qore
# the dispatch capability of the HTTP server is checked against the record of the loaded HttpServer module when it is
# first used; a check that fails (here the file of the module, loaded from a copy, is removed before the first
# request, so that it cannot be resolved) makes the capability unusable: it makes no request current, so that the
# server's stream requests are refused, and does not fail, so that its other requests are served (qore#6843); run by
# ../HttpServerDispatchCapability.qtest with a module path whose only module named HttpServer is a copy that this
# script may remove

%modern
%requires HttpServer
%requires Logger

class PlainHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        return <HttpResponseInfo>{"code": 200, "body": "plain"};
    }
}

class StreamHandler inherits AbstractHttpRequestHandler {
    constructor() : AbstractHttpRequestHandler(NOTHING, True) {
    }
}

int sub status(int port, string path) {
    Socket s();
    s.connect("127.0.0.1:" + port);
    on_exit s.close();
    s.send("POST /" + path + " HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello");
    return s.readHTTPHeader(10s).status_code;
}

# the file of the loaded module cannot be resolved when the capability is first used
unlink(get_module_hash().HttpServer.filename);

HttpServer server(<HttpServerOptionInfo>{"logger": new Logger("server", LoggerLevel::getLevelFatal())});
on_exit server.stop();
server.setHandler("plain", "^plain", NOTHING, new PlainHandler());
server.setHandler("stream", "^stream", NOTHING, new StreamHandler());
int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
printf("plain: %d, stream: %d, plain again: %d\n", status(port, "plain"), status(port, "stream"),
    status(port, "plain"));
