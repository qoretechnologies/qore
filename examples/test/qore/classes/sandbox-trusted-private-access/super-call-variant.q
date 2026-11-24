#!/usr/bin/env qore
# the call of the version of a base class of a method by its override, in a class that sandboxed code declares, can
# select only the variant that the override overrides, on the override's own object, and only in its own variant
# search: not another private variant of the method, and not a private variant selected by a call in its arguments
# or in the code that it runs (qore#6843); the class's own call of the constructor of its base class cannot select a
# private constructor nor a private variant in its arguments; run by ../sandbox-trusted-private-access.qtest in every
# execution mode

%modern

#! a class that trusted code declares
class Trusted {
    public {
        string how = "public constructor";
    }

    constructor() {
    }

    private constructor(string why) {
        how = "private constructor: " + why;
    }

    private string hook() {
        return "hook()";
    }

    private string hook(int i) {
        return "private hook(int) " + i;
    }

    string op(string s) {
        return "op(string) " + s;
    }

    private string op(int i) {
        return "private op(int) " + i;
    }

    string callHook() {
        return hook();
    }
}

#! another object of the trusted class, which the tenant's code gets
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

#! parses the code and runs its run() function
sub run(string label, string code) {
    *Program p = sandboxed(label, code);
    if (!p) {
        return;
    }
    try {
        printf("%s: OK %y\n", label, p.callFunction("run"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

# another private variant of the overridden private method: refused
run("other-variant-of-private-override", "class T inherits Trusted {
    private string hook() { return Trusted::hook(42); } }
auto sub run() { return new T().callHook(); }");

# a private variant of a method whose public variant is overridden: refused
run("private-variant-from-public-override", "class T inherits Trusted {
    string op(string s) { return Trusted::op(7); } }
auto sub run() { return new T().op('x'); }");

# a private variant on another object of the class, in the arguments of the call: refused
run("private-variant-in-arguments", "class T inherits Trusted {
    string op(string s) { Trusted o = Holder::o; return Trusted::op(o.op(99)); } }
auto sub run() { return new T().op('x'); }");

# a private variant called dynamically in the arguments of the call: refused
run("dynamic-private-variant-in-arguments", "class T inherits Trusted {
    string op(string s) { return Trusted::op(T::leak()); }
    static string leak() { return call_object_method(Holder::o, 'op', 5); } }
auto sub run() { return new T().op('x'); }");

# the private variant of the overridden method on another object, dynamically, in the arguments: refused
run("overridden-variant-on-other-object", "class T inherits Trusted {
    private string hook() { return Trusted::hook() + T::leak(); }
    static string leak() { return call_object_method(Holder::o, 'hook'); } }
auto sub run() { return new T().callHook(); }");

# a private variant in the arguments of the class's own base class constructor call: refused
run("private-variant-in-constructor-arguments", "class T inherits Trusted {
    constructor(Trusted o) : Trusted(o.op(11)) {} }
auto sub run() { return new T(Holder::o).how; }");

# the overridden variant: allowed
run("overridden-private-variant", "class T inherits Trusted {
    private string hook() { return 'tenant+' + Trusted::hook(); } }
auto sub run() { return new T().callHook(); }");

# the overridden public variant: allowed
run("overridden-public-variant", "class T inherits Trusted {
    string op(string s) { return 'tenant+' + Trusted::op(s); } }
auto sub run() { return new T().op('x'); }");

# a base class version's call in the arguments of another: each selects its own overridden variant
run("nested-overridden-variant", "class T inherits Trusted {
    string op(string s) { return Trusted::op(Trusted::op(s)); } }
auto sub run() { return new T().op('x'); }");

# a private constructor of the base class in the class's own base class constructor call: refused, as the
# private constructors of a class that trusted code declared are private:internal for the class
run("private-constructor-in-initializer", "class T inherits Trusted {
    constructor() : Trusted('initializer') {} }
auto sub run() { return new T().how; }");
