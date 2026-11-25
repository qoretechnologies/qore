#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# the classes of a private:internal hierarchy in child Programs, which have copies of the parent Program's classes,
# and calls with types resolved when the code is parsed; private-internal-subclass-calls.qtest compares the output of
# every execution mode and of qcc executables with child.expected

%modern

public namespace PIS {
public class C5 {
    string m(int i) { return "C5::m(int)"; }
    static string s(int i) { return "C5::s(int)"; }
}
public class K inherits private:internal C5 {
    string kdyn(auto a) { object o = self; return o.m(a); }
    static string kstatic(string cn, auto a) { return call_static_method(cn, "s", a); }
}
public class L2 inherits K {
    string m(string s) { return "L2::m(string)"; }
    static string s(string x) { return "L2::s(string)"; }
    string dyn(auto a) { object o = self; return o.m(a); }
    static string sdyn(auto a) { return call_static_method("PIS::L2", "s", a); }
}
}

string sub trycall0(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# static methods: from the code of K, of its subclass and from outside
foreach auto arg in ((1, "a")) {
    printf("static %s: K: %s; L2: %s; outside: %s\n", arg.type(),
        trycall0(auto sub () { return PIS::K::kstatic("PIS::L2", arg); }),
        trycall0(auto sub () { return PIS::L2::sdyn(arg); }),
        trycall0(auto sub () { return call_static_method("PIS::L2", "s", arg); }));
}

const Subclass = "class L2c inherits PIS::K {
    string m(string s) { return \"L2c::m(string)\"; }
    string dyn(auto a) { object o = self; return o.m(a); }
}
";

string sub trycall(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

const Args = (1, "a");

# the copies of the parent Program's classes and a subclass of the copy of K
{
    Program p(PO_NEW_STYLE);
    p.parse(Subclass + "
string sub l2c_dyn(auto a) { return (new L2c()).dyn(a); }
string sub l2c_kdyn(auto a) { return (new L2c()).kdyn(a); }
string sub l2_dyn(auto a) { return (new PIS::L2()).dyn(a); }
string sub l2_kdyn(auto a) { return (new PIS::L2()).kdyn(a); }
string sub l2_outside(auto a) { object o = new PIS::L2(); return o.m(a); }
", "child");
    foreach string f in ("l2c_dyn", "l2c_kdyn", "l2_dyn", "l2_kdyn", "l2_outside") {
        list<string> rv = ();
        foreach auto arg in (Args) {
            rv += sprintf("%s: %s", arg.type(), trycall(auto sub () { return p.callFunction(f, arg); }));
        }
        printf("child %s: %s\n", f, rv.join("; "));
    }
}

# calls whose variant is selected when the code is parsed: the receiver's and the argument's types are known
foreach hash<auto> h in ((
        {"label": "typed subclass", "code": Subclass + "string sub t() { L2c o(); return o.m(1); }"},
        {"label": "typed subclass code", "code": "class L2t inherits PIS::K {
    string m(string s) { return \"L2t::m(string)\"; }
    string t() { L2t o = self; return o.m(1); }
}
string sub t() { return (new L2t()).t(); }"},
        {"label": "typed copy", "code": "string sub t() { PIS::L2 o(); return o.m(1); }"},
        {"label": "typed K code", "code": "class Kt inherits private:internal PIS::C5 {
    string t() { Kt o = self; return o.m(1); }
}
string sub t() { return (new Kt()).t(); }"},
        # the variant committed when parsing the code of the inheriting class is called on an object of a subclass
        # that has a method of the name
        {"label": "typed K code on subclass", "code": "class Kt inherits private:internal PIS::C5 {
    string t() { Kt o = self; return o.m(1); }
}
class Lt inherits Kt { string m(string s) { return \"Lt::m(string)\"; } }
string sub t() { return (new Lt()).t(); }"},
        # the receiver typed as the inheriting class in the code of a subclass
        {"label": "typed as K in subclass code", "code": "class Kt inherits private:internal PIS::C5 {}
class Lt inherits Kt {
    string m(string s) { return \"Lt::m(string)\"; }
    string t() { Kt o = self; return o.m(1); }
}
string sub t() { return (new Lt()).t(); }"},
        {"label": "typed as K outside", "code": "class Kt inherits private:internal PIS::C5 {}
string sub t() { Kt o(); return o.m(1); }"},
    )) {
    Program p(PO_NEW_STYLE);
    printf("%s: %s\n", h.label, trycall(auto sub () {
        p.parse(h.code, h.label);
        return p.callFunction("t");
    }));
}
