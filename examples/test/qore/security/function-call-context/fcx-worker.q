#!/usr/bin/env qore
# the call Program context of the body of a function and of a closure (qore#7394): run by ../function-call-context.qtest
# Each line is "<case>: <result>"; the frames are named by their Program: host (this script), T (a child Program), TS
# (a sandboxed child Program), TR (a child Program restricted from process control, databases and terminal I/O) and M
# (the library module FcxLib)

%modern
%no-child-restrictions
%requires FcxLib
%requires YamlTagHandler
%requires AsyncSocketIo

#! the code of the child Programs
const ChildCode = "%modern
%requires FcxLib
list<string> sub g() { return FcxLib::Lib::frames(); }
list<string> sub g1() { return g(); }
list<string> sub gs() { return TS::s(); }
class TS {
    static list<string> s() { return FcxLib::Lib::frames(); }
    list<string> m() { return FcxLib::Lib::frames(); }
}
*code sub mk(string shape) {
    switch (shape) {
        case \"function\": return list<string> sub () { return g(); };
        case \"static\": return list<string> sub () { return TS::s(); };
        case \"module\": return list<string> sub () { return FcxLib::Lib::frames(); };
        case \"method\": return list<string> sub () { TS o(); return o.m(); };
        case \"function-chain\": return list<string> sub () { return g1(); };
        case \"function-static\": return list<string> sub () { return gs(); };
        case \"module-function\": return list<string> sub () { return FcxLib::libs(); };
    }
}
string sub check_register() { return FcxLib::libregister(); }
string sub check_direct() { return FcxLib::libcheck(); }
string sub check_static() { return FcxLib::libcheck_static(); }
string sub check_method() { return FcxLib::Lib::checks(); }
code sub mk_check(string which) {
    switch (which) {
        case \"direct\": return string sub () { return FcxLib::libcheck(); };
        case \"static\": return string sub () { return FcxLib::libcheck_static(); };
        case \"register\": return string sub () { return FcxLib::libregister(); };
    }
    return string sub () { return FcxLib::Lib::checks(); };
}
# the host's code, called by this Program's code through a function of the library module
auto sub call_host(code c) { return FcxLib::libcall(c); }
auto sub call_direct(code c) { return c(); }
";

const Shapes = ("function", "static", "module", "method", "function-chain", "function-static", "module-function");

Program t(PO_NEW_STYLE);
t.parse(ChildCode, "T");
Program ts(PO_NEW_STYLE);
ts.setSandboxManager(new SandboxManager());
ts.parse(ChildCode, "TS");
Program tr(PO_NEW_STYLE | PO_NO_PROCESS_CONTROL | PO_NO_DATABASE | PO_NO_TERMINAL_IO);
tr.parse(ChildCode, "TR");

hash<string, string> names = {
    FcxLib::Lib::callerId().toString(): "host",
    t.getProgramId().toString(): "T",
    ts.getProgramId().toString(): "TS",
    tr.getProgramId().toString(): "TR",
    FcxLib::Lib::ownId().toString(): "M",
};

string sub fmt(list<string> l) {
    list<string> out = ();
    foreach string f in (l) {
        (*string fn, *string id) = (f =~ x/^(.*)=(-?[0-9]+)$/);
        out += sprintf("%s:%s", fn, names{id} ?? "other");
    }
    return out.join(" ");
}

class HostCls {
    static auto run(code c) { return c(); }
}

# 1) the frames of the calls that the code of a closure of T makes, when the host calls the closure; and after the calls
# have run many times, which runs them on other paths (the IR fast paths and native code)
foreach string shape in (Shapes) {
    code c = t.callFunction("mk", shape);
    auto cold = HostCls::run(c);
    auto warm;
    for (int i = 0; i < 3000; ++i) {
        warm = HostCls::run(c);
    }
    printf("closure-%s: %s\n", shape, fmt(cold));
    printf("closure-%s-warm: %s\n", shape, fmt(warm));
}
# a plain function of the library called by the host
printf("host-module-function: %s\n", fmt(FcxLib::libs()));
# a function of T called through Program::callFunction()
printf("call-function: %s\n", fmt(t.callFunction("g1")));

