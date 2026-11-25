# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A shared object of a class that inherits a class of another library module: the base class's method updates it in
# a scope; a closure of the base class that a tenant obtains runs once only, through a helper of the base class's
# module, and not through a helper of the derived class's module, as its code is not that module's (qore#6843); see
# shared-object-cross-module.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%append-module-path "${SCRIPT_DIR}/modules"
%requires SharedScopeDerived

# the code of a tenant: a sandboxed Program; it gets the objects from trusted code and calls their methods untyped
const TenantCode = "
%modern

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
        \"the base class updates the object in a scope\": sub (object d) { d.fill(); },
        \"a closure of the base class through a helper of its module\": sub (object d) {
            d.forward(d.getUpdate());
        },
        \"the same closure a second time\": sub (object d) {
            code c = d.getUpdate();
            d.forward(c);
            d.forward(c);
        },
        \"a closure of the base class through a helper of the derived class's module\": sub (object d) {
            d.forwardDerived(d.getUpdate());
        },
        \"the tenant's closure through a helper of the base class's module\": sub (object d) {
            d.forward(sub () { d.v = \"tenant\"; });
        },
        \"a closure of the base class, run by the tenant\": sub (object d) {
            d.getUpdate()();
        },
    };
}

list<string> sub change_names() {
    return keys changes();
}

string sub try_one(string name, object d) {
    return try_change(sub () { changes(){name}(d); });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

foreach string name in (p.callFunction("change_names")) {
    object d = SharedScopeDerived::make();
    printf("%s: %s, v: %y\n", name, p.callFunction("try_one", name, d), d.v);
}
