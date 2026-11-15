# -*- mode: qore; indent-tabs-mode: nil -*-
# the probes of deleted-object-generic-variant.qtest (qore#6152): the variant selected for a call with explicit type
# arguments of a generic variant beside a variant without type parameters, for a deleted and a live object

%modern

class Obj {
    public {}
}

class Util {
    public {}

    string m<X>(X x) { return "X"; }
    string m(auto x) { return "plain-auto"; }

    static string sm<X>(X x) { return "X"; }
    static string sm(auto x) { return "plain-auto"; }

    #! the non-generic control
    string n(Obj x) { return "Obj"; }
    string n(auto x) { return "plain-auto"; }
}

string sub g<X>(X x) { return "X"; }
string sub g(auto x) { return "plain-auto"; }

#! the non-generic control
string sub f(Obj x) { return "Obj"; }
string sub f(auto x) { return "plain-auto"; }

#! a generic class whose method of the class's type parameter has an overload that takes any value
class Gen<T> {
    public {}

    string m(T x) { return "T"; }
    string m(auto x) { return "plain-auto"; }
}

#! a generic method of a parent class beside a method without type parameters of a child class
class Parent {
    public {}

    string m<X>(X x) { return "parent-X"; }
}

class Child inherits Parent {
    public {}

    string m(auto x) { return "child-auto"; }
}

#! a call without arguments: the variant without parameters is declared first
class Zero {
    public {}

    string z() { return "plain"; }
    string z<X>(*X x) { return "X"; }
}

#! a static method call of a generic class whose type argument is inferred from the arguments
class Inferred<T> {
    public {}

    static string s<X>(X x, int i) { return "X"; }
    static string s(T t, auto i) { return "plain-T"; }
}

class Box<T> {
    auto func(T a) { return g<T>(a); }
    auto stat(T a) { return Util::sm<T>(a); }
    auto ctl(T a) { Util u(); return u.n(a); }
}

#! a typed argument
auto sub m_typed(Obj a) { Util u(); return u.m<Obj>(a); }
#! an untyped argument
auto sub m_auto(auto a) { Util u(); return u.m<Obj>(a); }
auto sub m_named(Obj a) { Util u(); return u.m<Obj>(x: a); }
auto sub sm_typed(Obj a) { return Util::sm<Obj>(a); }
auto sub g_typed(Obj a) { return g<Obj>(a); }
auto sub g_auto(auto a) { return g<Obj>(a); }
auto sub g_named(Obj a) { return g<Obj>(x: a); }
#! the type argument inferred
auto sub m_inferred(Obj a) { Util u(); return u.m(a); }
auto sub gen_typed(Obj a) { Gen<Obj> gen(); return gen.m(a); }
auto sub child_typed(Obj a) { Child c(); return c.m<Obj>(a); }
auto sub n_typed(Obj a) { Util u(); return u.n(a); }
auto sub n_auto(auto a) { Util u(); return u.n(a); }
auto sub f_typed(Obj a) { return f(a); }
auto sub f_auto(auto a) { return f(a); }

#! the result of parsing and calling a call with explicit type arguments
string sub parse_call(string call) {
    Program p(PO_NEW_STYLE);
    try {
        p.parse("class Obj { public {} } class U { public {} string m<X: int>(X x) { return \"X\"; } "
            "string m(auto x) { return \"plain-auto\"; } string p(auto x) { return \"plain-auto\"; } } "
            "auto sub t() { U u(); return " + call + "; }", "explicit-type-args");
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
    return sprintf("%y", p.callFunction("t"));
}

string sub probe(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! the results for an object that is deleted (del = True) or live (del = False)
string sub probes(bool del) {
    Obj o();
    Obj d = o;
    if (del) {
        delete o;
    }
    Box<Obj> b();
    Util u();
    list<string> l = (
        probe(auto sub () { return m_typed(d); }),
        probe(auto sub () { return m_auto(d); }),
        probe(auto sub () { return m_named(d); }),
        probe(auto sub () { return u.m<Obj>(d); }),
        probe(auto sub () { return sm_typed(d); }),
        probe(auto sub () { return g_typed(d); }),
        probe(auto sub () { return g_auto(d); }),
        probe(auto sub () { return g_named(d); }),
        probe(auto sub () { return g<Obj>(d); }),
        probe(auto sub () { return b.func(d); }),
        probe(auto sub () { return b.stat(d); }),
        probe(auto sub () { return b.ctl(d); }),
        probe(auto sub () { return n_typed(d); }),
        probe(auto sub () { return n_auto(d); }),
        probe(auto sub () { return f_typed(d); }),
        probe(auto sub () { return f_auto(d); }),
        # the variant without type parameters is a candidate at parse time as at run time
        probe(auto sub () { return u.m<Obj>(1); }),
        probe(auto sub () { return g<Obj>("s"); }),
        probe(auto sub () { return m_inferred(d); }),
        probe(auto sub () { return gen_typed(d); }),
        probe(auto sub () { return child_typed(d); }),
    );
    if (!del) {
        # explicit type arguments that no generic variant takes: more than its type parameters, outside its bound, and
        # of a method without a generic variant; and ones that it takes
        l += (
            parse_call("u.m<Obj, int>(new Obj())"),
            parse_call("u.m<Obj>(new Obj())"),
            parse_call("u.p<int>(1)"),
            parse_call("u.m<int>(1)"),
            probe(auto sub () { Zero z(); return z.z<int>(); }),
            probe(auto sub () { return Inferred::s<int>("str", "a"); }),
        );
    }
    return foldl $1 + "|" + $2, l;
}
