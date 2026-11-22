#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# an abstract method that a concrete method of a parent class implements has the more restrictive access of its
# declaration and of that method, also when the parent class is inherited with private or private:internal access, or
# when the method is a private method of another parent class, and the class has a method for it in qcc executables
# as in the interpreter (qore#7277); the test
# compares the output of each execution mode and of qcc executables with probe.expected

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

# the calls of each class's code: untyped, through a call reference and with call_object_method()
class Calls {
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
    static string c(auto o) { return call_object_method(o, "m"); }
}

# 1: a public method of a class inherited with private:internal implements the abstract method: it is public
class P1 {
    string m() { return "P1::m"; }
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
}
class A1 inherits private:internal P1 {
    abstract string m();
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
    static string t(A1 o) { return o.m(); }
}
class S1 inherits A1 {
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
}

# 2: the same with private (hierarchy) inheritance
class P2 {
    string m() { return "P2::m"; }
    static string u(auto o) { return o.m(); }
}
class A2 inherits private P2 {
    abstract string m();
    static string u(auto o) { return o.m(); }
}
class S2 inherits A2 {
    static string u(auto o) { return o.m(); }
}

# 3: a private method of a class inherited with private:internal implements the abstract method: the method is
# private, as in Q3 below, which implements it with a private method of its own
class P3 {
    private string m() { return "P3::m"; }
    static string u(auto o) { return o.m(); }
}
class A3 inherits private:internal P3 {
    abstract string m();
    static string u(auto o) { return o.m(); }
}
class S3 inherits A3 {
    static string u(auto o) { return o.m(); }
}
class Q3 inherits private:internal P3 {
    private string m() { return "P3::m"; }
    static string u(auto o) { return o.m(); }
}
class T3 inherits Q3 {
    static string u(auto o) { return o.m(); }
}

# 4: a private method of another parent class implements the abstract method: the code of a third parent class of the
# object can call it, as a private method of the object's class
class P4 {
    private string m() { return "P4::m"; }
    static string u(auto o) { return o.m(); }
}
class O4 {
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
}
class A4 inherits P4, O4 {
    abstract string m();
    static string u(auto o) { return o.m(); }
}

# 5: a private:internal method of a parent class or of another parent class implements the abstract method, but only
# the code of its own class can call it (qore#6158)
class P5 {
    private:internal string m() { return "P5::m"; }
    static string u(auto o) { return o.m(); }
}
class A5 inherits P5 {
    abstract string m();
    static string u(auto o) { return o.m(); }
}
class C5 inherits A5 {
    static string u(auto o) { return o.m(); }
}
class I5 {
    abstract string m();
    static string u(auto o) { return o.m(); }
}
class B5 inherits P5, I5 {
    static string u(auto o) { return o.m(); }
}

# 6: a private or private:internal abstract method that a public method of a class inherited with private or
# private:internal access implements keeps the access of its declaration (the more restrictive one); Q6 is the
# control, which implements it with a private method of its own
class P6 {
    string m() { return "P6::m"; }
    static string u(auto o) { return o.m(); }
}
class A6 inherits private P6 {
    private abstract string m();
    static string u(auto o) { return o.m(); }
}
class S6 inherits A6 {
    static string u(auto o) { return o.m(); }
    static string r(auto o) { code c = \o.m(); return c(); }
}
class B6 inherits private:internal P6 {
    private abstract string m();
    static string u(auto o) { return o.m(); }
}
class SB6 inherits B6 {
    static string u(auto o) { return o.m(); }
}
class C6 inherits private P6 {
    private:internal abstract string m();
    static string u(auto o) { return o.m(); }
}
class SC6 inherits C6 {
    static string u(auto o) { return o.m(); }
}
class D6 inherits private:internal P6 {
    private:internal abstract string m();
    static string u(auto o) { return o.m(); }
}
class SD6 inherits D6 {
    static string u(auto o) { return o.m(); }
}
class Q6 inherits private P6 {
    private string m() { return "Q6::m"; }
}

