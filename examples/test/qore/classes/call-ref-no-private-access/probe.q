#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a call reference made by code of a class without private access to the object's class refers to the method that a
# call from the same code calls (qore#7220); the test compares the output of each execution mode and of qcc
# executables with probe.expected

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

# the classes of the issue: Z is a private:internal parent class of B, so its code has no private access to X; X
# reaches R::m() first through its private:hierarchy parent P, and B::m() through its public parent B
class R {
    string m() { return "R::m"; }
}
class P inherits R {}
class Z {
    static string u(auto o) { return o.m(); }
    static string t(X o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
    static string rt(X o) { code c = \o.m(); return c(); }
    static string rn(auto o) { code c = \o.m(); return c(m: 1); }
    static string c(auto o) { return call_object_method(o, "m"); }
}
class B inherits P, private:internal Z {
    string m(*int m) { return "B::m"; }
    # B's code has private access to X: it calls R::m(), reached first
    string bu(auto o) { return o.m(); }
    string br(auto o) { code c = \o.m(); return c(); }
}
class X inherits private:hierarchy P, B {}

# a private override in the object's class, which code without private access cannot call
class P2 {
    string m() { return "P2::m"; }
}
class Z2 {
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
}
class Y2 inherits private:internal Z2 {}
class Q2 inherits Y2, P2 {}
class X2 inherits Q2 {
    private string m() { return "X2::m"; }
}

# code of a class without private access to the object's class (a sibling) selects the private variant of a common
# parent class's method that it can call, through a call and through a call reference alike
class PB {
    string m(int i) { return "PB::m(int)"; }
    private string m(string s) { return "PB::m(string)"; }
}
class PBS inherits PB {}
class SB inherits PB {
    static string t(PB b) { return b.m("x"); }
    static string u(auto b) { return b.m("x"); }
    static string r(PB b) { code c = \b.m(); return c("x"); }
    static string ru(auto b) { code c = \b.m(); return c("x"); }
    static string rn(auto b) { code c = \b.m(); return c(s: "x"); }
}

string sub outside(auto o) {
    return o.m();
}
string sub outsideRef(auto o) {
    code c = \o.m();
    return c();
}

X x();
check("outside untyped", string sub () { return outside(x); });
check("outside call reference", string sub () { return outsideRef(x); });
check("Z untyped", string sub () { return Z::u(x); });
check("Z typed", string sub () { return Z::t(x); });
check("Z call reference", string sub () { return Z::r(x); });
check("Z typed call reference", string sub () { return Z::rt(x); });
check("Z call reference, named argument", string sub () { return Z::rn(x); });
check("Z call_object_method", string sub () { return Z::c(x); });
check("B untyped", string sub () { return x.bu(x); });
check("B call reference", string sub () { return x.br(x); });

X2 x2();
check("Z2 untyped", string sub () { return Z2::u(x2); });
check("Z2 call reference", string sub () { return Z2::r(x2); });
check("outside X2 call reference", string sub () { return outsideRef(x2); });

PBS pbs();
check("SB typed", string sub () { return SB::t(pbs); });
check("SB untyped", string sub () { return SB::u(pbs); });
check("SB call reference", string sub () { return SB::r(pbs); });
check("SB untyped call reference", string sub () { return SB::ru(pbs); });
check("SB call reference, named argument", string sub () { return SB::rn(pbs); });
