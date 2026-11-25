#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# classes that inherit abstract overloads of one method from several parent classes; abstract-overload-parents.qtest
# runs it in every execution mode and as qcc executables and compares the output with probe.expected

%modern
%disable-warning non-existent-method-call

# implementations
class Secret2 {
    string m(int i) { return "Secret2::m(int)"; }
    string m(string s) { return "Secret2::m(string)"; }
}
class Secret3 inherits Secret2 {
    string m(bool b) { return "Secret3::m(bool)"; }
}
class SInt { string m(int i) { return "SInt::m(int)"; } }
class SStr { string m(string s) { return "SStr::m(string)"; } }
class SBool { string m(bool b) { return "SBool::m(bool)"; } }
class A { string m() { return "A::m()"; } }
class Pub { string m() { return "Pub::m()"; } }
class PubC { string m() const { return "PubC::m() const"; } }

# abstract declarations
class IfaceInt { abstract string m(int i); }
class IfaceStr { abstract string m(string s); }
class IfaceBool { abstract string m(bool b); }
class Iface { abstract string m(); }
class IfaceB { abstract string m(); }
class IfaceC { abstract string m() const; }

# a third parent class implements the overloads of two abstract parent classes (qore#7313)
class X12 inherits Secret2, IfaceInt, IfaceStr {}
class X21 inherits Secret2, IfaceStr, IfaceInt {}
class X12a inherits IfaceInt, IfaceStr, Secret2 {}
class XP12 inherits private Secret2, IfaceInt, IfaceStr {}
class XP21 inherits private Secret2, IfaceStr, IfaceInt {}
class XH12 inherits private:hierarchy Secret2, IfaceInt, IfaceStr {}
class XI12 inherits private:internal Secret2, IfaceInt, IfaceStr {}
class XI21 inherits private:internal Secret2, IfaceStr, IfaceInt {}
# an abstract declaration reached through private inheritance does not make the implementation public
class XPS12 inherits Secret2, IfaceInt, private IfaceStr {}
class XPS21 inherits Secret2, private IfaceStr, IfaceInt {}
class XPP12 inherits private Secret2, private IfaceInt, private IfaceStr {}
class XPPS12 inherits private Secret2, IfaceInt, private IfaceStr { string via(auto a) { return m(a); } }
class XPPS21 inherits private Secret2, private IfaceStr, IfaceInt { string via(auto a) { return m(a); } }
class XIPS12 inherits private:internal Secret2, IfaceInt, private IfaceStr { string via(auto a) { return m(a); } }

# three abstract parent classes
class X123 inherits Secret3, IfaceInt, IfaceStr, IfaceBool {}
class X321 inherits Secret3, IfaceBool, IfaceStr, IfaceInt {}
class X231 inherits IfaceStr, IfaceBool, IfaceInt, Secret3 {}
class XP123 inherits private Secret3, IfaceInt, IfaceStr, IfaceBool {}
# one overload is not implemented: the class is abstract
class Y123 inherits Secret2, IfaceInt, IfaceStr, IfaceBool {}
class Y321 inherits Secret2, IfaceBool, IfaceStr, IfaceInt {}
class YP123 inherits private Secret2, IfaceInt, IfaceStr, IfaceBool {}

# the overloads are implemented by different parent classes
class D12 inherits SInt, SStr, IfaceInt, IfaceStr {}
class D21 inherits IfaceStr, IfaceInt, SStr, SInt {}
class D123 inherits SBool, IfaceInt, SInt, IfaceStr, IfaceBool, SStr {}
class DP12 inherits private SInt, private SStr, IfaceInt, IfaceStr {}
class DI12 inherits private:internal SInt, SStr, IfaceStr, IfaceInt {}
class E12 inherits SInt, IfaceInt, IfaceStr {}
class E21 inherits SInt, IfaceStr, IfaceInt {}

# the abstract overload of a later parent class remains when a parent class resolves another overload (qore#7313)
class B inherits A, Iface {}
class C inherits B, IfaceInt {}
class C2 inherits IfaceInt, B {}
class BP inherits private A, Iface {}
class CP inherits BP, IfaceInt {}
class C3 inherits B, IfaceInt, SInt {}
class C4 inherits SInt, IfaceInt, B {}
class CI3 inherits B, IfaceInt, private:internal SInt {}

