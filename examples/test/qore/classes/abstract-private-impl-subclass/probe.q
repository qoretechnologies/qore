#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a subclass of a class that implements its abstract methods with the methods of a privately inherited parent class
# calls them as the class does; the test compares the output of each execution mode and of qcc executables with
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

#! prints the outcome of calls of m() with the argument \a arg on an object of the class \a cn from outside the class
#! hierarchy
sub calls(string cn, auto arg, string al) {
    string p = sprintf("%s m(%s)", cn, al);
    check(p, string sub () { auto o = create_object(cn); return o.m(arg); });
    check(p + " ref", string sub () { auto o = create_object(cn); code c = \o.m(); return c(arg); });
    check(p + " call_object_method", string sub () { return call_object_method(create_object(cn), "m", arg); });
    check(p + " call_function_args", string sub () {
        auto o = create_object(cn);
        return call_function_args(\call_object_method(), (o, "m", arg));
    });
}

class Impl {
    string m(int i) { return "Impl::m(int)"; }
    string m(string s) { return "Impl::m(string)"; }
}
class A {
    private abstract string m(int i);
    abstract string m(string s);
}

# private inheritance of the implementing class
class X inherits A, private Impl {
    string own(auto a) { return m(a); }
}
# a subclass with an overload of its own
class Y3 inherits X {
    string m(bool b) { return "Y3::m(bool)"; }
    string via(auto a) { return m(a); }
    string subInt() { return m(1); }
    string subStr() { return m("a"); }
}
# a subclass with no method of its own
class Y0 inherits X {
    string via(auto a) { return m(a); }
}
# a subclass of the subclass with the overload
class Z3 inherits Y3 {}
# a subclass with an overload of its own of the subclass with no method
class Z0 inherits Y0 {
    string m(float f) { return "Z0::m(float)"; }
}
# a subclass that overrides the public variant
class Y5 inherits X {
    string m(string s) { return "Y5::m(string)"; }
}
# a subclass with an overload of its own that overrides the public variant
class Y6 inherits X {
    string m(bool b) { return "Y6::m(bool)"; }
    string m(string s) { return "Y6::m(string)"; }
}

# private:hierarchy inheritance
class XH inherits A, private:hierarchy Impl {}
class YH3 inherits XH {
    string m(bool b) { return "YH3::m(bool)"; }
    string via(auto a) { return m(a); }
}

# private:internal inheritance
class XI inherits A, private:internal Impl {}
class YI3 inherits XI {
    string m(bool b) { return "YI3::m(bool)"; }
    string via(auto a) { return m(a); }
}
class ZI3 inherits YI3 {}

# controls: the class implements the abstract methods itself
class Xc inherits A {
    private string m(int i) { return "Xc::m(int)"; }
    string m(string s) { return "Xc::m(string)"; }
}
class Yc inherits Xc {
    string m(bool b) { return "Yc::m(bool)"; }
    string via(auto a) { return m(a); }
}
# public inheritance of the implementing class
class Xp inherits A, Impl {}
class Yp inherits Xp {
    string m(bool b) { return "Yp::m(bool)"; }
}

foreach string cn in ("X", "Y3", "Y0", "Z3", "Z0", "Y5", "Y6", "XH", "YH3", "XI", "YI3", "ZI3", "Xc", "Yc", "Xp", "Yp") {
    calls(cn, "a", "string");
    # the private variant stays private from outside the class hierarchy
    calls(cn, 1, "int");
}
calls("Y3", True, "bool");
calls("Z3", True, "bool");
calls("Z0", 1.5, "float");

# typed calls
check("X typed", string sub () { X o(); return o.m("a"); });
check("Y3 typed", string sub () { Y3 o(); return o.m("a"); });
check("Y3 typed bool", string sub () { Y3 o(); return o.m(True); });
check("Y0 typed", string sub () { Y0 o(); return o.m("a"); });
check("Z3 typed", string sub () { Z3 o(); return o.m("a"); });
check("Z0 typed", string sub () { Z0 o(); return o.m("a"); });
check("Y5 typed", string sub () { Y5 o(); return o.m("a"); });
check("Y6 typed", string sub () { Y6 o(); return o.m("a"); });
check("YH3 typed", string sub () { YH3 o(); return o.m("a"); });
check("YI3 typed", string sub () { YI3 o(); return o.m("a"); });
check("ZI3 typed", string sub () { ZI3 o(); return o.m("a"); });
check("Yc typed", string sub () { Yc o(); return o.m("a"); });
check("Yp typed", string sub () { Yp o(); return o.m("a"); });
# typed as the parent class
check("Y3 as X", string sub () { X o = new Y3(); return o.m("a"); });
check("Y5 as X", string sub () { X o = new Y5(); return o.m("a"); });
check("Y3 as A", string sub () { A o = new Y3(); return o.m("a"); });
check("YI3 as XI", string sub () { XI o = new YI3(); return o.m("a"); });

# calls in the code of the class hierarchy
check("X own int", string sub () { return (new X()).own(1); });
check("X own string", string sub () { return (new X()).own("a"); });
foreach string cn in ("Y3", "Y0", "Z3", "YH3", "YI3", "ZI3", "Yc") {
    check(cn + " via int", string sub () { auto o = create_object(cn); return o.via(1); });
    check(cn + " via string", string sub () { auto o = create_object(cn); return o.via("a"); });
}
check("Y3 via bool", string sub () { return (new Y3()).via(True); });
check("Y3 subInt", string sub () { return (new Y3()).subInt(); });
check("Y3 subStr", string sub () { return (new Y3()).subStr(); });
check("Z3 subInt", string sub () { return (new Z3()).subInt(); });
check("Y3 own int", string sub () { return (new Y3()).own(1); });
check("Y3 own string", string sub () { return (new Y3()).own("a"); });
check("Y5 own string", string sub () { return (new Y5()).own("a"); });
