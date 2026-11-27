#!/usr/bin/env qore
# a stream request class that sandboxed code declares reads the request through the public methods of
# AbstractStreamRequest, which give a copy of the request metadata only, and overrides sendResponse() calling the
# version of the library class; the request handler gives the code that serves the request the request without the
# server's objects and the credentials of the request, also in the arguments of the factory and the constructor, and
# its trusted code gets the raw request with getRawRequest() (qore#6843); run by
# ../sandbox-trusted-private-access.qtest in every execution mode

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires HttpServerUtil
%requires HttpServer

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

const Tenant = "
%requires reflection

class TS inherits HttpServer::AbstractStreamRequest {
    private {
        hash<auto> facts;
        hash<auto> args;
        bool sent;
    }

    constructor(HttpServer::HttpListenerInterface listener, HttpServer::AbstractHttpRequestHandler handler,
            Qore::Socket s, hash<auto> cx, hash<auto> hdr, auto body)
            : HttpServer::AbstractStreamRequest(listener, handler, s, cx, hdr, body) {
        # what the constructor's arguments have
        args = {
            'credentials': (map $1, ('authorization', 'proxy-authorization', 'cookie'),
                hdr.hasKey($1) || cx.hdr.hasKey($1)),
            'server objects': (map $1, ('auth', 'socket', 'socket-info', 'uctx', 'logger'), cx.hasKey($1)),
            'peer-info keys': sort(keys cx.'peer-info'),
            # the members of the object that the subclass can read
            'members': sort(keys self),
            # the raw request in the members of the library class, through reflection
            'reflection cx': reflect('cx'),
            'reflection hdr': reflect('hdr'),
            'reflection raw_cx': reflect('raw_cx'),
            'reflection raw_hdr': reflect('raw_hdr'),
        };
    }

    string reflect(string member) {
        try {
            return sprintf('%y', cast<Reflection::NormalMember>(Reflection::Class::forName('HttpServer::AbstractStreamRequest')
                .getMember(member)).getValue(self));
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    }

    # an override of the private method of the library class that calls the version of the library class
    private hash<HttpServer::HttpHandlerResponseInfo> sendResponse() {
        hash<auto> cx = getContext();
        facts = {
            'id': cx.id.typeCode() == NT_INT,
            'orig_path': cx.orig_path,
            'method': cx.hdr.method,
            'headers-raw': cx.'header-info'.'headers-raw'.typeCode() == NT_HASH,
            'header-info keys': sort(keys cx.'header-info'),
            'context keys': sort(keys cx),
            'headers method': getHeaders().method,
            'body': getBody(),
            # the credentials of the request are not given
            'credentials': (map $1, ('authorization', 'proxy-authorization', 'cookie'),
                cx.hdr.hasKey($1) || getHeaders().hasKey($1) || cx.hdr.headers.hasKey($1)
                || (cx.'header-info'.'headers-raw'.keys() ?? ()).contains($1)
                || (map $1.lwr(), keys cx.'header-info'.'headers-raw').contains($1)),
        };
        # the context given is a copy: changing it changes nothing in the request
        cx.id = -1;
        cx.'header-info'.x = 1;
        facts.copy = getContext().id != -1 && !getContext().'header-info'.hasKey('x');
        facts.constructor = args;
        return HttpServer::AbstractStreamRequest::sendResponse();
    }

    private hash<HttpServer::HttpResponseInfo> getResponseHeaderMessageImpl() {
        return <HttpServer::HttpResponseInfo>{
            'code': 200,
            'hdr': {'Content-Type': 'text/plain', 'Transfer-Encoding': 'chunked'},
        };
    }

    private auto sendImpl() {
        if (sent) {
            return;
        }
        sent = True;
        return sprintf('%y', facts);
    }
}

HttpServer::AbstractStreamRequest sub make_request(HttpServer::HttpListenerInterface listener,
        HttpServer::AbstractHttpRequestHandler handler, Qore::Socket s, hash<auto> cx, hash<auto> hdr, auto body) {
    return new TS(listener, handler, s, cx, hdr, body);
}
";

Program p(Opts);
p.setSandboxManager(new SandboxManager());
p.loadModule("HttpServerUtil");
p.parse(Tenant, "tenant");

#! the handler of the host: the request object is the tenant's
class TenantHandler inherits AbstractHttpRequestHandler {
    public {
        #! the keys of the context of the server and of its header-info hash that the tenant does not get
        list<string> withheld;
        #! the credentials in the raw request, which the trusted code of the handler gets
        list<string> raw_credentials;
        #! the keys of the context that the code serving the request gets that the raw context has
        list<string> filtered_has;
    }

    private {
        Program p;
    }

    constructor(Program p) : AbstractHttpRequestHandler(NOTHING, True) {
        self.p = p;
        # the handler hands its requests to the code of a tenant: it gets a socket of each request (qore#7305), and the
        # request is filtered for the tenant's code (qore#6843)
        setHandlerSocketView();
        setFilterRequests();
    }

    private AbstractStreamRequest getStreamRequestImpl(HttpListenerInterface listener, Socket s, hash<auto> cx,
            hash<auto> hdr, *data body) {
        # the raw request, as the server made it, which only the trusted code of the handler gets
        hash<auto> raw = getRawRequest();
        # the context of the server has objects that are not request metadata
        withheld = sort(keys (raw.cx - AbstractStreamRequest::ContextKeys))
            + sort(map "header-info." + $1, keys (raw.cx."header-info" - AbstractStreamRequest::HeaderInfoKeys));
        raw_credentials = map $1, ("authorization", "proxy-authorization", "cookie"), raw.hdr.hasKey($1);
        filtered_has = map $1, ("auth", "socket", "socket-info", "uctx"), cx.hasKey($1);
        return p.callFunction("make_request", listener, self, s, cx, hdr, body);
    }
}

TenantHandler handler(p);
HttpServer server(<HttpServerOptionInfo>{"debug": True});
on_exit server.stop();
server.setHandler("tenant", "/ts", NOTHING, handler);
int port = server.addListener(<HttpListenerOptionInfo>{"node": "127.0.0.1", "service": 0}).port;
server.waitForAsyncIo();

HTTPClient client({"url": "http://127.0.0.1:" + port});
hash<auto> info;
string body = client.send("request-body", "POST", "/ts", {
    "Content-Type": "text/plain",
    "Authorization": "Basic dGVzdDp0ZXN0",
    "Proxy-Authorization": "Basic dGVzdDp0ZXN0",
    "Cookie": "session=secret",
}, False, \info).body;
on_exit client.disconnect();
printf("response: %s\n", body);
printf("withheld: %y\n", handler.withheld);
printf("raw credentials for the trusted handler: %y\n", handler.raw_credentials);
printf("server objects that the handler's code gets: %y\n", handler.filtered_has);

# a request handler class that sandboxed code declares cannot get the raw request
{
    Program h(Opts);
    h.setSandboxManager(new SandboxManager());
    h.loadModule("HttpServerUtil");
    try {
        h.parse("class TH inherits HttpServer::AbstractHttpRequestHandler {
            hash<auto> raw(hash<auto> cx) { return getRawRequest(); } }", "tenant-handler");
        printf("tenant handler: raw request: parse: OK\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("tenant handler: raw request: parse: %s\n", ex.err);
    }
}
