#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# an abstract method that a class reaches only through private inheritance does not make the method that implements it
# callable from outside the class hierarchy: the declaration has at least the access of the least restrictive path
# to its class (qore#7288); the test compares the output of each execution mode and of qcc executables with
# probe.expected

%modern
%no-child-restrictions
%disable-warning non-existent-method-call

#! runs a call three times (a call site resolved at run time caches its method), printing its value or its error
sub check(string label, code c) {
    list<string> l = ();
    for (int i = 0; i < 3; ++i) {
        try {
            l += sprintf("%y", c());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err;
        }
    }
    # printed once when every call gives the same outcome
    printf("%s: %s\n", label, (select l, $1 != l[0]) ? l.join(" | ") : l[0]);
}

class Secret {
    string m() { return "Secret::m"; }
}
class SecretPriv {
    private string m() { return "SecretPriv::m"; }
}
class Iface {
    abstract string m();
    static string u(auto o) { return o.m(); }
}
class IfacePriv {
    private abstract string m();
}

# 1: the declaration and the implementation are both reached through private inheritance: private
class X1 inherits private Secret, private Iface {
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
    static string c(auto o) { return call_object_method(o, "m"); }
    static string t(X1 o) { return o.m(); }
    static code ref(X1 o) { return \o.m(); }
    static string viaIface(auto o) { return Iface::u(o); }
}
class Y1 inherits X1 {
    static string u(auto o) { return o.m(); }
}
# the control: without the declaration
class K1 inherits private Secret {
    static string u(auto o) { return o.m(); }
    static string c(auto o) { return call_object_method(o, "m"); }
}

# 2: the declaration is reached publicly: public, as the class declares it
class X2 inherits private Secret, Iface {
    static string u(auto o) { return o.m(); }
}

# 3: the implementation is reached publicly: public, as without the declaration
class X3 inherits Secret, private Iface {
    static string u(auto o) { return o.m(); }
}

# 4: private:internal edges to both: only the class's code
class X4 inherits private:internal Secret, private:internal Iface {
    static string u(auto o) { return o.m(); }
}
class Y4 inherits X4 {
    static string u(auto o) { return o.m(); }
}
class K4 inherits private:internal Secret {
    static string u(auto o) { return o.m(); }
}
class L4 inherits K4 {
    static string u(auto o) { return o.m(); }
}

# 5: a private edge to the implementation and a private:internal edge to the declaration
class X5 inherits private Secret, private:internal Iface {
    static string u(auto o) { return o.m(); }
}
class Y5 inherits X5 {
    static string u(auto o) { return o.m(); }
}

# 6: a private:internal edge to the implementation and a private edge to the declaration
class X6 inherits private:internal Secret, private Iface {
    static string u(auto o) { return o.m(); }
}
class Y6 inherits X6 {
    static string u(auto o) { return o.m(); }
}

# 7: the declaration through a private edge to a class that inherits it publicly
class Mid inherits Iface {
    static string u(auto o) { return o.m(); }
}
class X7 inherits private Mid, private Secret {
    static string u(auto o) { return o.m(); }
}
# 8: the same with a public edge to the intermediate class: public
class X8 inherits Mid, private Secret {
    static string u(auto o) { return o.m(); }
}
# 9: a public edge to a class that inherits the declaration privately
class MidP inherits private Iface {
    static string u(auto o) { return o.m(); }
}
class X9 inherits MidP, private Secret {
    static string u(auto o) { return o.m(); }
}
# 10: a public edge to a class that inherits the declaration with private:internal
class MidI inherits private:internal Iface {
    static string u(auto o) { return o.m(); }
}
class X10 inherits MidI, private Secret {
    static string u(auto o) { return o.m(); }
}
# 11: the declaration reached privately and publicly: public (the least restrictive path)
class X11 inherits private Iface, Mid, private Secret {
    static string u(auto o) { return o.m(); }
}

# 12: overloads: one declared through a private edge, one through a public edge
class Secret2 {
    string m(int i) { return "Secret2::m(int)"; }
    string m(string s) { return "Secret2::m(string)"; }
}
class IfaceInt {
    abstract string m(int i);
}
class IfaceStr {
    abstract string m(string s);
}
class X12 inherits private Secret2, IfaceStr, private IfaceInt {
    static string ui(auto o) { return o.m(1); }
    static string us(auto o) { return o.m("a"); }
}
class X13 inherits private Secret2, private IfaceInt, private IfaceStr {
    static string ui(auto o) { return o.m(1); }
}
# the class declares the public overload itself
class X18 inherits private Secret2, private IfaceInt {
    abstract string m(string s);
    static string ui(auto o) { return o.m(1); }
}

# 14: a subclass that declares the abstract method again: the access of the implementing variant in X1 (#7277)
class Z14 inherits X1 {
    abstract string m();
    static string u(auto o) { return o.m(); }
}

# 15: a private implementation and a private declaration: private, as before
class X15 inherits private SecretPriv, private Iface {
    static string u(auto o) { return o.m(); }
}
class X16 inherits private Secret, private IfacePriv {
    static string u(auto o) { return o.m(); }
}

# 17: the class declares the abstract method itself: public (#7277)
class X17 inherits private Secret, private Iface {
    abstract string m();
}

