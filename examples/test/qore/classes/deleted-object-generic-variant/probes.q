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

#! two receiver classes through one call site: the child class overrides the method without type parameters
class Base {
    public {}

    string m<X>(X x) { return "base-X"; }
    string m(auto x) { return "base-auto"; }
}

class Derived inherits Base {
    public {}
}

class Override inherits Base {
    public {}

    string m(auto x) { return "override-auto"; }
}

#! a method type parameter bounded by the class's type parameter (qore#6155)
class Bounded<T> {
    public {}

    string m<X: T>(X x) { return "X"; }
    string m(auto x) { return "plain"; }

    static string sm<X: T>(X x) { return "sX"; }
    static string sm(auto x) { return "splain"; }

    #! the class's type parameter as the type argument
    string viaT(T a) { return m<T>(a); }
    #! an argument of a type that is known at parse time, outside the bound of a class instantiated with Obj
    string outside() { return m(1); }
    #! a static method called from a method of the class: the bound takes the class's instantiation
    string viaStatic(auto a) { return sm(a); }
}

#! a default type that names the class's type parameter
class BoundedDefault<T> {
    public {}

    X e<X: T = T>(auto v) { return v; }
}

#! the original case of qore#6155: the only variant, with an inferred type argument
class BoundedOnly<T> {
    public {}

    X m<X: T>(X x) { return x; }
}

auto sub bounded_only_auto(auto a) { BoundedOnly<int> b(); return b.m(a); }

#! a static method called with a receiver that names the calling code's type parameter: the bound is not satisfied
class BoundedCaller<U> {
    public {}

    string viaReceiver(auto a) { return Bounded<U>::sm(a); }
    string viaReceiverOnly(auto a) { return BoundedOnlyStatic<U>::only(a); }
}

class BoundedOnlyStatic<T> {
    public {}

    static string only<X: T>(X x) { return "oX"; }
}

string sub bounded_generic_receiver<Y>(auto a) { return Bounded<Y>::sm(a); }

class Box<T> {
    auto func(T a) { return g<T>(a); }
    auto stat(T a) { return Util::sm<T>(a); }
    #! the call of qore#6152: a method with an explicit type argument naming the class's type parameter
    auto meth(T a) { Util u(); return u.m<T>(a); }
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

#! one call site for every call: the variant selected at run time depends on the receiver and the argument
string sub site(Base b, auto a) { return b.m<Obj>(a); }

#! one call site whose type argument is the type parameter of the calling generic function
string sub generic_site<T>(auto a) { Util u(); return u.m<T>(a); }

#! the results of calls through one call site with live and deleted objects in turn
string sub alternate(Obj o, Obj d) {
    list<string> l = ();
    for (int i = 0; i < 6; ++i) {
        l += site(new Derived(), (i % 2) != 0 ? d : o);
    }
    return l.join(",");
}

#! the results of calls through one call site in several threads, each with live and deleted objects in turn
string sub threads(Obj o, Obj d) {
    Queue q();
    for (int t = 0; t < 4; ++t) {
        background sub () {
            bool ok = True;
            for (int i = 0; i < 200; ++i) {
                bool del = ((i + t) % 2) != 0;
                if (site(new Derived(), del ? d : o) != "base-X") {
                    ok = False;
                }
            }
            q.push(ok);
        }();
    }
    list<bool> l = ();
    for (int t = 0; t < 4; ++t) {
        l += q.get(30s);
    }
    return sprintf("%y", l);
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
        probe(auto sub () { return b.meth(d); }),
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
            # a type argument within the bound of the class's type parameter, in the class and from outside it, and
            # an argument outside the bound (qore#6155)
            probe(auto sub () { Bounded<Obj> bo(); return bo.viaT(new Obj()); }),
            probe(auto sub () { Bounded<Obj> bo(); return bo.m<Obj>(new Obj()); }),
            probe(auto sub () { Bounded<Obj> bo(); return bo.m(1); }),
            probe(auto sub () { BoundedOnly<int> bi(); return bi.m(1); }),
            probe(auto sub () { return bounded_only_auto(1); }),
            probe(auto sub () { return bounded_only_auto("s"); }),
            probe(auto sub () { Bounded<Obj> bo(); return bo.outside(); }),
            probe(auto sub () { Bounded<Obj> bo(); return bo.viaStatic(1); }),
            probe(auto sub () { Bounded<Obj> bo(); return bo.viaStatic(new Obj()); }),
            probe(auto sub () { BoundedDefault<string> bd(); return bd.e("s"); }),
            probe(auto sub () { BoundedCaller<Obj> bc(); return bc.viaReceiver(new Obj()); }),
            probe(auto sub () { BoundedCaller<Obj> bc(); return bc.viaReceiverOnly(new Obj()); }),
            probe(auto sub () { return bounded_generic_receiver<Obj>(new Obj()); }),
        );
    } else {
        # the same call sites with a live object, a deleted object and other receivers in turn
        Obj live();
        l += (
            alternate(live, d),
            (map site($1, live), (new Derived(), new Override(), new Base(), new Override())).join(","),
            (map site($1, d), (new Derived(), new Override())).join(","),
            (generic_site<Obj>(live), generic_site<string>(live), generic_site<Obj>(live)).join(","),
            threads(live, d),
        );
    }
    return foldl $1 + "|" + $2, l;
}
