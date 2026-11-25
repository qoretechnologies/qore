# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Sandboxed code cannot change the members of a shared object that a builtin class keeps in external storage (an
# undeclared member that a member write gate stores, a declared member with accessors) (qore#6843); see
# shared-object-writes.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires cppapiuser

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires cppapiuser

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<string, code> sub changes() {
    return {
        \"undeclared member of a write gate\": sub (object o) { o.dyn = 2; },
        \"compound assignment of an undeclared member\": sub (object o) { o.dyn += 2; },
        \"remove of an undeclared member\": sub (object o) { auto v = remove o.dyn; },
        \"delete of an undeclared member\": sub (object o) { delete o.dyn; },
        \"declared member with accessors\": sub (object o) { o.count = 5; },
        \"remove of a declared member with accessors\": sub (object o) { auto v = remove o.count; },
    };
}

list<string> sub change_names() {
    return keys changes();
}

string sub try_one(string name, object o) {
    return try_change(sub () { changes(){name}(o); });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# a new probe of the class that the change is for
object sub make(string name) {
    object o = name =~ /undeclared/ ? new CppApiTest::MemberWriteProbe() : new CppApiTest::MemberAccessorProbe();
    if (name =~ /undeclared/) {
        o.dyn = 1;
    } else {
        o.count = 1;
    }
    return o;
}

# the value of the member that the change is for, or the exception that reading it raises
string sub value(string name, object o) {
    try {
        return sprintf("%y", name =~ /undeclared/ ? o.dyn : o.count);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

foreach string name in (p.callFunction("change_names")) {
    object shared = make(name);
    SandboxManager::shareObject(shared);
    string before = value(name, shared);
    string rv = p.callFunction("try_one", name, shared);
    object plain = make(name);
    string plain_before = value(name, plain);
    string plain_rv = p.callFunction("try_one", name, plain);
    printf("%s: shared: %s, %s; not shared: %s, %s\n", name, rv, value(name, shared) == before ? "unchanged" : "CHANGED",
        plain_rv, value(name, plain) == plain_before ? "unchanged" : "changed");
}
