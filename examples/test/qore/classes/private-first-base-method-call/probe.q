#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a call of a method resolved at run time skips the methods that the caller cannot call, as the parse-time lookup
# does (qore#6704); the test compares the output of each execution mode and of qcc executables with probe.expected

%modern
%disable-warning non-existent-method-call

class A {
    private string m() { return "A::m"; }
    private static string s() { return "A::s"; }
}

class B {
    string m() { return "B::m"; }
    static string s() { return "B::s"; }
}

class X inherits A, B {
    string inner() { return m(); }
    string innerSelf() { return self.m(); }
    string innerObj(object o) { return o.m(); }
    string innerStatic(object o) { return o.s(); }
    string innerRef(object o) { code c = \o.m(); return c(); }
}

class Y inherits X {
    string fromChild(object o) { return o.m(); }
}

class Z inherits X {
    string m() { return "Z::m"; }
}

class AI {
    private:internal string m() { return "AI::m"; }
    string fromAI(object o) { return o.m(); }
}
class XI inherits AI, B {
    string inner() { object o = self; return o.m(); }
}

class AH {
    private:hierarchy string m() { return "AH::m"; }
}
class XH inherits AH, B {
    string inner() { object o = self; return o.m(); }
}

class A2 {
    private string m() { return "A2::m"; }
}
class XP inherits A, A2 {
    string inner() { object o = self; return o.m(); }
}

class PubA {
    string m() { return "PubA::m"; }
}
class XPI inherits private PubA, B {
    string inner() { object o = self; return o.m(); }
}

class AA {
    abstract string m();
}
class XAB inherits A, AA, B {}
class XBA inherits AA, A, B {}

class G {
    private string m() { return "G::m"; }
    string methodGate(string n) { return "gate:" + n; }
}
class GX inherits A, G {}

class GB {
    string methodGate(string n) { return "gate:" + n; }
}
class GBX inherits A, GB {}

# a private override of a public method of a parent class hides it (qore#6062)
class Parent {
    string m() { return "Parent::m"; }
}
class PrivateChild inherits Parent {
    private string m() { return "PrivateChild::m"; }
}
class PrivateChildB inherits PrivateChild, B {}

# a diamond: the private method of the first path, the public one of the second
class DTop {
    string other() { return "DTop::other"; }
}
class DLeft inherits DTop {
    private string m() { return "DLeft::m"; }
}
class DRight inherits DTop {
    string m() { return "DRight::m"; }
}
class XD inherits DLeft, DRight {
    string inner() { object o = self; return o.m(); }
}

# a private normal method and a public static method of the name
class SP {
    private string m() { return "SP::m"; }
}
class SS {
    static string m() { return "SS::m"; }
}
class XS inherits SP, SS {}

# the iterator methods of the functional operators
class PI {
    private bool next() { return False; }
    private auto getValue() { return "PI::getValue"; }
}
class It inherits PI, ListIterator {
    constructor() : ListIterator((1, 2, 3)) {}
    list<auto> inner() { list<auto> l = map $1, self; return l; }
}
class PrivateIt inherits ListIterator {
    constructor() : ListIterator((1, 2, 3)) {}
    private bool next() { return False; }
}

class Unrelated {
    string t(object o) { return o.m(); }
    string tt(X x) { return x.m(); }
    string ts(object o) { return o.s(); }
    string tr(object o) { code c = \o.m(); return c(); }
}