# 19: the same signature declared through a private and a public edge, in every order: public (the least
# restrictive declaration, whichever the class keeps)
class IfaceB {
    abstract string m();
}
class A1 inherits private Secret, private Iface, IfaceB {}
class A2 inherits private Secret, IfaceB, private Iface {}
class A3 inherits private Iface, IfaceB, private Secret {}
class A4 inherits IfaceB, private Iface, private Secret {}
# both private: private
class A5 inherits private Secret, private Iface, private IfaceB {}

# 20: a const declaration through a private edge, which the class keeps instead of a public non-const one
class IfaceC {
    abstract string m() const;
}
class SecretC {
    string m() const { return "SecretC::m"; }
}
class XC1 inherits private SecretC, IfaceB, private IfaceC {}
class XC2 inherits private SecretC, private IfaceC, IfaceB {}
class XC3 inherits private SecretC, private IfaceC, private IfaceB {}

# 21: the class declares the method itself as private: its declaration overrides the public one of its parent class
class X21 inherits private Secret, Iface {
    private abstract string m();
    static string u(auto o) { return o.m(); }
}

string sub u(auto o) { return o.m(); }
string sub r(auto o) { code c = \o.m(); return c(); }
string sub c(auto o) { return call_object_method(o, "m"); }

X1 x1();
check("1 outside", string sub () { return u(x1); });
check("1 outside ref", string sub () { return r(x1); });
check("1 outside call_object_method", string sub () { return c(x1); });
check("1 class", string sub () { return X1::u(x1); });
check("1 class typed", string sub () { return X1::t(x1); });
check("1 class ref", string sub () { return X1::r(x1); });
check("1 class ref called outside", string sub () { return X1::ref(x1)(); });
check("1 class call_object_method", string sub () { return X1::c(x1); });
check("1 declaring class", string sub () { return X1::viaIface(x1); });
Y1 y1();
check("1 subclass object outside", string sub () { return u(y1); });
check("1 subclass", string sub () { return Y1::u(y1); });
K1 k1();
check("1 control outside", string sub () { return u(k1); });
check("1 control class", string sub () { return K1::u(k1); });
check("1 control class call_object_method", string sub () { return K1::c(k1); });

check("2 outside", string sub () { return u(new X2()); });
check("3 outside", string sub () { return u(new X3()); });

X4 x4();
check("4 outside", string sub () { return u(x4); });
check("4 class", string sub () { return X4::u(x4); });
Y4 y4();
check("4 subclass object outside", string sub () { return u(y4); });
check("4 subclass", string sub () { return Y4::u(y4); });
check("4 control outside", string sub () { return u(new K4()); });
check("4 control class", string sub () { return K4::u(new K4()); });
check("4 control subclass", string sub () { return L4::u(new L4()); });

X5 x5();
check("5 outside", string sub () { return u(x5); });
check("5 class", string sub () { return X5::u(x5); });
check("5 subclass", string sub () { return Y5::u(new Y5()); });
X6 x6();
check("6 outside", string sub () { return u(x6); });
check("6 class", string sub () { return X6::u(x6); });
check("6 subclass", string sub () { return Y6::u(new Y6()); });

X7 x7();
check("7 outside", string sub () { return u(x7); });
check("7 class", string sub () { return X7::u(x7); });
check("8 outside", string sub () { return u(new X8()); });
X9 x9();
check("9 outside", string sub () { return u(x9); });
check("9 class", string sub () { return X9::u(x9); });
X10 x10();
check("10 outside", string sub () { return u(x10); });
check("10 class", string sub () { return X10::u(x10); });
check("11 outside", string sub () { return u(new X11()); });

X12 x12();
check("12 private overload outside", string sub () { auto o = x12; return o.m(1); });
check("12 public overload outside", string sub () { auto o = x12; return o.m("a"); });
check("12 private overload class", string sub () { return X12::ui(x12); });
check("12 public overload class", string sub () { return X12::us(x12); });
check("13 outside", string sub () { auto o = new X13(); return o.m(1); });
check("13 class", string sub () { return X13::ui(new X13()); });
X18 x18();
check("18 private overload outside", string sub () { auto o = x18; return o.m(1); });
check("18 public overload outside", string sub () { auto o = x18; return o.m("a"); });
check("18 private overload class", string sub () { return X18::ui(x18); });

Z14 z14();
check("14 outside", string sub () { return u(z14); });
check("14 class", string sub () { return Z14::u(z14); });

check("15 outside", string sub () { return u(new X15()); });
check("15 class", string sub () { return X15::u(new X15()); });
check("16 outside", string sub () { return u(new X16()); });
check("16 class", string sub () { return X16::u(new X16()); });
check("17 outside", string sub () { return u(new X17()); });

foreach string cn in ("A1", "A2", "A3", "A4", "A5", "XC1", "XC2", "XC3") {
    object o = create_object(cn);
    check("19/20 " + cn + " outside", string sub () { return u(o); });
    check("19/20 " + cn + " outside ref", string sub () { return r(o); });
    check("19/20 " + cn + " outside call_object_method", string sub () { return c(o); });
}
check("21 outside", string sub () { return u(new X21()); });
check("21 class", string sub () { return X21::u(new X21()); });
