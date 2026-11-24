# -*- mode: qore; indent-tabs-mode: nil -*-
# the classes and calls of abstract-typed-call-created-method.qtest that inherit the classes of the namespace ATC,
# which probe.q declares itself and module-user.q loads from the module; included by both

# G6 implements G0's abstract m(int) with C5's variant, so resolving its abstract methods creates its own method m,
# which has G3's m(bool) too; a call bound when parsing (a typed call, a call in the code of the class) calls it as
# the untyped call does, also when it is parsed before the class's abstract methods are resolved (qore#7411)

# a class parsed before G6 whose code calls G6's method
class Early {
    string t() { G6 g(); return g.m(True); }
}
class G6 inherits ATC::C5, ATC::G3 {
    string own(bool b) { return m(b); }
    string ownSelf(bool b) { return self.m(b); }
    string ownInt(int i) { return m(i); }
    code ownRef() { return \m(); }
}
# the same with private:internal and private inheritance of the implementing class
class G6s inherits private:internal ATC::C5, ATC::G3 {
    string own(bool b) { return m(b); }
    string ownInt(int i) { return m(i); }
}
class G6p inherits private ATC::C5, ATC::G3 {
    string own(bool b) { return m(b); }
    string ownInt(int i) { return m(i); }
}
# the implementation reached through a parent class that inherits it with private:internal access: the parent class gets
# a method of its own for G0's abstract m(int), which implements it in G9 too
class Pi inherits private:internal ATC::C5, ATC::G0 {}
class G9 inherits Pi, ATC::G3 {
    string own(bool b) { return m(b); }
}
# overloads of a function called with the call: the type of the call before it is bound is that of the variants that
# all the classes of the hierarchy with a method of the name select, so that the overload selected when parsing is the
# one that the bound call needs (a parent class that gets a method of its own, and a parent class whose parent classes
# have more than one method of the name)
class Pq inherits ATC::C5, ATC::G3 {}
class Qb { int m(bool b) { return 7; } }
class Cq inherits Pq, Qb {}
class Sm { string m(int i) { return "Sm::m(int)"; } }
class Tm { int m(bool b) { return 7; } }
class Rm inherits Sm, Tm {}
class G10 inherits Rm, ATC::G3 {}
string sub fo(int i) { return "fo(int)"; }
string sub fo(string s) { return "fo(string)"; }
# a variant declared private:internal in its class, which the method that a subclass gets for an abstract variant has:
# the code of the subclass calls the other variant, also with a call bound once the abstract methods are resolved
# (qore#7410)
class Bp {
    private:internal string m(string s) { return "Bp::m(string) private:internal"; }
    string m(softstring s) { return "Bp::m(softstring)"; }
}
class G0p { abstract string m(string s); }
class Cp inherits Bp, G0p {
    string own() { return m("x"); }
    string dyn() { auto a = self; return a.m("x"); }
}
class Bq {
    private:internal int m(string s) { return 1; }
    string m(softstring s) { return "Bq::m(softstring)"; }
}
class G0q { private:internal abstract int m(string s); }
class Cq2 inherits Bq, G0q {
    string own() { string s = "x"; return fo(m(s)); }
}
# the parent classes in the other order
class G6r inherits ATC::G3, ATC::C5 {
    string own(bool b) { return m(b); }
}
# an argument whose type is not known when parsing: the variant of the call is selected at run time
auto sub any_value(auto v) { return v; }

# a generic class whose type argument is inferred from the type of the call
class Box<T> {
    public {
        T v;
    }
    constructor(T v) { self.v = v; }
}

# member initializers and a default argument parsed with the class
class Mi {
    public {
        string v = (new G6()).m(True);
        string vi = (new G6()).m(1);
        string vu = (new G6()).m(any_value(True));
        static string sv = (new G6()).m(True);
    }
    string d(string a = (new G6()).m(False)) { return a; }
}
# a subclass without the method, and a class with an overload of its own
class G7 inherits G6 {
    string own(bool b) { return m(b); }
    # calls of the parent class's method named with its class
    string scoped(bool b) { return G6::m(b); }
    code scopedRef() { return \G6::m(); }
}
class G8 inherits ATC::C5, ATC::G3 {
    string m(string s) { return "G8::m(string)"; }
    string own(bool b) { return m(b); }
}

