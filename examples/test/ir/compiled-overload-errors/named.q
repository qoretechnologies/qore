%modern
# named arguments to overloaded targets: the arguments are bound to the parameter names of a variant at parse time,
# and a variant resolved at run time (for a call without a committed variant, or again for a committed variant that
# an argument without a value does not match) is one with those parameter names at the positions of the arguments;
# see compiled-overload-errors.qtest and QoreFunction::parseFindVariantNamed()
class C {
    public { int mi; int m = 1; }
    reference<int> get() { return \m; }
}

# variants with the same parameter names
string sub nf(int a, string b = "d") { return sprintf("nf(int a, string b) a=%y b=%y", a, b); }
string sub nf(*string a, string b = "d") { return sprintf("nf(*string a, string b) a=%y b=%y", a, b); }

# a parameter skipped by the call
string sub sk(int a, *int b, string c) { return sprintf("sk(int a, *int b, string c) a=%y b=%y c=%y", a, b, c); }
string sub sk(*string a, *int b, string c) { return sprintf("sk(*string a, *int b, string c) a=%y b=%y c=%y", a, b,
    c); }

# a reference never matches int or string: resolved at run time, it raises the overload error
string sub rf(int a) { return "rf(int)"; }
string sub rf(string a) { return "rf(string)"; }

class M {
    string m(int a, string b) { return sprintf("M::m(int a, string b) a=%y b=%y", a, b); }
    string m(*string a, string b) { return sprintf("M::m(*string a, string b) a=%y b=%y", a, b); }
}

class K {
    public { string v; }
    constructor(int a, string b) { v = sprintf("K(int a, string b) a=%y b=%y", a, b); }
    constructor(*string a, string b) { v = sprintf("K(*string a, string b) a=%y b=%y", a, b); }
}

sub run(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

C c();
M o();
run("literal", string sub () { return nf(b: "x", a: 1); });
run("assigned member", string sub () { return nf(b: "x", a: c.m); });
run("unassigned member", string sub () { return nf(b: "x", a: c.mi); });
run("default", string sub () { return nf(a: c.mi); });
run("skipped", string sub () { return sk(c: "z", a: c.mi); });
run("skipped assigned", string sub () { return sk(c: "z", a: c.m); });
run("reference", string sub () { return rf(a: c.get()); });
run("method", string sub () { return o.m(b: "y", a: c.mi); });
run("method assigned", string sub () { return o.m(b: "y", a: c.m); });
run("constructor", string sub () { return (new K(b: "y", a: c.mi)).v; });
run("constructor assigned", string sub () { return (new K(b: "y", a: c.m)).v; });