# 2) what library code sees of the code that calls it: the sandboxed Program TS and the restricted Program TR call
# a plain function of the library whose body makes the checks, one that makes them through a static method of the
# library, and the static method itself; directly, from a closure that the host calls, and when the library calls
# code of the host for them
foreach hash<auto> i in ({"TS": ts, "TR": tr}.pairIterator()) {
    Program p = i.value;
    foreach string which in (("direct", "static", "method", "register")) {
        printf("%s-%s: %s\n", i.key, which, p.callFunction("check_" + which));
        code c = p.callFunction("mk_check", which);
        auto cold = HostCls::run(c);
        auto warm;
        for (int j = 0; j < 3000; ++j) {
            warm = HostCls::run(c);
        }
        printf("%s-%s-closure: %s\n", i.key, which, cold);
        printf("%s-%s-closure-warm: %s\n", i.key, which, warm);
    }
    # the host's code that this Program's code calls, through a function of the library and directly
    code host_check = string sub () { return FcxLib::libcheck(); };
    printf("%s-host-code-via-library: %s\n", i.key, p.callFunction("call_host", host_check));
    printf("%s-host-code: %s\n", i.key, p.callFunction("call_direct", host_check));
    # the process-control capability that a frame check of a library method sees when the host's code calls it for
    # this Program: the host's code makes the call
    code host_mask = string sub () { return FcxLib::Lib::callerMask(); };
    printf("%s-host-code-mask: %s\n", i.key, p.callFunction("call_direct", host_mask));
}

# 3) the plain functions of the standard modules that check the code that calls them, called by the sandboxed and the
# restricted Program
const LibraryChecks = "%modern
%requires YamlTagHandler
%requires AsyncSocketIo
string sub yaml_handlers() { return sprintf(\"%y\", YamlTagHandler::get_yaml_tag_handlers()); }
string sub yaml_registry() {
    try {
        YamlTagHandler::globalTagRegistry();
        return \"granted\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub async_controller() {
    return AsyncSocketIo::getGlobalAsyncIoController() == AsyncSocketIo::getGlobalAsyncIoController()
        ? \"global\" : \"own\";
}
code sub mk(string which) {
    switch (which) {
        case \"yaml-handlers\": return string sub () { return yaml_handlers(); };
        case \"yaml-registry\": return string sub () { return yaml_registry(); };
    }
    return string sub () { return async_controller(); };
}
";
Program lts(PO_NEW_STYLE);
lts.setSandboxManager(new SandboxManager());
lts.parse(LibraryChecks, "TS-library");
Program ltr(PO_NEW_STYLE | PO_NO_PROCESS_CONTROL | PO_NO_DATABASE | PO_NO_TERMINAL_IO);
ltr.parse(LibraryChecks, "TR-library");
foreach hash<auto> i in ({"TS": lts, "TR": ltr}.pairIterator()) {
    Program p = i.value;
    foreach string which in (("yaml-handlers", "yaml-registry", "async-controller")) {
        string fn = replace(which, "-", "_");
        printf("%s-lib-%s: %s\n", i.key, which, p.callFunction(fn));
        code c = p.callFunction("mk", which);
        auto warm;
        for (int j = 0; j < 300; ++j) {
            warm = HostCls::run(c);
        }
        printf("%s-lib-%s-closure: %s\n", i.key, which, warm);
    }
}

# 4) the process-wide settings of the standard modules that only the frame of the call checks (a caller restricted from
# process control may not change them): a Program restricted from process control calls them directly, from a closure
# that calls a function of its own that makes the call, and from a closure that makes the call itself, which the host
# calls: its code makes the call in every case
const FrameChecks = "%modern
%requires DataProvider
%requires RestSchemaActions
string sub retry_unit() {
    try {
        DataProvider::DataProviderRetryUtil::setTransportRetryDelayUnit(
            DataProvider::DataProviderRetryUtil::getTransportRetryDelayUnit());
        return \"granted\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
string sub eager_types() {
    try {
        RestSchemaActions::RestSchemaActionSet::setEagerResponseTypes(False);
        return \"granted\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
code sub mk(string which) {
    return which == \"retry-unit\" ? string sub () { return retry_unit(); } : string sub () { return eager_types(); };
}
code sub mk_direct(string which) {
    if (which == \"retry-unit\") {
        return string sub () {
            try {
                DataProvider::DataProviderRetryUtil::setTransportRetryDelayUnit(
                    DataProvider::DataProviderRetryUtil::getTransportRetryDelayUnit());
                return \"granted\";
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
        };
    }
    return string sub () {
        try {
            RestSchemaActions::RestSchemaActionSet::setEagerResponseTypes(False);
            return \"granted\";
        } catch (hash<ExceptionInfo> ex) {
            return ex.err;
        }
    };
}
";
Program lfr(PO_NEW_STYLE | PO_NO_PROCESS_CONTROL);
lfr.parse(FrameChecks, "TR-frame");
foreach string which in (("retry-unit", "eager-types")) {
    printf("TR-frame-%s: %s\n", which, lfr.callFunction(which == "retry-unit" ? "retry_unit" : "eager_types"));
    foreach string how in (("mk", "mk_direct")) {
        code c = lfr.callFunction(how, which);
        auto cold = HostCls::run(c);
        auto warm;
        for (int j = 0; j < 300; ++j) {
            warm = HostCls::run(c);
        }
        printf("TR-frame-%s-%s: %s\n", which, how, cold);
        printf("TR-frame-%s-%s-warm: %s\n", which, how, warm);
    }
}
