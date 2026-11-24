#!/usr/bin/env qore
# the call Program context of the body of a function that a fast call runs in another Program, when the body runs again
# in the AST interpreter (the retry after a guard failure or an invalidation, forced with the debug switch
# dbg_jit_force_deopt()); run by ../function-call-context-deopt.qtest (debug builds only)
# Each line is "<case>: <result>"; the frames are named by their Program: host (this script), T (a child Program), TS
# (a sandboxed child Program) and M (the library module FcxLib)

%modern
%no-child-restrictions
%requires FcxLib

const ChildCode = "%modern
%requires FcxLib
list<string> sub g() { return FcxLib::Lib::frames(); }
*code sub mk(string shape) {
    switch (shape) {
        case \"function\": return list<string> sub () { return g(); };
        case \"module\": return list<string> sub () { return FcxLib::Lib::frames(); };
        case \"module-function\": return list<string> sub () { return FcxLib::libs(); };
        case \"register\": return string sub () { return FcxLib::libregister(); };
        case \"checks\": return string sub () { return FcxLib::libcheck(); };
    }
}
";

Program t(PO_NEW_STYLE);
t.parse(ChildCode, "T");
Program ts(PO_NEW_STYLE);
ts.setSandboxManager(new SandboxManager());
ts.parse(ChildCode, "TS");

hash<string, string> names = {
    FcxLib::Lib::callerId().toString(): "host",
    t.getProgramId().toString(): "T",
    ts.getProgramId().toString(): "TS",
    FcxLib::Lib::ownId().toString(): "M",
};

string sub fmt(auto v) {
    if (v.typeCode() != NT_LIST) {
        return v;
    }
    list<string> out = ();
    foreach string f in (v) {
        (*string fn, *string id) = (f =~ x/^(.*)=(-?[0-9]+)$/);
        out += sprintf("%s:%s", fn, names{id} ?? "other");
    }
    return out.join(" ");
}

class HostCls {
    static auto run(code c) { return c(); }
}

# the calls run once before the retry is forced, so that the fast call paths and native code run them
foreach hash<auto> i in ({"T": t, "TS": ts}.pairIterator()) {
    foreach string shape in (("function", "module", "module-function", "register", "checks")) {
        if (i.key == "TS" && shape !~ /^(register|checks|module-function)$/) {
            continue;
        }
        code c = i.value.callFunction("mk", shape);
        for (int j = 0; j < 300; ++j) {
            HostCls::run(c);
        }
        call_function("dbg_jit_force_deopt", True);
        auto v;
        try {
            v = HostCls::run(c);
        } catch (hash<ExceptionInfo> ex) {
            call_function("dbg_jit_force_deopt", False);
            rethrow;
        }
        call_function("dbg_jit_force_deopt", False);
        printf("%s-deopt-%s: %s\n", i.key, shape, fmt(v));
    }
}