sub t(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# a function parsed before the calls below
string sub top(bool b) { G7 g(); return g.m(b); }

G6 g6();
auto u6 = g6;
t("typed", auto sub () { return g6.m(True) + "; " + g6.m(1); });
t("untyped", auto sub () { return u6.m(True) + "; " + u6.m(1); });
t("own code", auto sub () { return g6.own(True) + "; " + g6.ownSelf(True) + "; " + g6.ownInt(1); });
t("earlier class", auto sub () { return (new Early()).t(); });
G6s g6s();
auto u6s = g6s;
t("private:internal typed", auto sub () { return g6s.m(True) + "; " + g6s.m(1); });
t("private:internal untyped", auto sub () { return u6s.m(True) + "; " + u6s.m(1); });
t("private:internal own code", auto sub () { return g6s.own(True) + "; " + g6s.ownInt(1); });
G6p g6p();
auto u6p = g6p;
t("private typed", auto sub () { return g6p.m(True) + "; " + g6p.m(1); });
t("private untyped", auto sub () { return u6p.m(True) + "; " + u6p.m(1); });
t("private own code", auto sub () { return g6p.own(True) + "; " + g6p.ownInt(1); });
G6r g6r();
G9 g9();
auto u9 = g9;
t("private:internal parent typed", auto sub () { return g9.m(True) + "; " + g9.m(1); });
t("private:internal parent untyped", auto sub () { return u9.m(True) + "; " + u9.m(1); });
t("private:internal parent own code", auto sub () { return g9.own(True); });
Cq cq();
auto ucq = cq;
t("overload, pending parent", auto sub () { return fo(cq.m(True)) + "; " + fo(ucq.m(True)); });
G10 g10();
auto ug10 = g10;
t("overload, parent with methods in its hierarchy", auto sub () { return fo(g10.m(True)) + "; " + fo(ug10.m(True)); });
t("private:internal variant", auto sub () { Cp cp(); return cp.own() + "; " + cp.dyn(); });
t("private:internal variant, overload", auto sub () { return (new Cq2()).own(); });
t("other order", auto sub () { return g6r.m(True) + "; " + g6r.m(1) + "; " + g6r.own(True); });
t("member initializer", auto sub () { Mi mi(); return mi.v + "; " + mi.vi + "; " + mi.vu + "; " + Mi::sv; });
t("default argument", auto sub () { return (new Mi()).d(); });
G7 g7();
auto u7 = g7;
t("subclass typed", auto sub () { return g7.m(True) + "; " + g7.m(1); });
t("subclass untyped", auto sub () { return u7.m(True) + "; " + u7.m(1); });
t("subclass own code", auto sub () { return g7.own(True) + "; " + top(True); });
G8 g8();
auto u8 = g8;
t("own overload typed", auto sub () { return g8.m(True) + "; " + g8.m(1) + "; " + g8.m("s"); });
t("own overload untyped", auto sub () { return u8.m(True) + "; " + u8.m(1) + "; " + u8.m("s"); });
t("own overload own code", auto sub () { return g8.own(True); });
ATC::G3 g3 = g6;
t("typed as G3", auto sub () { return g3.m(True); });
t("named argument", auto sub () { return g6.m(b: True) + "; " + g6.m(i: 1); });
object o6 = g6;
t("cast", auto sub () { return cast<G6>(o6).m(True); });
t("subclass scoped", auto sub () { return g7.scoped(True); });
code r6 = \g6.m();
t("call reference", auto sub () { return r6(True) + "; " + r6(1); });
t("call reference in the class", auto sub () { return g6.ownRef()(True); });
t("scoped call reference", auto sub () { return g7.scopedRef()(True); });
t("call_object_method", auto sub () { return call_object_method(g6, "m", True); });
# the type of the call is known before it is bound (the variants that its arguments select have it)
t("typed result", auto sub () { string s = g6.m(True); string si = g6.m(1); return s + "; " + si; });
t("inferred type argument", auto sub () { auto bx = new Box(g6.m(True)); Box<string> bs = bx; return bs.v; });
*G6 ng6 = g6;
t("named argument, or-nothing receiver", auto sub () { return ng6.m(b: True); });
