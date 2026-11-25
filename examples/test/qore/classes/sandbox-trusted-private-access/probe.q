#!/usr/bin/env qore
# the private access of the classes that sandboxed code declares to the declarations of the classes that trusted code
# declares (qore#6843); run by ../sandbox-trusted-private-access.qtest in every execution mode

%modern
%enable-all-warnings

#! a class that trusted code declares, with private members, methods and a static variable
class Trusted {
    public {
        string pub = "pub";
    }

    private {
        string secret = "secret";
        static string svar = "svar";
    }

    private {
        const PC = "pc";
    }

    const PUBC = "pubc";

    constructor() {
    }

    private string helper() {
        return "helper";
    }

    private static string shelper() {
        return "shelper";
    }

    private string hook() {
        return "trusted-hook";
    }

    string callHook() {
        return hook();
    }

    string readSecret() {
        return secret;
    }

    #! trusted code reads the private member of any object of the class, also of a subclass that sandboxed code
    #! declares
    static string readSecretOf(Trusted t) {
        return t.secret;
    }
}

#! a class that trusted code declares with a private abstract method, which its subclasses implement
class TrustedAbstract {
    abstract private string impl();

    string callImpl() {
        return impl();
    }
}

#! a subclass that trusted code declares: it uses the private declarations of its base class on any object
class TrustedSub inherits Trusted {
    static string peek(Trusted t) {
        return t.secret + ":" + t.helper() + ":" + Trusted::shelper() + ":" + Trusted::svar + ":" + PC;
    }
}

# an object shared with the sandboxed code
Trusted shared();

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.importClass("Trusted");
    p.importClass("TrustedAbstract");
    p.loadModule("reflection");
    p.setSandboxManager(new SandboxManager());
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        # the description names the classes and the declaration, not the location
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

# the static forms: refused at parse time
const ParseCases = (
    "member-typed": "class T inherits Trusted {
        static string f(Trusted h) { return h.secret; } }",
    "member-write-typed": "class T inherits Trusted {
        static f(Trusted h) { h.secret = \"x\"; } }",
    "member-bare": "class T inherits Trusted {
        string f() { return secret; } }",
    "member-other-instance": "class T inherits Trusted {
        static string f(T o) { return o.secret; } }",
    "member-own-other-instance": "class T inherits Trusted { private { string own = \"own\"; }
        static string f(T o) { return o.own; } }",
    "member-self": "class T inherits Trusted {
        string f() { return self.secret; } }",
    "method-typed": "class T inherits Trusted {
        static string f(Trusted h) { return h.helper(); } }",
    "method-self": "class T inherits Trusted {
        string f() { return helper(); } }",
    "method-self-dot": "class T inherits Trusted {
        string f() { return self.helper(); } }",
    "method-base-not-overridden": "class T inherits Trusted {
        string f() { return Trusted::helper(); } }",
    "static-method": "class T inherits Trusted {
        static string f() { return Trusted::shelper(); } }",
    "static-var": "class T inherits Trusted {
        static string f() { return Trusted::svar; } }",
    "constant-bare": "class T inherits Trusted {
        static string f() { return PC; } }",
    "constant-scoped": "class T inherits Trusted {
        static string f() { return Trusted::PC; } }",
    "constant-public": "class T inherits Trusted {
        static string f() { return PUBC + Trusted::PUBC; } }",
    "method-ref-typed": "class T inherits Trusted {
        static code f(Trusted h) { return \\h.helper(); } }",
    "method-ref-self": "class T inherits Trusted {
        code f() { return \\helper(); } }",
    "static-method-ref": "class T inherits Trusted {
        static code f() { return \\Trusted::shelper(); } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    *Program p = sandboxed(i.key, i.value);
    if (p) {
        printf("%s: parse: OK\n", i.key);
    }
}

# the dynamic forms, refused at run time, and the allowed uses
string tenant = "
class T inherits Trusted, TrustedAbstract {
    private {
        string own = \"own\";
    }

    private:internal {
        string iown = \"iown\";
    }

    private string impl() {
        return \"tenant-impl:\" + own;
    }

    # an override calls the version of the base class
    private string hook() {
        return \"tenant-hook+\" + Trusted::hook();
    }

