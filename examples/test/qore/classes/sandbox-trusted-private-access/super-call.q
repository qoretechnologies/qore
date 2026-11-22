#!/usr/bin/env qore
# a class that sandboxed code declares can call the version of a base class of a method only in its method that
# overrides it (with the parameter types of a variant of the base class's method), not from another method, not from
# a method of the name with other parameters and not through a call reference (qore#6843); run by
# ../sandbox-trusted-private-access.qtest in every execution mode

%modern

#! a class that trusted code declares
class Trusted {
    private string privOp() {
        return "trusted";
    }

    string callPrivOp() {
        return privOp();
    }
}

const Opts = PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES | PO_STRONG_ENCAPSULATION;

#! returns a new sandboxed Program with the code parsed, or the parse error
*Program sub sandboxed(string label, string code) {
    Program p(Opts);
    p.importClass("Trusted");
    p.setSandboxManager(new SandboxManager());
    try {
        p.parse(code, label);
        return p;
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: parse: %s: %s\n", label, ex.err, ex.desc);
    }
}

const ParseCases = (
    # a method of the name with other parameters does not override the method: a decoy
    "decoy-from-other-method": "class T inherits Trusted {
        private string privOp(int x) { return ''; }
        string f() { return Trusted::privOp(); } }",
    "decoy-from-itself": "class T inherits Trusted {
        private string privOp(int x) { return Trusted::privOp(); } }",
    # an override, but the call is in another method
    "override-from-other-method": "class T inherits Trusted {
        private string privOp() { return 'tenant'; }
        string f() { return Trusted::privOp(); } }",
    # a call reference to the base class's version, also in the override
    "reference-in-override": "class T inherits Trusted {
        private string privOp() { code c = \\Trusted::privOp(); return c(); } }",
    # the override's call of the base class's version: allowed
    "override": "class T inherits Trusted {
        private string privOp() { return 'tenant+' + Trusted::privOp(); } }",
);

foreach hash<auto> i in (ParseCases.pairIterator()) {
    if (sandboxed(i.key, i.value)) {
        printf("%s: parse: OK\n", i.key);
    }
}

*Program p = sandboxed("tenant", "class T inherits Trusted {
    private string privOp() { return 'tenant+' + Trusted::privOp(); }
}
string sub run() { return new T().callPrivOp(); }
");
if (p) {
    printf("override called by the base class: %y\n", p.callFunction("run"));
}
