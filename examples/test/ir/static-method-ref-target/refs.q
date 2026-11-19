#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The references of static-method-ref-target.qtest: static method call references to a method that a class inherits
# from more than one parent class, made where the code is parsed with different access to the parent classes; each
# reference reports the class of the method it calls (qore#6679)

%modern

class A {
    static string m() { return "A"; }
}

class B {
    static string m() { return "B"; }
}

class C inherits private A, B {
    # references made in the class itself
    static code inside() { return \C::m(); }
    static string insideDefault(code r = \C::m()) { return r(); }
}

class D {
    public {
        code r = \C::m();
        static code sr = \C::m();
    }
}

string sub defaultArg(code r = \C::m()) {
    return r();
}

# returns the result of the call, or the error that it raised
string sub tryc(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

printf("direct: %s\n", tryc(string sub () { return call_function(\C::m()); }));
printf("member: %s\n", tryc(string sub () { return (new D()).r(); }));
printf("static member: %s\n", tryc(string sub () { return D::sr(); }));
printf("default argument: %s\n", tryc(\defaultArg()));
printf("in the class: %s\n", tryc(string sub () { return C::inside()(); }));
printf("default argument in the class: %s\n", tryc(\C::insideDefault()));