    # an object of a class that trusted code declared
    static auto memberAuto(auto h) { return h.secret; }
    static auto memberWriteAuto(auto h) { h.secret = \"changed\"; return h.readSecret(); }
    static auto memberComputed(auto h) { string n = \"sec\" + \"ret\"; return h.(n); }
    static auto methodAuto(auto h) { return h.helper(); }
    static auto callObjectMethod(auto h) { return call_object_method(h, \"helper\"); }
    static auto callObjectMethodArgs(auto h) { return call_object_method_args(h, \"helper\"); }
    static auto closureMember(auto h) { code c = auto sub () { return h.secret; }; return c(); }
    static auto hashOf(auto h) { return keys ({} + h); }
    static auto keysOf(auto h) { return keys h; }

    # a value without a type: the class is not known when parsing
    static auto alias(auto x) { return x; }

    # the object of the code itself
    auto selfAuto() { auto o = alias(self); return o.secret; }
    auto selfComputed() { string n = \"sec\" + \"ret\"; return self.(n); }
    auto selfWriteAuto() { auto o = alias(self); o.secret = \"changed\"; return readSecret(); }
    auto selfHash() { return keys ({} + self); }
    auto selfSlice() { list<string> l = (\"secret\", \"pub\", \"own\"); return keys self.(l); }
    auto selfIterate() { list<string> l; auto o = alias(self); foreach string k in (keys o) { l += k; } return l; }
    auto selfFirstLast() { auto o = alias(self); return (o.firstKey(), o.lastKey()); }
    auto selfMethodAuto() { auto o = alias(self); return o.helper(); }
    auto selfCallObjectMethod() { return call_object_method(self, \"helper\"); }
    auto selfKeys() { return keys self; }
    auto selfReflectionMember() {
        return cast<Reflection::NormalMember>(Reflection::Class::forName(\"Trusted\").getMember(\"secret\"))
            .getValue(self);
    }
    auto selfReflectionMethod() {
        return cast<Reflection::NormalMethodVariant>(Reflection::Class::forName(\"Trusted\").getMethod(\"helper\")
            .getVariant()).call(self);
    }
    auto selfReflectionConstant() {
        return Reflection::Class::forName(\"Trusted\").getConstant(\"PC\").getValue();
    }
    auto selfReflectionPublicConstant() {
        return Reflection::Class::forName(\"Trusted\").getConstant(\"PUBC\").getValue();
    }
    auto selfClosure() { auto o = alias(self); code c = auto sub () { return o.secret; }; return c(); }

    # the allowed uses
    auto ownMember() { auto o = alias(self); string n = \"o\" + \"wn\"; return (self.own, o.own, self.(n)); }
    auto inheritedPublic() { auto o = alias(self); string n = \"p\" + \"ub\"; return (self.pub, o.pub, self.(n)); }
    auto ownCallImpl() { return callImpl(); }
    auto ownCallHook() { return callHook(); }

    # every combination of the inline member check (see qore_class_private::runtimeCheckMemberAccess()): the class
    # of the object is this class (self or another instance) or a subclass of it; the member is declared private or
    # private:internal by this class, private by the trusted class, or public by the trusted class; each typed (parsed),
    # through a value without a type and with a computed name
    auto rowsOwnPrivate(T o) {
        auto a = alias(o); string n = \"o\" + \"wn\";
        return (o.own, a.own, a.(n));
    }
    auto rowsOwnInternal(T o) {
        auto a = alias(o); string n = \"i\" + \"own\";
        return (o.iown, a.iown, a.(n));
    }
    auto rowsInheritedPublic(T o) {
        auto a = alias(o); string n = \"p\" + \"ub\";
        return (o.pub, a.pub, a.(n));
    }
    static auto rowsInheritedPrivateAlias(auto a) { return a.secret; }
    static auto rowsInheritedPrivateComputed(auto a) { string n = \"sec\" + \"ret\"; return a.(n); }
}

class T2 inherits T {
}

auto sub run(string m, *list<auto> args) {
    return call_static_method_args(\"T\", m, args);
}

auto sub runSelf(string m) {
    return call_object_method(new T(), m);
}

T sub make() {
    return new T();
}

auto sub rows(string label, string m) {
    T t();
    switch (label) {
        case \"self\": return call_object_method(t, m, t);
        case \"other\": return call_object_method(t, m, new T());
        case \"sub\": return call_object_method(t, m, new T2());
    }
}

auto sub rowsStatic(string label, string m) {
    return call_static_method(\"T\", m, label == \"sub\" ? new T2() : new T());
}
";

*Program p = sandboxed("tenant", tenant);
if (!p) {
    exit(1);
}

sub show(string label, code c) {
    try {
        auto v = c();
        printf("%s: OK %y\n", label, v);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

foreach string m in (("memberAuto", "memberWriteAuto", "memberComputed", "methodAuto", "callObjectMethod",
        "callObjectMethodArgs", "closureMember", "hashOf", "keysOf")) {
    show(m, auto sub () { return p.callFunction("run", m, (shared,)); });
}
printf("shared secret after: %y\n", shared.readSecret());

foreach string m in (("selfAuto", "selfComputed", "selfWriteAuto", "selfHash", "selfSlice", "selfIterate",
        "selfFirstLast", "selfMethodAuto", "selfCallObjectMethod", "selfKeys", "selfReflectionMember",
        "selfReflectionMethod", "selfReflectionConstant", "selfReflectionPublicConstant", "selfClosure", "ownMember", "inheritedPublic",
        "ownCallImpl", "ownCallHook")) {
    show(m, auto sub () { return p.callFunction("runSelf", m); });
}

foreach string label in (("self", "other", "sub")) {
    foreach string m in (("rowsOwnPrivate", "rowsOwnInternal", "rowsInheritedPublic")) {
        show("rows " + label + " " + m, auto sub () { return p.callFunction("rows", label, m); });
    }
    foreach string m in (("rowsInheritedPrivateAlias", "rowsInheritedPrivateComputed")) {
        show("rows " + label + " " + m, auto sub () { return p.callFunction("rowsStatic", label, m); });
    }
}

# trusted code uses the private declarations of the class on the objects of the subclass that sandboxed code declared
object t = p.callFunction("make");
show("trusted-on-tenant", auto sub () { return (Trusted::readSecretOf(t), t.readSecret(), t.callImpl()); });
# a trusted subclass keeps its private access
show("trusted-sub", auto sub () { return TrustedSub::peek(shared); });
