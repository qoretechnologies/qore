#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# probe of private-first-base-method-ref.qtest (qore#6680): a call reference resolves the method that a call of the
# same name resolves, where a class inherits the name from two public parent classes and the first one's method is not
# accessible

%modern

class A {
    private static string s() { return "A::s"; }
    private string m() { return "A::m"; }
    private:internal string mi() { return "A::mi"; }
    private:hierarchy string mh() { return "A::mh"; }
    private string only() { return "A::only"; }
    private string both() { return "A::both"; }
    private bool val() { return True; }
}

class B {
    static string s() { return "B::s"; }
    string m() { return "B::m"; }
    string mi() { return "B::mi"; }
    string mh() { return "B::mh"; }
    private string both() { return "B::both"; }
    bool val() { return True; }
}

class X inherits A, B {
    # inside the class, the private method of the first parent class is accessible, for a call and for a reference
    string callInside() {
        X x = self;
        return x.m() + "," + X::s();
    }

    string refInside() {
        X x = self;
        code c = \x.m();
        code s = \X::s();
        return c() + "," + s();
    }
}

class Y inherits A, B {
    string m() { return "Y::m"; }
    static string s() { return "Y::s"; }
}

# references in a default argument value and in a member's initial value, which qcc executables store as serialized
# expressions
our X gx = new X();

string sub default_arg(code c = \gx.m()) {
    return c();
}

class H {
    public {
        code c = \gx.m();
    }
}

string sub try_call(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub parse_error(string code) {
    Program p(PO_NEW_STYLE);
    try {
        p.parse(code, "probe");
        return "parsed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

const ParentClasses = "class A { private string only() { return 'A::only'; } "
    "private string both() { return 'A::both'; } } "
    "class B { private string both() { return 'B::both'; } } "
    "class X inherits A, B {} ";

sub main() {
    X x();
    Y y();
    object o = x;
    list<string> l = (
        # the call and the reference from outside the class: the method of the second parent class
        try_call(string sub () { return x.m(); }),
        try_call(string sub () { code c = \x.m(); return c(); }),
        try_call(string sub () { return X::s(); }),
        # a reference in a default argument value and in a member's initial value
        try_call(string sub () { return default_arg(); }),
        try_call(string sub () { H h(); return h.c(); }),
        # a call of a static method through an object of the class (the references to static methods are in
        # probe-static.q)
        try_call(string sub () { return x.s(); }),
        # private:hierarchy and private:internal methods of the first parent class
        try_call(string sub () { return x.mh(); }),
        try_call(string sub () { code c = \x.mh(); return c(); }),
        try_call(string sub () { return x.mi(); }),
        try_call(string sub () { code c = \x.mi(); return c(); }),
        # inside the class: the private method of the first parent class
        x.callInside(),
        x.refInside(),
        # methods of the subclass that override both
        try_call(string sub () { return y.m(); }),
        try_call(string sub () { code c = \y.m(); return c(); }),
        try_call(string sub () { code c = \Y::s(); return c(); }),
        # a method that is private in the first parent class only, or in both, is refused
        try_call(string sub () { code c = \o.only(); return c(); }),
        try_call(string sub () { code c = \o.both(); return c(); }),
        parse_error(ParentClasses + "X x(); code c = \\x.only();"),
        parse_error(ParentClasses + "X x(); code c = \\x.both();"),
        # a receiver with no value: a reference to the pseudo-method of the same name, also where the method is one
        # of a parent class that a qcc executable stores with its classes
        try_call(string sub () { X nx; code c = \nx.val(); return sprintf("%y", c()); }),
    );
    print(l.join("\n") + "\n");
}

main();
