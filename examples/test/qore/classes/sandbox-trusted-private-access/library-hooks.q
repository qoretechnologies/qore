#!/usr/bin/env qore
# the uses of the private hooks of library classes that stay allowed for a class that sandboxed code declares, and the
# private declarations of the library classes that it cannot use (qore#6843); run by
# ../sandbox-trusted-private-access.qtest in every execution mode

%modern

%prepend-module-path "${SCRIPT_DIR}/../../../../../qlib"
%requires DataProvider
%requires Schema
%requires Sqlite3SqlUtil
%requires RestHandler

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the library modules and the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.setSandboxManager(new SandboxManager());
    p.loadModule("DataProvider");
    p.loadModule("Schema");
    p.loadModule("Sqlite3SqlUtil");
    p.loadModule("RestHandler");
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

# the static uses of another private method of the library class: refused at parse time
const ParseCases = (
    "rest-other-private": "class R inherits RestHandler::RestHandler {
        constructor() : RestHandler::RestHandler() {}
        hash<auto> f() { return errorResponseHeaders({}) ?? {}; } }",
    "rest-other-private-base-call": "class R inherits RestHandler::RestHandler {
        constructor() : RestHandler::RestHandler() {}
        hash<auto> f() { return RestHandler::errorResponseHeaders({}) ?? {}; } }",
    "processor-other-private": "class P inherits DataProvider::AbstractDataProcessor {
        bool f() { return supportsBulkApiImpl(); } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

const Tenant = "
# a processor that overrides the private hooks of the library class privately and calls its own override
# recursively
class P1 inherits DataProvider::AbstractDataProcessor {
    private submitImpl(code enqueue, auto _data) {
        if (_data.typeCode() == NT_LIST) {
            map submitImpl(enqueue, $1), _data;
            return;
        }
        enqueue(_data + 1);
    }
    private flushImpl(code enqueue) {
        enqueue('flushed');
    }
    private bool supportsBulkApiImpl() {
        return False;
    }
}

# a processor that overrides the private hook publicly
class P2 inherits DataProvider::AbstractDataProcessor {
    submitImpl(code enqueue, auto _data) {
        if (_data.typeCode() == NT_LIST) {
            map submitImpl(enqueue, $1), _data;
            return;
        }
        enqueue(_data * 10);
    }
}

# a schema that implements the private abstract hooks of the library class
class S inherits Schema::AbstractSchema {
    constructor(Qore::SQL::AbstractDatasource ds) : Schema::AbstractSchema(ds) {
    }
    private string getNameImpl() {
        return 'tenant-schema';
    }
    private string getVersionImpl() {
        return '1.0';
    }
    private *hash<auto> getTablesImpl() {
        return {'t1': {}};
    }
}

# a REST handler that overrides a private method of the library class and calls the version of the library class
class R inherits RestHandler::RestHandler {
    constructor() : RestHandler::RestHandler() {
    }
    private hash<HttpServer::HttpHandlerResponseInfo> returnRestException(hash<ExceptionInfo> ex) {
        hash<HttpServer::HttpHandlerResponseInfo> rv = RestHandler::returnRestException(ex);
        rv.hdr.'X-Tenant' = 'yes';
        return rv;
    }
    hash<auto> callOverride() {
        hash<HttpServer::HttpHandlerResponseInfo> rv = returnRestException(<ExceptionInfo>{
            'err': 'TENANT-ERROR',
            'desc': 'tenant error',
        });
        return {'code': rv.code, 'tenant': rv.hdr.'X-Tenant'};
    }
    # another private method of the library class, called through a value without a type
    static auto otherPrivate(auto h) {
        return h.errorResponseHeaders({});
    }
    static auto alias(auto x) {
        return x;
    }
}

auto sub run_p(string cls) {
    list<auto> out;
    object p = create_object(cls);
    code enqueue = sub (auto v) { out += v; };
    p.submit(enqueue, (1, 2, 3));
    p.flush(enqueue);
    return out;
}

auto sub run_p2_public() {
    list<auto> out;
    P2 p();
    p.submitImpl(sub (auto v) { out += v; }, 4);
    return out;
}

auto sub run_s(Qore::SQL::AbstractDatasource ds) {
    S s(ds);
    return (s.getName(), s.getVersion(), keys s.getTables());
}

auto sub run_r() {
    R r();
    return r.callOverride();
}

auto sub run_r_other() {
    R r();
    return R::otherPrivate(R::alias(r));
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

show("processor private overrides", auto sub () { return p.callFunction("run_p", "P1"); });
show("processor public override", auto sub () { return p.callFunction("run_p", "P2"); });
show("processor public override called", auto sub () { return p.callFunction("run_p2_public"); });
Datasource ds("sqlite3:x@:memory:");
show("schema hooks", auto sub () { return p.callFunction("run_s", ds); });
show("rest override and base call", auto sub () { return p.callFunction("run_r"); });
show("rest other private", auto sub () { return p.callFunction("run_r_other"); });