sub check(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# the issue: the private method of the first parent class is skipped from outside the hierarchy
check("typed", string sub () { X x = new X(); return x.m(); });
check("untyped", string sub () { object o = new X(); return o.m(); });
check("typed, object of a child class", string sub () { X x = new Y(); return x.m(); });
check("untyped, object of a child class", string sub () { object o = new Y(); return o.m(); });
check("call_object_method()", string sub () { object o = new X(); return call_object_method(o, "m"); });
check("call_object_method_args()", string sub () { object o = new X(); return call_object_method_args(o, "m"); });
# static methods called through an object
check("static, typed", string sub () { X x = new X(); return x.s(); });
check("static, untyped", string sub () { object o = new X(); return o.s(); });
# from inside the hierarchy the private method is accessible and is called
check("inside, plain call", string sub () { return (new X()).inner(); });
check("inside, self call", string sub () { return (new X()).innerSelf(); });
check("inside, untyped", string sub () { X x(); return x.innerObj(x); });
check("inside, untyped child object", string sub () { X x(); return x.innerObj(new Y()); });
check("inside, static untyped", string sub () { X x(); return x.innerStatic(x); });
check("inside, reference", string sub () { X x(); return x.innerRef(x); });
check("child class, untyped", string sub () { Y y(); return y.fromChild(y); });
# from an unrelated class
check("unrelated class, untyped", string sub () { return (new Unrelated()).t(new X()); });
check("unrelated class, typed", string sub () { return (new Unrelated()).tt(new X()); });
check("unrelated class, static", string sub () { return (new Unrelated()).ts(new X()); });
check("unrelated class, reference", string sub () { return (new Unrelated()).tr(new X()); });
# an override in a child class is called
check("override", string sub () { object o = new Z(); return o.m(); });
# private:internal
check("private:internal, untyped", string sub () { object o = new XI(); return o.m(); });
check("private:internal, inside the child", string sub () { return (new XI()).inner(); });
check("private:internal, inside its class", string sub () { XI x(); return x.fromAI(x); });
# private:hierarchy
check("private:hierarchy, untyped", string sub () { object o = new XH(); return o.m(); });
check("private:hierarchy, inside", string sub () { return (new XH()).inner(); });
# private in every parent class
check("all private, untyped", string sub () { object o = new XP(); return o.m(); });
check("all private, reference", string sub () { object o = new XP(); code c = \o.m(); return c(); });
check("all private, inside", string sub () { return (new XP()).inner(); });
# a public method of a privately inherited class
check("private inheritance, typed", string sub () { XPI x(); return x.m(); });
check("private inheritance, untyped", string sub () { object o = new XPI(); return o.m(); });
check("private inheritance, inside", string sub () { return (new XPI()).inner(); });
# a private override of a parent class's public method
check("private override, parent type", string sub () { Parent p = new PrivateChild(); return p.m(); });
check("private override, untyped", string sub () { object o = new PrivateChild(); return o.m(); });
check("private override, reference", string sub () { object o = new PrivateChild(); code c = \o.m(); return c(); });
check("private override, later parent", string sub () { object o = new PrivateChildB(); return o.m(); });
# a diamond hierarchy
check("diamond, typed", string sub () { XD x(); return x.m(); });
check("diamond, untyped", string sub () { object o = new XD(); return o.m(); });
check("diamond, inside", string sub () { return (new XD()).inner(); });
check("diamond, other method", string sub () { object o = new XD(); return o.other(); });
# a static method after a private normal method
check("static after private, typed", string sub () { XS x(); return x.m(); });
check("static after private, untyped", string sub () { object o = new XS(); return o.m(); });
# hasCallableMethod() answers as the call does
check("hasCallableMethod()", string sub () {
    object o = new X();
    return sprintf("%y %y %y %y %y", o.hasCallableMethod("m"), o.hasCallableNormalMethod("m"),
        o.hasCallableStaticMethod("s"), (new XP()).hasCallableMethod("m"), (new PrivateChild()).hasCallableMethod("m"));
});
# abstract methods implemented by a later parent class
check("abstract, typed", string sub () { XAB x(); return x.m(); });
check("abstract, untyped", string sub () { object o = new XAB(); return o.m(); });
check("abstract first, typed", string sub () { XBA x(); return x.m(); });
check("abstract first, untyped", string sub () { object o = new XBA(); return o.m(); });
# methodGate()
check("methodGate, private method, typed", string sub () { G g(); return g.m(); });
check("methodGate, private method, untyped", string sub () { object o = new G(); return o.m(); });
check("methodGate, private methods, typed", string sub () { GX g(); return g.m(); });
check("methodGate, private methods, untyped", string sub () { object o = new GX(); return o.m(); });
check("methodGate, other name, untyped", string sub () { object o = new GX(); return o.other(); });
check("methodGate in second parent, untyped", string sub () { object o = new GBX(); return o.m(); });
# the functional operators over an iterator call the iterator methods that a call of them makes
check("iterator, typed calls", string sub () {
    It i();
    list<auto> l;
    while (i.next()) {
        l += i.getValue();
    }
    return sprintf("%y", l);
});
check("iterator, untyped calls", string sub () {
    object i = new It();
    list<auto> l;
    while (i.next()) {
        l += i.getValue();
    }
    return sprintf("%y", l);
});
check("iterator, map", string sub () { return sprintf("%y", map $1, new It()); });
check("iterator, foldl", string sub () { return sprintf("%y", foldl $1 + $2, new It()); });
check("iterator, select", string sub () { return sprintf("%y", select new It(), $1 > 1); });
check("iterator, foreach", string sub () {
    list<auto> l;
    foreach auto v in (new It()) {
        l += v;
    }
    return sprintf("%y", l);
});
check("iterator, map inside", string sub () { return sprintf("%y", (new It()).inner()); });
check("iterator, private override, map", string sub () { return sprintf("%y", map $1, new PrivateIt()); });
# pseudo-methods
check("pseudo-method", string sub () { object o = new X(); return o.className(); });
# call references through an untyped receiver
check("reference, untyped", string sub () { object o = new X(); code c = \o.m(); return c(); });
check("reference, untyped child object", string sub () { object o = new Y(); code c = \o.m(); return c(); });
check("reference, typed child object", string sub () { X x = new Y(); code c = \x.m(); return c(); });
check("reference, static untyped", string sub () { object o = new X(); code c = \o.s(); return c(); });
check("reference, private inheritance", string sub () { object o = new XPI(); code c = \o.m(); return c(); });
# repeated calls through the same call sites (caches)
check("repeated", string sub () {
    string rv;
    foreach object o in ((new X(), new Y(), new Z(), new X())) {
        rv += o.m() + " " + o.s() + " ";
    }
    return rv;
});
