#!/usr/bin/env qore
# a Program that may not use uncontrolled APIs (PO_NO_UNCONTROLLED_APIS) loads the HttpServerUtil module first, from
# its source, so that the module's Program has its options; the module must parse all the same; the dispatch
# capability of the HTTP server that the initialization of the HttpServer module then claims cannot be checked with the
# system policy, so it fails closed: the server's stream requests are refused while its other requests are served, and
# the server logs it once; with "trusted" as the argument the first loader is not restricted, and the capability works
# (qore#6843); run by ../HttpServerDispatchCapability.qtest with "restricted" or "trusted" as its argument, with a
# module path whose first directory has the source modules only

%modern
%no-child-restrictions

Program loader(PO_NEW_STYLE | (ARGV[0] == "restricted" ? PO_NO_UNCONTROLLED_APIS : 0));
try {
    loader.parse("%modern\n%requires HttpServerUtil\n", "loader");
    printf("loaded: %y\n", get_module_hash().HttpServerUtil.filename =~ /\.qm$/);
} catch (hash<ExceptionInfo> ex) {
    printf("loaded: %s: %s\n", ex.err, ex.desc);
    exit(1);
}

# the server, in a Program that is not restricted, which loads the HttpServer module
Program s(PO_NEW_STYLE);
s.parse("%requires HttpServer
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

# collects the messages of the server about its dispatch capability
class CapabilityLog inherits LoggerAppenderWithLayout {
    public {
        list<string> messages = ();
    }

    constructor() : LoggerAppenderWithLayout('capability', new LoggerLayoutPattern('%m')) {
        open();
    }

    processEventImpl(int type, auto params) {
        if (type == EVENT_LOG && params =~ /dispatch capability/) {
            messages += params;
        }
    }
}

int sub status(int port, string path) {
    Socket s();
    s.connect('127.0.0.1:' + port);
    on_exit s.close();
    s.send(sprintf('POST /%s HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello', path));
    return s.readHTTPHeader(10s).status_code;
}

string sub run() {
    CapabilityLog log();
    Logger logger('server', LoggerLevel::getLevelError());
    logger.addAppender(log);
    HttpServer server(<HttpServerOptionInfo>{'logger': logger});
    on_exit server.stop();
    server.setHandler('plain', '^plain', NOTHING, new PlainHandler());
    server.setHandler('stream', '^stream', NOTHING, new StreamHandler());
    int port = server.addListener(<HttpListenerOptionInfo>{'node': '127.0.0.1', 'service': 0}).port;
    string rv = sprintf('plain: %d, stream: %d, plain again: %d, stream again: %d', status(port, 'plain'),
        status(port, 'stream'), status(port, 'plain'), status(port, 'stream'));
    rv += sprintf('\nlogged: %d%s', log.messages.size(),
        (log.messages[0] =~ /may not use uncontrolled APIs/) ? ' (no system-policy call)' : '');
    return rv;
}
", "server");
printf("%s\n", s.callFunction("run"));
