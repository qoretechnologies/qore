#!/usr/bin/env qore
# the copy of an object of a class that trusted code declares, made by sandboxed code, keeps the private state of the
# object private: a class that sandboxed code declares cannot read the private members of the copy nor of the
# original in its own copy() method, and the copy's keys and its conversion to a hash give the public members only;
# the trusted class's copy() runs as it does for trusted code (qore#6843); run by
# ../sandbox-trusted-private-access.qtest in every execution mode

%modern

#! a class that trusted code declares, with private state that its copy() method changes
class Trusted {
    public {
        string pub = "pub";
    }

    private {
        string secret = "secret";
    }

    copy() {
        secret += "+copied";
    }

    string reveal() {
        return secret;
    }
}

#! holds an object of the trusted class that the host shares with the tenant
class Holder {
    public {
        static Trusted o();
    }
}

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.importClass("Trusted");
    p.importClass("Holder");
    p.setSandboxManager(new SandboxManager());
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

# the static forms: refused at parse time
const ParseCases = (
    "copy-member": "string sub f() { Trusted c = Holder::o.copy(); return c.secret; }",
    "subclass-copy-member": "class T inherits Trusted { copy(T old) { string s = old.secret; } }",
    "subclass-copy-own-member": "class T inherits Trusted { copy(T old) { string s = secret; } }",
    "subclass-copy-public": "class T inherits Trusted { public { string seen; } copy(T old) { seen = old.pub; } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

const Tenant = "
class T inherits Trusted {
    public {
        string seen;
    }

    copy(T old) {
        auto o = alias(old);
        try {
            seen = o.secret;
        } catch (hash<ExceptionInfo> ex) {
            seen = ex.err;
        }
    }

    static auto alias(auto x) { return x; }
}

auto sub copyMember() { auto c = T::alias(Holder::o.copy()); return c.secret; }
auto sub copyKeys() { return keys Holder::o.copy(); }
auto sub copyHash() { return {} + Holder::o.copy(); }
auto sub copyTrustedCode() { return Holder::o.copy().reveal(); }
auto sub subclassCopy() { T t(); T c = t.copy(); return (c.seen, c.reveal(), keys c); }
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

foreach string f in (("copyMember", "copyKeys", "copyHash", "copyTrustedCode", "subclassCopy")) {
    show(f, auto sub () { return p.callFunction(f); });
}
# the shared object itself is not changed by the copies
show("trusted", auto sub () { return (Holder::o.reveal(), Holder::o.copy().reveal()); });