# 7: overloaded abstract methods with different accesses: each variant of a parent class that implements one has the
# more restrictive access of its abstract declaration and of its own; Y7 is the control with methods of its own
class Calls7 {
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
    static string c(auto o, auto a) { return call_object_method_args(o, "m", (a,)); }
}
class P7 {
    string m(int i) { return "P7::m(int)"; }
    string m(string s) { return "P7::m(string)"; }
}
class A7 inherits private P7 {
    private abstract string m(int i);
    abstract string m(string s);
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
}
class SA7 inherits A7 {
    static string u(auto o, auto a) { return o.m(a); }
}
class B7 inherits private:internal P7 {
    private:internal abstract string m(int i);
    abstract string m(string s);
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
}
class SB7 inherits B7 {
    static string u(auto o, auto a) { return o.m(a); }
}
class C7 inherits private:internal P7 {
    private:hierarchy abstract string m(int i);
    abstract string m(string s);
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
}
class SC7 inherits C7 {
    static string u(auto o, auto a) { return o.m(a); }
}
class D7 inherits private P7 {
    private:hierarchy abstract string m(int i);
    abstract string m(string s);
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
}
class SD7 inherits D7 {
    static string u(auto o, auto a) { return o.m(a); }
}
class Y7 {
    private string m(int i) { return "Y7::m(int)"; }
    string m(string s) { return "Y7::m(string)"; }
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
}
class SY7 inherits Y7 {
    static string u(auto o, auto a) { return o.m(a); }
}
# public abstract overloads implemented by a private and a public variant of a parent class (qore#7206): the private
# one stays private
class PV7 {
    private string m(int i) { return "PV7::m(int)"; }
    string m(string s) { return "PV7::m(string)"; }
}
class E7 inherits PV7 {
    abstract string m(int i);
    abstract string m(string s);
    static string u(auto o, auto a) { return o.m(a); }
    static string r(auto o, auto a) { return call_function_args(\o.m(), (a,)); }
}
class SE7 inherits E7 {
    static string u(auto o, auto a) { return o.m(a); }
}

# 8: a subclass that declares the abstract variant again does not make public the private variant that the parent
# class's method has (F8, G8: overloaded, H8: one variant, I8: private:internal, J8: a grandchild); an abstract variant
# of another parent class keeps its access when a privately inherited class implements it (K8, L8); M8 to O8 are the
# controls with methods of their own
class F8 inherits private P7 {
    private abstract string m(int i);
    abstract string m(string s);
}
class G8 inherits F8 {
    abstract string m(int i);
}
class H8p inherits private P7 {
    private abstract string m(int i);
}
class H8 inherits H8p {
    abstract string m(int i);
}
class I8p inherits private P7 {
    private:internal abstract string m(int i);
    abstract string m(string s);
}
class I8 inherits I8p {
    abstract string m(int i);
}
class J8 inherits G8 {}
class AK8 {
    private abstract string m(int i);
}
class K8 inherits AK8, private P7 {}
class AL8 {
    private abstract string m(int i);
    abstract string m(string s);
}
class L8 inherits AL8, private P7 {}
class M8p {
    private string m(int i) { return "M8p::m(int)"; }
    string m(string s) { return "M8p::m(string)"; }
}
class M8 inherits M8p {
    abstract string m(int i);
}
class N8p {
    private string m(int i) { return "N8p::m(int)"; }
}
class N8 inherits N8p {
    abstract string m(int i);
}
class O8 inherits N8p {}

A1 a1();
S1 s1();
check("1 A1 outside", auto sub () { return a1.m(); });
check("1 A1 outside untyped", auto sub () { return Calls::u(a1); });
check("1 A1 outside ref", auto sub () { return Calls::r(a1); });
check("1 A1 outside com", auto sub () { return Calls::c(a1); });
check("1 A1 P1 code", auto sub () { return P1::u(a1); });
check("1 A1 P1 code ref", auto sub () { return P1::r(a1); });
check("1 A1 A1 code", auto sub () { return A1::u(a1); });
check("1 A1 A1 code ref", auto sub () { return A1::r(a1); });
check("1 A1 A1 code typed", auto sub () { return A1::t(a1); });
check("1 S1 outside", auto sub () { return s1.m(); });
check("1 S1 outside untyped", auto sub () { return Calls::u(s1); });
check("1 S1 outside com", auto sub () { return Calls::c(s1); });
check("1 S1 P1 code", auto sub () { return P1::u(s1); });
check("1 S1 A1 code", auto sub () { return A1::u(s1); });
check("1 S1 A1 code typed", auto sub () { return A1::t(s1); });
check("1 S1 S1 code", auto sub () { return S1::u(s1); });
check("1 S1 S1 code ref", auto sub () { return S1::r(s1); });

