#!/usr/bin/env qore
# the dispatch capability of the HTTP server is checked against the record of the loaded HttpServer module when it is
# first used; a check that cannot be made (here the file of the module, loaded from a copy, is removed before the first
# request, so that it cannot be resolved) makes no request current for the request being dispatched, so that the
# server's stream requests are refused while its other requests are served, and it is made again for the next request:
# once the file is back, the stream requests are served; the server logs the failed check once (qore#6843); run by
# ../HttpServerDispatchCapability.qtest with a module path whose only module named HttpServer is a copy that this
# script may remove and write again

%modern
%requires HttpServer
%requires Logger

class PlainHandler inherits AbstractHttpRequestHandler {
    hash<HttpResponseInfo> handleRequest(HttpListenerInterface listener, Socket s, hash<auto> cx, hash<auto> hdr,
            *data body) {
        return <HttpResponseInfo>{"code": 200, "body": "plain"};
    }
}

class StreamRequest inherits AbstractStreamRequest {
    constructor(HttpListenerInterface listener, AbstractHttpRequestHandler handler, Socket s, hash<auto> cx,
            hash<auto> hdr, auto body) : AbstractStreamRequest(listener, handler, s, cx, hdr, body) {
    }

    private nothing recvImpl(hash<auto> v) {
    }

    private hash<HttpResponseInfo> getResponseHeaderMessageImpl() {
        return <HttpResponseInfo>{"code": 200, "body": "stream"};
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

#! counts the messages of the server about its dispatch capability
class CapabilityLog inherits LoggerAppenderWithLayout {
    public {
        list<string> messages = ();
    }

    constructor() : LoggerAppenderWithLayout("capability", new LoggerLayoutPattern("%m")) {
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
    s.connect("127.0.0.1:" + port);
    on_exit s.close();
    s.send("POST /" + path + " HTTP/1.1\r\nHost: x\r\nContent-Length: 5\r\nConnection: close\r\n\r\nhello");
    return s.readHTTPHeader(10s).status_code;
}

# the file of the loaded module cannot be resolved when the capability is first used
string file = get_module_hash().HttpServer.filename;
string src = ReadOnlyFile::readTextFile(file);
unlink(file);

CapabilityLog log();
Logger logger("server", LoggerLevel::getLevelError());
logger.addAppender(log);
HttpServer server(<HttpServerOptionInfo>{"logger": logger});
on_exit server.stop();
server.setHandler("plain", "^plain", NOTHING, new PlainHandler());
server.setHandler("stream", "^stream", NOTHING, new StreamHandler());
int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
printf("plain: %d, stream: %d, plain again: %d\n", status(port, "plain"), status(port, "stream"),
    status(port, "plain"));
# the file is back: the check is made again, and the stream requests are served
{
    File f();
    f.open2(file, O_CREAT | O_WRONLY | O_TRUNC);
    f.write(src);
}
printf("stream after: %d, stream again: %d\n", status(port, "stream"), status(port, "stream"));
printf("logged: %d %s\n", log.messages.size(), (log.messages[0] =~ /cannot be checked/) ? "(check not made)" : "");
