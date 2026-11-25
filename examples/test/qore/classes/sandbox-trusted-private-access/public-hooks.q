#!/usr/bin/env qore
# the public methods of library classes that subclasses declared by sandboxed code use in place of the private members
# of the library classes (qore#6843); run by ../sandbox-trusted-private-access.qtest in every execution mode

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires FtpPoller
%requires DataProvider
%requires RestHandler
%requires FsUtil
%requires Util

%include ../FtpServer.qc

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the library modules and the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.setSandboxManager(new SandboxManager());
    p.loadModule("FtpPoller");
    p.loadModule("DataProvider");
    p.loadModule("RestHandler");
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

const PollerHead = "class MyPoller inherits FtpPoller::FtpPoller {
    constructor(Qore::FtpClient f) : FtpPoller::FtpPoller(f, {'path': '/', 'mask': '*'}) {}
    singleFileEvent(hash<FtpPoller::FtpPollerFileEventInfo> event) {}
    postSingleFileEvent(hash<FtpPoller::FtpPollerFileEventInfo> event) {}
";

# the private members themselves: refused at parse time
const ParseCases = (
    "ftppoller-fatal-member": PollerHead + "    bool f() { fatal = True; return fatal; } }",
    "observable-map-member": "class MyObservable inherits DataProvider::Observable {
        int f() { return observer_map.size(); } }",
    "rest-stream-cx-member": "class MyRestStream inherits RestHandler::AbstractRestStreamRequestHandler {
        constructor(hash<auto> cx) : RestHandler::AbstractRestStreamRequestHandler(cx) {}
        hash<auto> f() { return cx; } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

const Tenant = PollerHead + "    list<bool> useFatal() {
        list<bool> rv = (isFatal(),);
        setFatal();
        rv += isFatal();
        setFatal(False);
        rv += isFatal();
        return rv;
    }
}

class MyObservable inherits DataProvider::Observable {
    list<auto> useObservers() {
        list<auto> rv = (hasObservers(), getObserverCount());
        DataProvider::Observer o = new MyObserver();
        registerObserver(o);
        rv += (hasObservers(), getObserverCount());
        deregisterObserver(o);
        rv += (hasObservers(), getObserverCount());
        return rv;
    }
}

class MyObserver inherits DataProvider::Observer {
    update(string event_id, hash<auto> data_) {}
}

auto sub run_observable() {
    MyObservable o();
    return o.useObservers();
}

class MyRestStream inherits RestHandler::AbstractRestStreamRequestHandler {
    constructor(hash<auto> cx, *hash<auto> ah) : RestHandler::AbstractRestStreamRequestHandler(cx, ah) {
    }
    private hash<auto> getResponseHeaderMessageImpl() {
        return {'code': 200};
    }
    private nothing recvImpl(hash<auto> v) {
    }
    private auto sendImpl() {
    }
    hash<auto> useContext() {
        hash<auto> cx = getContext();
        hash<auto> rv = {
            'context keys': sort(keys cx),
            'header-info keys': sort(keys cx.'header-info'),
            'args': getArgs(),
        };
        # a copy: changing it changes nothing in the handler
        cx.orig_path = 'changed';
        rv.copy = getContext().orig_path == '/r';
        return rv;
    }
}

auto sub run_rest_stream(hash<auto> cx) {
    MyRestStream r(cx, {'a': 1});
    return r.useContext();
}

auto sub run_poller(Qore::FtpClient f) {
    MyPoller p(f);
    return p.useFatal();
}
";

*Program p = sandboxed("tenant", Tenant);
if (!p) {
    exit(1);
}

sub show(string label, code c) {
    try {
        printf("%s: OK %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

# a client of the host connected to a local server: the poller uses it as it is
FtpServer serv(0, 0, "127.0.0.1");
on_exit serv.shutdown();
FtpClient ftp("ftp://user:pass@127.0.0.1:" + serv.getPort());
ftp.connect();
on_exit ftp.disconnect();
show("ftppoller fatal flag", auto sub () { return p.callFunction("run_poller", ftp); });
show("observable observers", auto sub () { return p.callFunction("run_observable"); });
# a call context as the server and the REST handler set it, with objects of the server
hash<auto> rest_cx = {
    "id": 1,
    "orig_path": "/r",
    "hdr": {"method": "POST"},
    "header-info": {"headers-raw": {"Host": "x"}, "sock": new Socket(), "session_id": 1},
    "async_ctrl": new Mutex(),
    "sock_key": "io-1",
    "logger": new Mutex(),
    "rest_method": "Get",
    "rest_api_path": "r",
};
show("rest stream context", auto sub () { return p.callFunction("run_rest_stream", rest_cx); });