A2 a2();
S2 s2();
check("2 A2 outside", auto sub () { return a2.m(); });
check("2 A2 outside untyped", auto sub () { return Calls::u(a2); });
check("2 A2 outside ref", auto sub () { return Calls::r(a2); });
check("2 A2 outside com", auto sub () { return Calls::c(a2); });
check("2 A2 P2 code", auto sub () { return P2::u(a2); });
check("2 A2 A2 code", auto sub () { return A2::u(a2); });
check("2 S2 outside untyped", auto sub () { return Calls::u(s2); });
check("2 S2 P2 code", auto sub () { return P2::u(s2); });
check("2 S2 A2 code", auto sub () { return A2::u(s2); });
check("2 S2 S2 code", auto sub () { return S2::u(s2); });

foreach list<object> l in ((new A3(), new Q3()), (new S3(), new T3())) {
    object x = l[0];
    object y = l[1];
    string cx = x.className();
    string cy = y.className();
    check(sprintf("3 %s outside untyped", cx), auto sub () { return Calls::u(x); });
    check(sprintf("3 %s outside untyped", cy), auto sub () { return Calls::u(y); });
    check(sprintf("3 %s outside com", cx), auto sub () { return Calls::c(x); });
    check(sprintf("3 %s outside com", cy), auto sub () { return Calls::c(y); });
    check(sprintf("3 %s P3 code", cx), auto sub () { return P3::u(x); });
    check(sprintf("3 %s P3 code", cy), auto sub () { return P3::u(y); });
    check(sprintf("3 %s own code", cx), auto sub () { return A3::u(x); });
    check(sprintf("3 %s own code", cy), auto sub () { return Q3::u(y); });
    if (x instanceof S3) {
        check(sprintf("3 %s subclass code", cx), auto sub () { return S3::u(x); });
        check(sprintf("3 %s subclass code", cy), auto sub () { return T3::u(y); });
    }
}

A4 a4();
check("4 A4 outside untyped", auto sub () { return Calls::u(a4); });
check("4 A4 outside com", auto sub () { return Calls::c(a4); });
check("4 A4 P4 code", auto sub () { return P4::u(a4); });
check("4 A4 O4 code", auto sub () { return O4::u(a4); });
check("4 A4 O4 code ref", auto sub () { return O4::r(a4); });
check("4 A4 A4 code", auto sub () { return A4::u(a4); });

A5 a5();
C5 c5();
B5 b5();
check("5 A5 outside untyped", auto sub () { return Calls::u(a5); });
check("5 A5 P5 code", auto sub () { return P5::u(a5); });
check("5 A5 A5 code", auto sub () { return A5::u(a5); });
check("5 C5 outside untyped", auto sub () { return Calls::u(c5); });
check("5 C5 P5 code", auto sub () { return P5::u(c5); });
check("5 C5 A5 code", auto sub () { return A5::u(c5); });
check("5 C5 C5 code", auto sub () { return C5::u(c5); });
check("5 B5 outside untyped", auto sub () { return Calls::u(b5); });
check("5 B5 P5 code", auto sub () { return P5::u(b5); });
check("5 B5 I5 code", auto sub () { return I5::u(b5); });
check("5 B5 B5 code", auto sub () { return B5::u(b5); });

