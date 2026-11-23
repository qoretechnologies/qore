# -*- mode: qore; indent-tabs-mode: nil -*-
# the classes and calls of private-internal-subclass-calls.qtest that inherit the classes of the namespace PIS, which
# probe.q declares itself and module-user.q loads from the module; included by both

# a subclass of K, which inherits C5 with private:internal access, with another parent class
class L inherits PIS::K, PIS::P3 {
    string m(string s) { return "L::m(string)"; }
    string dyn(auto a) { object o = self; return o.m(a); }
}
# a subclass of K: the call forms of its code
class L2 inherits PIS::K {
    string m(string s) { return "L2::m(string)"; }
    static string s(string x) { return "L2::s(string)"; }

    string dyn(auto a) { object o = self; return o.m(a); }
    string selfcall(auto a) { return self.m(a); }
    string com(auto a) { return call_object_method(self, "m", a); }
    string coma(auto a) { return call_object_method_args(self, "m", (a,)); }
    string cref(auto a) { code c = self.getCallReference("m"); return c(a); }
    string bref(auto a) { code c = \self.m(); return c(a); }
    string cfa(auto a) { return call_function_args(self.getCallReference("m"), (a,)); }
    string clos(auto a) { code c = string sub () { object o = self; return o.m(a); }; return c(); }
    string bg(auto a) {
        Queue q();
        background q.push(trycall(string sub () { object o = self; return o.m(a); }));
        return q.get();
    }
    string sdyn(auto a) { return call_static_method("L2", "s", a); }
    string sdirect(auto a) { return L2::s(a); }
}
# a subclass of K without a method of the name
class L5 inherits PIS::K {
    string dyn(auto a) { object o = self; return o.m(a); }
    string com(auto a) { return call_object_method(self, "m", a); }
    string sdyn(auto a) { return call_static_method("L5", "s", a); }
}
# deeper subclasses
class L3 inherits L2 {
    string dyn3(auto a) { object o = self; return o.m(a); }
}
class L4 inherits L3 {
    string m(float f) { return "L4::m(float)"; }
    string dyn4(auto a) { object o = self; return o.m(a); }
}
# a class that inherits K with private:internal access: C5 is limited to K, not to it
class K2 inherits private:internal PIS::K {
    string k2dyn(auto a) { object o = self; return o.m(a); }
}
class M2 inherits K2 {
    string m(string s) { return "M2::m(string)"; }
    string dyn(auto a) { object o = self; return o.m(a); }
}
# a public parent class beside a private:internal one: only the inheriting class's code calls Y's methods
class Head2 inherits PIS::A, private:internal PIS::Y {
    string m(bool b) { return "Head2::m(bool)"; }
    string hdyn(auto a) { object o = self; return o.m(a); }
}
class Head3 inherits Head2 {
    string h3dyn(auto a) { object o = self; return o.m(a); }
}
# two private:internal parent classes: the inheriting class's code calls the methods of both
class Two inherits private:internal PIS::Y, private:internal PIS::P3 {
    string m(string s) { return "Two::m(string)"; }
    string dyn(auto a) { object o = self; return o.m(a); }
}
class Two2 inherits Two {
    string dyn2(auto a) { object o = self; return o.m(a); }
}
# a path with two private:internal edges: D7 is limited to C7 and C7 to K7, so neither reaches D7 through K7
class D7 { string m(int i) { return "D7::m(int)"; } }
class C7 inherits private:internal D7 {
    string m(float f) { return "C7::m(float)"; }
    string c7call(object o, auto a) { return o.m(a); }
}
class K7 inherits private:internal C7 {
    string m(string s) { return "K7::m(string)"; }
    string c7dyn(auto a) { return c7call(self, a); }
    string k7dyn(auto a) { object o = self; return o.m(a); }
}
# two classes that inherit the same class with private:internal access: the code of each calls X8's method through
# the method of A8; A8's and other code cannot
class X8 { string m(int i) { return "X8::m(int)"; } }
class B8 inherits private:internal X8 { string b8call(object o, auto a) { return o.m(a); } }
class C8 inherits private:internal X8 { string c8call(object o, auto a) { return o.m(a); } }
class A8 inherits B8, C8 {
    string m(string s) { return "A8::m(string)"; }
    string bdyn(auto a) { return b8call(self, a); }
    string cdyn(auto a) { return c8call(self, a); }
    string adyn(auto a) { object o = self; return o.m(a); }
}
# a class that inherits X8 with private:internal access itself after or before a parent class that does
class D8 inherits B8, private:internal X8 {
    string m(string s) { return "D8::m(string)"; }
    string ddyn(auto a) { object o = self; return o.m(a); }
    string bdyn(auto a) { return b8call(self, a); }
    # resolved when parsed
    string dtyped() { D8 o = self; return o.m(1); }
}
class E8 inherits private:internal X8, B8 {
    string m(string s) { return "E8::m(string)"; }
    string edyn(auto a) { object o = self; return o.m(a); }
    string bdyn(auto a) { return b8call(self, a); }
}
class F8 inherits D8 {
    string fdyn(auto a) { object o = self; return o.m(a); }
}
# a class that a public path reaches after a private:internal one: its methods are public
class N9 { string m(int i) { return "N9::m(int)"; } }
class M9 inherits N9 {}
class B9 inherits private:internal M9 { string b9call(object o, auto a) { return o.m(a); } }
class C9 inherits M9 { string c9call(object o, auto a) { return o.m(a); } }
class A9 inherits B9, C9 {
    string m(string s) { return "A9::m(string)"; }
    string bdyn(auto a) { return b9call(self, a); }
    string cdyn(auto a) { return c9call(self, a); }
    string adyn(auto a) { object o = self; return o.m(a); }
}
# a class reached through the private:internal paths of two classes and a public path: its method is public and
# listed once in the variants that an overload error reports
class X10 { string m(int i) { return "X10::m(int)"; } }
class K10a inherits private:internal X10 {}
class K10b inherits private:internal X10 {
    string variants(object o) {
        try {
            return o.m(True);
        } catch (hash<ExceptionInfo> ex) {
            return sprintf("%s: X10::m(int) listed %d time(s)", ex.err, (ex.desc =~ x/(X10::m\(int)/g).size());
        }
    }
}
class P10 inherits X10 {}
class Root10 inherits K10a, K10b, P10 {
    string m(string s) { return "Root10::m(string)"; }
    string variants() { return K10b::variants(self); }
}
# classes whose private:internal paths merge before the class of the method: the code of each class of a
# private:internal edge on a path calls it
class B11 { string m(int i) { return "B11::m(int)"; } }
class A11 inherits B11 {}
class K11 inherits A11 {}
class X11 inherits private:internal K11 { string xd(auto a) { object o = self; return o.m(a); } }
class Y11 inherits private:internal A11 { string yd(auto a) { object o = self; return o.m(a); } }
class Z11 inherits private:internal B11 { string zd(auto a) { object o = self; return o.m(a); } }
class R11 inherits X11, Y11 { string m(string s) { return "R11::m(string)"; } }
class R12 inherits Z11, X11, Y11 { string m(string s) { return "R12::m(string)"; } }
# classes that inherit their own classes with private:internal access, each of which inherits a common chain
class C13a { string m(int i) { return "C13a::m(int)"; } }
class C13b inherits C13a {}
class C13c inherits C13b {}
class I13a inherits C13c {}
class I13b inherits C13c {}
class I13c inherits C13b {}
class K13a inherits private:internal I13a { string ka(auto a) { object o = self; return o.m(a); } }
class K13b inherits private:internal I13b { string kb(auto a) { object o = self; return o.m(a); } }
class K13c inherits private:internal I13c { string kc(auto a) { object o = self; return o.m(a); } }
class R13 inherits K13a, K13b, K13c {
    string m(string s) { return "R13::m(string)"; }
    string rd(auto a) { object o = self; return o.m(a); }
}
# an unrelated class
class Other {
    string call(object o, auto a) { return o.m(a); }
}

string sub trycall(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

const Args = (1, "a", True, 1.5);

# the results of a call with each argument of Args
string sub results(code call) {
    list<string> rv = ();
    foreach auto arg in (Args) {
        rv += sprintf("%s: %s", arg.type(), trycall(auto sub () { return call(arg); }));
    }
    return rv.join("; ");
}

sub show(string label, object o, string meth) {
    printf("%s %s: %s\n", label, meth, results(auto sub (auto a) { return call_object_method(o, meth, a); }));
}

# outside code
sub show_outside(string label, object o) {
    printf("%s outside: %s\n", label, results(auto sub (auto a) { return o.m(a); }));
}

{
    PIS::K k();
    show("K", k, "kdyn");

    L l();
    show_outside("L", l);
    foreach string m in ("dyn", "kdyn", "kcom", "pdyn") {
        show("L", l, m);
    }

    L2 l2();
    show_outside("L2", l2);
    foreach string m in ("dyn", "selfcall", "com", "coma", "cref", "bref", "cfa", "clos", "bg", "sdyn", "sdirect",
            "kdyn", "kcom", "kcoma", "kref") {
        show("L2", l2, m);
    }
    printf("outside static: %s\n", results(auto sub (auto a) { return call_static_method("L2", "s", a); }));

    L5 l5();
    show_outside("L5", l5);
    foreach string m in ("dyn", "com", "sdyn", "kdyn", "kcom") {
        show("L5", l5, m);
    }

    L3 l3();
    show_outside("L3", l3);
    foreach string m in ("dyn3", "dyn", "kdyn") {
        show("L3", l3, m);
    }
    L4 l4();
    show_outside("L4", l4);
    foreach string m in ("dyn4", "dyn", "kdyn") {
        show("L4", l4, m);
    }

    M2 m2();
    show_outside("M2", m2);
    foreach string m in ("dyn", "k2dyn") {
        show("M2", m2, m);
    }

    Head2 h2();
    show_outside("Head2", h2);
    foreach string m in ("hdyn", "adyn") {
        show("Head2", h2, m);
    }
    Head3 h3();
    show_outside("Head3", h3);
    foreach string m in ("h3dyn", "hdyn", "adyn") {
        show("Head3", h3, m);
    }

    Two two();
    show_outside("Two", two);
    show("Two", two, "dyn");
    Two2 two2();
    show_outside("Two2", two2);
    foreach string m in ("dyn2", "dyn") {
        show("Two2", two2, m);
    }

    K7 k7();
    show_outside("K7", k7);
    foreach string m in ("c7dyn", "k7dyn") {
        show("K7", k7, m);
    }

    A8 a8();
    show_outside("A8", a8);
    foreach string m in ("bdyn", "cdyn", "adyn") {
        show("A8", a8, m);
    }

    D8 d8();
    show_outside("D8", d8);
    foreach string m in ("ddyn", "bdyn") {
        show("D8", d8, m);
    }
    E8 e8();
    show_outside("E8", e8);
    foreach string m in ("edyn", "bdyn") {
        show("E8", e8, m);
    }
    F8 f8();
    show_outside("F8", f8);
    foreach string m in ("fdyn", "ddyn", "bdyn") {
        show("F8", f8, m);
    }
    printf("D8 typed: %s\n", d8.dtyped());

    A9 a9();
    show_outside("A9", a9);
    foreach string m in ("bdyn", "cdyn", "adyn") {
        show("A9", a9, m);
    }
    {
        A9 ta9();
        printf("A9 typed: %s\n", ta9.m(1));
    }

    Root10 r10();
    show_outside("Root10", r10);
    printf("Root10 variants: %s\n", r10.variants());

    R11 r11();
    show_outside("R11", r11);
    foreach string m in ("xd", "yd") {
        show("R11", r11, m);
    }
    R12 r12();
    show_outside("R12", r12);
    foreach string m in ("zd", "xd", "yd") {
        show("R12", r12, m);
    }
    R13 r13();
    show_outside("R13", r13);
    foreach string m in ("ka", "kb", "kc", "rd") {
        show("R13", r13, m);
    }

    Other other();
    foreach object o in ((l2, l3, m2, h2, two, k7, a8, a9, r11, r13)) {
        printf("Other %s: %s\n", o.className(), results(auto sub (auto a) { return other.call(o, a); }));
    }
}