# a class's own abstract overload and that of its parent class
class P { abstract string m(int i); }
class Q inherits P { abstract string m(string s); }
class R inherits Q { string m(string s) { return "R::m(string)"; } }
class R2 inherits Q {
    string m(string s) { return "R2::m(string)"; }
    string m(int i) { return "R2::m(int)"; }
}
class R3 inherits Q, SInt { string m(string s) { return "R3::m(string)"; } }
class Q2 inherits IfaceInt { abstract string m(string s); }
class R4 inherits Secret2, Q2 {}
class R5 inherits Q2, private Secret2 {}

# the const contract of an abstract declaration holds whatever the order of the parent classes (qore#7317)
class H5 inherits Pub, IfaceB, IfaceC {}
class H5r inherits Pub, IfaceC, IfaceB {}
class H6 inherits Pub, private IfaceB, private IfaceC {}
class H6r inherits Pub, private IfaceC, private IfaceB {}
class HC5 inherits PubC, IfaceB, IfaceC {}
class HC5r inherits PubC, IfaceC, IfaceB {}
class HL5 inherits IfaceInt, IfaceB, IfaceC {
    string m() const { return "HL5::m() const"; }
    string m(int i) { return "HL5::m(int)"; }
}
class HL5c inherits IfaceB, IfaceC { string m() const { return "HL5c::m() const"; } }
class HI5 inherits IfaceInt, IfaceB, IfaceC, Pub, SInt {}
class HI5r inherits IfaceC, IfaceInt, IfaceB, SInt, PubC {}

# a parent class that implements an inherited abstract variant itself and another parent class that inherits it
# unimplemented: the class's own code calls the implementation (found by the abstract overload fuzzer)
class K0 { abstract private string m(int i); }
class K1 inherits K0 {
    abstract string m();
    string m(int i) { return "K1::m(int)"; }
}
class K1b inherits K1 { string m() { return "K1b::m()"; } }
class K2 inherits private:hierarchy K1, K0 {
    string m() { return "K2::m()"; }
    string via(auto a) { auto s = self; return s.m(a); }
}
class K4 inherits private:internal K2, private:hierarchy K1, private:hierarchy K0 {
    string via(auto a) { auto s = self; return s.m(a); }
}
class K5 inherits K2, private:hierarchy K1, private:hierarchy K0 {
    string via(auto a) { auto s = self; return s.m(a); }
}
class K7 inherits private:internal K2, private:hierarchy K1 {
    string via(auto a) { auto s = self; return s.m(a); }
}

# calls a variant of m() on an object of the class from outside its hierarchy
string sub call(object o, auto arg) {
    try {
        if (!exists arg) {
            return o.m();
        }
        return o.m(arg);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub cmcall(object o, auto arg) {
    try {
        return exists arg ? call_object_method(o, "m", arg) : call_object_method(o, "m");
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

const Args = (NOTHING, 1, "a", True);

foreach string cn in ("X12", "X21", "X12a", "XP12", "XP21", "XH12", "XI12", "XI21", "XPS12", "XPS21", "XPP12",
        "XPPS12", "XPPS21", "XIPS12", "X123", "X321", "X231", "XP123", "Y123", "Y321", "YP123", "D12", "D21", "D123",
        "DP12", "DI12", "E12", "E21", "B", "C", "C2", "BP", "CP", "C3", "C4", "CI3", "Q", "R", "R2", "R3", "Q2", "R4",
        "R5", "H5", "H5r", "H6", "H6r", "HC5", "HC5r", "HL5", "HL5c", "HI5", "HI5r", "K1b", "K2", "K4", "K5", "K7") {
    object o;
    try {
        o = create_object(cn);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", cn, ex.err);
        continue;
    }
    list<string> l = map sprintf("m(%s): %s / %s", $1.type(), call(o, $1), cmcall(o, $1)), Args;
    printf("%s: %s\n", cn, l.join("; "));
}

# typed calls, resolved at parse time with the methods that the first parent class with the method has
sub typed() {
    X12 x12();
    X21 x21();
    X321 x321();
    printf("typed: %s %s %s %s %s %s %s\n", x12.m(1), x12.m("a"), x21.m(1), x21.m("a"), x321.m(1), x321.m("a"),
        x321.m(True));
}
typed();

# the variant that a private abstract declaration makes private is called from the class's own code
foreach string cn in ("XPPS12", "XPPS21", "XIPS12") {
    object o = create_object(cn);
    printf("%s via: %s %s\n", cn, o.via(1), o.via("a"));
}
foreach string cn in ("K2", "K4", "K5", "K7") {
    object o = create_object(cn);
    printf("%s via: %s %s\n", cn, o.via(1), o.via(NOTHING));
}