foreach list<auto> l in ((new A6(), new S6(), \A6::u(), \S6::u()), (new B6(), new SB6(), \B6::u(), \SB6::u()),
        (new C6(), new SC6(), \C6::u(), \SC6::u()), (new D6(), new SD6(), \D6::u(), \SD6::u())) {
    object x = l[0];
    object s = l[1];
    code own = l[2];
    code sub_code = l[3];
    string cx = x.className();
    string cs = s.className();
    check(sprintf("6 %s outside", cx), auto sub () { return x.m(); });
    check(sprintf("6 %s outside untyped", cx), auto sub () { return Calls::u(x); });
    check(sprintf("6 %s outside ref", cx), auto sub () { return Calls::r(x); });
    check(sprintf("6 %s outside com", cx), auto sub () { return Calls::c(x); });
    check(sprintf("6 %s P6 code", cx), auto sub () { return P6::u(x); });
    check(sprintf("6 %s own code", cx), auto sub () { return own(x); });
    check(sprintf("6 %s outside untyped", cs), auto sub () { return Calls::u(s); });
    check(sprintf("6 %s outside ref", cs), auto sub () { return Calls::r(s); });
    check(sprintf("6 %s outside com", cs), auto sub () { return Calls::c(s); });
    check(sprintf("6 %s parent code", cs), auto sub () { return own(s); });
    check(sprintf("6 %s subclass code", cs), auto sub () { return sub_code(s); });
}
check("6 S6 subclass code ref", auto sub () { return S6::r(new S6()); });
Q6 q6();
check("6 Q6 outside untyped", auto sub () { return Calls::u(q6); });
check("6 Q6 outside com", auto sub () { return Calls::c(q6); });

foreach list<auto> l in ((new A7(), new SA7(), \A7::u(), \A7::r(), \SA7::u()),
        (new B7(), new SB7(), \B7::u(), \B7::r(), \SB7::u()),
        (new C7(), new SC7(), \C7::u(), \C7::r(), \SC7::u()),
        (new D7(), new SD7(), \D7::u(), \D7::r(), \SD7::u()),
        (new Y7(), new SY7(), \Y7::u(), \Y7::r(), \SY7::u()),
        (new E7(), new SE7(), \E7::u(), \E7::r(), \SE7::u())) {
    object x = l[0];
    object s = l[1];
    code own = l[2];
    code own_ref = l[3];
    code sub_code = l[4];
    string cx = x.className();
    string cs = s.className();
    foreach auto a in ((1, "a")) {
        string t = a.typeCode() == NT_INT ? "int" : "string";
        check(sprintf("7 %s %s outside", cx, t), auto sub () { return x.m(a); });
        check(sprintf("7 %s %s outside untyped", cx, t), auto sub () { return Calls7::u(x, a); });
        check(sprintf("7 %s %s outside ref", cx, t), auto sub () { return Calls7::r(x, a); });
        check(sprintf("7 %s %s outside com", cx, t), auto sub () { return Calls7::c(x, a); });
        check(sprintf("7 %s %s own code", cx, t), auto sub () { return own(x, a); });
        check(sprintf("7 %s %s own code ref", cx, t), auto sub () { return own_ref(x, a); });
        check(sprintf("7 %s %s outside untyped", cs, t), auto sub () { return Calls7::u(s, a); });
        check(sprintf("7 %s %s outside com", cs, t), auto sub () { return Calls7::c(s, a); });
        check(sprintf("7 %s %s parent code", cs, t), auto sub () { return own(s, a); });
        check(sprintf("7 %s %s subclass code", cs, t), auto sub () { return sub_code(s, a); });
    }
}

foreach object x in ((new G8(), new H8(), new I8(), new J8(), new K8(), new L8(), new M8(), new N8(), new O8())) {
    string cx = x.className();
    foreach auto a in ((1, "a")) {
        string t = a.typeCode() == NT_INT ? "int" : "string";
        check(sprintf("8 %s %s outside", cx, t), auto sub () { return x.m(a); });
        check(sprintf("8 %s %s outside ref", cx, t), auto sub () { return Calls7::r(x, a); });
        check(sprintf("8 %s %s outside com", cx, t), auto sub () { return Calls7::c(x, a); });
    }
}
