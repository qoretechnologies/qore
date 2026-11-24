#!/usr/bin/env qore
# a class that sandboxed code declares cannot call the private variants of a method of a class that trusted code
# declares, nor create an object of such a class with a private constructor, also not in its own base-class
# constructor call; it can call the public variants and the public constructors (qore#6843); run by
# ../sandbox-trusted-private-access.qtest in every execution mode

%modern

#! a class that trusted code declares, with a method that has a public and a private variant, and a private
#! constructor beside a public one
class Trusted {
    public {
        string how;
    }

    constructor() {
        how = "public constructor";
    }

    private constructor(string why) {
        how = "private constructor: " + why;
    }

    string op() {
        return "public variant";
    }

    private string op(int i) {
        return "private variant " + i;
    }

    #! trusted code uses the private constructor and variant of its class
    static string viaPrivate() {
        return new Trusted("trusted").how + "/" + new Trusted().op(2);
    }
}

#! a class that trusted code declares with private constructors only
class TrustedPrivateOnly {
    public {
        string how;
    }

    private constructor() {
        how = "private default constructor";
    }

    private constructor(string why) {
        how = "private constructor: " + why;
    }

    #! trusted code creates its objects
    static TrustedPrivateOnly make() {
        return new TrustedPrivateOnly();
    }
}

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.importClass("Trusted");
    p.importClass("TrustedPrivateOnly");
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
    "variant-self": "class T inherits Trusted { string f() { return op(1); } }",
    "variant-self-dot": "class T inherits Trusted { string f() { return self.op(1); } }",
    "variant-typed": "class T inherits Trusted { static string f(Trusted h) { return h.op(1); } }",
    "new-private-ctor": "class T inherits Trusted { static Trusted f() { return new Trusted('x'); } }",
    "new-private-ctor-decl": "class T inherits Trusted { static Trusted f() { Trusted t('x'); return t; } }",
    "base-private-ctor": "class T inherits Trusted { constructor() : Trusted('init') {} }",
    "base-private-ctor-named": "class T inherits Trusted { constructor() : Trusted(why: 'init') {} }",
    "base-private-ctor-only": "class T inherits TrustedPrivateOnly { constructor() : TrustedPrivateOnly('init') {} }",
    "base-private-default-ctor": "class T inherits TrustedPrivateOnly { constructor() : TrustedPrivateOnly() {} }",
    "base-public-ctor": "class T inherits Trusted { constructor() : Trusted() {} }",
    "public-variant": "class T inherits Trusted { string f() { return op(); } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

# the dynamic forms, refused at run time, and the allowed uses
const Tenant = "
class T inherits Trusted {
    constructor() : Trusted() {
    }

    static auto variantAuto(auto h) { return h.op(1); }
    static auto variantCallObjectMethod(auto h) { return call_object_method(h, 'op', 1); }
    static auto variantRef(Trusted h) { code c = \\h.op(); return c(1); }
    static auto alias(auto x) { return x; }
    auto variantSelfAuto() { auto o = alias(self); return o.op(1); }
    static auto createObject() { return create_object('Trusted', 'x').how; }
    static auto createObjectArgs() { return create_object_args('Trusted', ('x',)).how; }
    # the allowed uses
    auto publicVariant() { auto o = alias(self); return (op(), o.op()); }
    auto initialiser() { return how; }
    static auto newPublic() { return new Trusted().how; }
}

# the private default constructor of the base class, called without a base class constructor call: refused at run time
class TImplicit inherits TrustedPrivateOnly {
    constructor() {}
    static auto make() { return new TImplicit().how; }
}
class TNoConstructor inherits TrustedPrivateOnly {
    static auto make() { return new TNoConstructor().how; }
    static auto createObject() { return create_object('TNoConstructor').how; }
}

auto sub run(string m, *list<auto> args) {
    return call_static_method_args('T', m, args);
}

auto sub runClass(string cls, string m) {
    return call_static_method(cls, m);
}

auto sub runSelf(string m) {
    return call_object_method(new T(), m);
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

Trusted shared();
foreach string m in (("variantAuto", "variantCallObjectMethod", "variantRef")) {
    show(m, auto sub () { return p.callFunction("run", m, (shared,)); });
}
foreach string m in (("createObject", "createObjectArgs", "newPublic")) {
    show(m, auto sub () { return p.callFunction("run", m); });
}
foreach string m in (("variantSelfAuto", "publicVariant", "initialiser")) {
    show(m, auto sub () { return p.callFunction("runSelf", m); });
}
show("implicit-private-default-ctor", auto sub () { return p.callFunction("runClass", "TImplicit", "make"); });
show("no-ctor-private-default-ctor", auto sub () { return p.callFunction("runClass", "TNoConstructor", "make"); });
show("no-ctor-private-default-ctor-create-object", auto sub () {
    return p.callFunction("runClass", "TNoConstructor", "createObject");
});
# a base class constructor call whose arguments are resolved at run time: the private constructor is refused then
if (*Program rp = sandboxed("base-private-ctor-runtime", "class TR inherits TrustedPrivateOnly {
    constructor(auto x) : TrustedPrivateOnly(x) {}
}
auto sub make() { return new TR('x').how; }")) {
    show("base-private-ctor-runtime", auto sub () { return rp.callFunction("make"); });
}
# trusted code calls the private variant and constructor of its class
show("trusted", auto sub () { return (shared.op(), Trusted::viaPrivate()); });
