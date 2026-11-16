# -*- mode: qore; indent-tabs-mode: nil -*-
# the probes of deleted-object-overload.qtest (qore#6159): the variant selected for a deleted object, which keeps its
# class for the selection, in each call form, compared with the variant selected for the live object

%modern

class Obj {
    public {}

    string getName() { return "obj"; }
}

class Child inherits Obj {
}

#! the variants of each case as functions
string sub k(Obj x) { return "Obj"; }
string sub k(int x) { return "int"; }
string sub n1(Obj x) { return "Obj"; }
string sub p(*Obj x) { return "*Obj"; }
string sub p(int x) { return "int"; }
string sub a(Obj x) { return "Obj"; }
string sub a(auto x) { return "auto"; }
string sub g<X>(X x) { return "X"; }
string sub g(auto x) { return "auto"; }
#! uses the object: a deleted object raises OBJECT-ALREADY-DELETED
string sub use(Obj x) { return x.getName(); }

#! the variants of each case as static and normal methods
class U {
    public {}

    static string k(Obj x) { return "Obj"; }
    static string k(int x) { return "int"; }
    static string n1(Obj x) { return "Obj"; }
    static string p(*Obj x) { return "*Obj"; }
    static string p(int x) { return "int"; }
    static string a(Obj x) { return "Obj"; }
    static string a(auto x) { return "auto"; }
    static string g<X>(X x) { return "X"; }
    static string g(auto x) { return "auto"; }
    static string use(Obj x) { return x.getName(); }

    string mk(Obj x) { return "Obj"; }
    string mk(int x) { return "int"; }
    string mn1(Obj x) { return "Obj"; }
    string mp(*Obj x) { return "*Obj"; }
    string mp(int x) { return "int"; }
    string ma(Obj x) { return "Obj"; }
    string ma(auto x) { return "auto"; }
    string mg<X>(X x) { return "X"; }
    string mg(auto x) { return "auto"; }
    string muse(Obj x) { return x.getName(); }
}

#! a generic method of a parent class beside a method without type parameters of a child class
class Parent {
    public {}

    string m<X>(X x) { return "parent-X"; }
}

class Kid inherits Parent {
    public {}

    string m(auto x) { return "child-auto"; }
}

string sub r(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! the results of the calls of each case in each call form for the given object
list<string> sub calls(Obj o) {
    U u();
    # the calls with typed (Obj) and untyped (auto) arguments
    Obj t = o;
    auto x = o;
    return (
        # functions
        r(string sub () { return k(t); }), r(string sub () { return k(x); }),
        r(string sub () { return n1(t); }), r(string sub () { return n1(x); }),
        r(string sub () { return p(t); }), r(string sub () { return p(x); }),
        r(string sub () { return a(t); }), r(string sub () { return a(x); }),
        r(string sub () { return g<Obj>(t); }), r(string sub () { return g<Obj>(x); }),
        r(string sub () { return use(t); }), r(string sub () { return use(x); }),
        # static methods
        r(string sub () { return U::k(t); }), r(string sub () { return U::k(x); }),
        r(string sub () { return U::n1(t); }), r(string sub () { return U::n1(x); }),
        r(string sub () { return U::p(t); }), r(string sub () { return U::p(x); }),
        r(string sub () { return U::a(t); }), r(string sub () { return U::a(x); }),
        r(string sub () { return U::g<Obj>(t); }), r(string sub () { return U::g<Obj>(x); }),
        r(string sub () { return U::use(t); }), r(string sub () { return U::use(x); }),
        # normal methods
        r(string sub () { return u.mk(t); }), r(string sub () { return u.mk(x); }),
        r(string sub () { return u.mn1(t); }), r(string sub () { return u.mn1(x); }),
        r(string sub () { return u.mp(t); }), r(string sub () { return u.mp(x); }),
        r(string sub () { return u.ma(t); }), r(string sub () { return u.ma(x); }),
        r(string sub () { return u.mg<Obj>(t); }), r(string sub () { return u.mg<Obj>(x); }),
        r(string sub () { return u.muse(t); }), r(string sub () { return u.muse(x); }),
        # call references
        r(string sub () { code c = \k(); return c(t); }),
        r(string sub () { code c = \n1(); return c(t); }),
        r(string sub () { code c = \p(); return c(t); }),
        r(string sub () { code c = \a(); return c(t); }),
        r(string sub () { code c = \g(); return c(t); }),
        r(string sub () { code c = \U::k(); return c(t); }),
        r(string sub () { code c = \U::p(); return c(t); }),
        r(string sub () { code c = \u.mk(); return c(t); }),
        r(string sub () { code c = \u.mn1(); return c(t); }),
        r(string sub () { code c = \u.ma(); return c(t); }),
        r(string sub () { code c = \u.mg(); return c(t); }),
        # call_function() and call_object_method()
        r(string sub () { return call_function("k", t); }),
        r(string sub () { return call_function("n1", t); }),
        r(string sub () { return call_function("p", t); }),
        r(string sub () { return call_function("a", t); }),
        r(string sub () { return call_function("g", t); }),
        r(string sub () { return call_object_method(u, "mk", t); }),
        r(string sub () { return call_object_method(u, "mn1", t); }),
        r(string sub () { return call_object_method(u, "mp", t); }),
        r(string sub () { return call_object_method(u, "ma", t); }),
        # a child class object
        r(string sub () { return k(new Child()); }),
    );
}

#! the results for the other calls, which take no deleted object
list<string> sub other_calls() {
    Kid kid();
    return (
        # an argument that only the variant without type parameters accepts
        r(string sub () { return g<Obj>(1); }),
        r(string sub () { return U::g<Obj>(1); }),
        r(string sub () { U u(); return u.mg<Obj>(1); }),
        # a method of a child class beside a generic method of its parent class
        r(string sub () { return kid.m<Obj>(new Obj()); }),
    );
}

#! one line per call: the result for the live object, then the result for the deleted object
string sub probes() {
    Obj live();
    Obj gone();
    Obj d = gone;
    delete gone;
    list<string> l = calls(live);
    list<string> ld = calls(d);
    list<string> rv = ();
    for (int i = 0; i < l.size(); ++i) {
        rv += sprintf("%d:%s/%s", i, l[i], ld[i]);
    }
    rv += other_calls();
    return rv.join(" ");
}
