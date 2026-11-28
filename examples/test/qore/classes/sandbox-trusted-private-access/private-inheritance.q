#!/usr/bin/env qore
# a class that sandboxed code declares cannot use the public members and methods that a class that trusted code
# declares inherits privately; its own private inheritance of a trusted class is not affected (qore#6843); run by
# ../sandbox-trusted-private-access.qtest in every execution mode

%modern

#! a class that trusted code declares
class X {
    public {
        string xpub = "xpub";
    }

    string xm() {
        return "xm";
    }
}

#! a class that trusted code declares, which inherits X privately
class D inherits private X {
    string useX() {
        return xpub + ":" + xm();
    }
}

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.importClass("X");
    p.importClass("D");
    p.setSandboxManager(new SandboxManager());
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

const ParseCases = (
    "member-self": "class T inherits D { string f() { return self.xpub; } }",
    "method-self": "class T inherits D { string f() { return xm(); } }",
    "method-self-dot": "class T inherits D { string f() { return self.xm(); } }",
    "method-typed": "class T inherits D { static string f(D d) { return d.xm(); } }",
    "member-typed": "class T inherits D { static string f(D d) { return d.xpub; } }",
    # the class's own private inheritance of a trusted class
    "own-private-inheritance": "class U inherits private X { string f() { return self.xpub + xm(); } }",
    # a public method of the class that inherits privately
    "public-method": "class T inherits D { string f() { return useX(); } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

const Tenant = "
class T inherits D {
    static auto alias(auto x) { return x; }
    auto memberAuto() { auto o = alias(self); return o.xpub; }
    auto methodAuto() { auto o = alias(self); return o.xm(); }
    static auto otherMemberAuto(auto d) { return d.xpub; }
    static auto otherMethodAuto(auto d) { return d.xm(); }
    auto callObjectMethod() { return call_object_method(self, 'xm'); }
    auto publicMethod() { return useX(); }
}

class U inherits private X {
    auto own() { auto o = T::alias(self); return (self.xpub, xm(), o.xpub, o.xm()); }
}

auto sub runSelf(string cls, string m) {
    return call_object_method(create_object(cls), m);
}

auto sub run(string m, auto arg) {
    return call_static_method('T', m, arg);
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

foreach string m in (("memberAuto", "methodAuto", "callObjectMethod", "publicMethod")) {
    show(m, auto sub () { return p.callFunction("runSelf", "T", m); });
}
D d();
foreach string m in (("otherMemberAuto", "otherMethodAuto")) {
    show(m, auto sub () { return p.callFunction("run", m, d); });
}
show("own private inheritance", auto sub () { return p.callFunction("runSelf", "U", "own"); });
show("trusted", auto sub () { return d.useX(); });
