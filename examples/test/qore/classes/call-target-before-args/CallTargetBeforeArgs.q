#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a call checks its target (the object of a method call or of a method call reference, or that a value has the
# method) before it evaluates its arguments, as the AST interpreter does: an argument with side effects is not
# evaluated when the check fails (issue #5784); prints what each case evaluates and raises

%modern

class C {
    m(auto a) {
        printf("  C::m() called\n");
    }
}

class A {
    constructor() {
        printf("  arg evaluated\n");
    }

    destructor() {
        printf("  arg destroyed\n");
    }
}

int sub arg_value() {
    printf("  arg function called\n");
    return 1;
}

sub check(string name, code c) {
    printf("%s:\n", name);
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s: %s\n", ex.err, ex.desc);
    }
}

sub cases() {
    # a method call reference to an object that has been deleted
    check("call reference", sub () { C o(); code r = \o.m(); delete o; r(new A()); });
    check("call reference of object", sub () { object o = new C(); code r = \o.m(); delete o; r(new A()); });
    check("call reference, function argument", sub () { C o(); code r = \o.m(); delete o; r(arg_value()); });
    # a method call on an object deleted through another variable
    check("method call", sub () { C o(); C o2 = o; delete o; o2.m(new A()); });
    check("method call of object", sub () { object o = new C(); object o2 = o; delete o; o2.m(new A()); });
    check("method call of auto", sub () { auto o = new C(); auto o2 = o; delete o; o2.m(new A()); });
    check("method call, function argument", sub () { C o(); C o2 = o; delete o; o2.m(arg_value()); });
    # a method call on no value: the "<nothing>" pseudo-class has no such method
    check("method call on nothing", sub () { C o(); delete o; o.m(new A()); });

    # the same calls with arguments without side effects
    check("call reference, local argument", sub () { C o(); code r = \o.m(); int x = 1; delete o; r(x); });
    check("method call, local argument", sub () { C o(); C o2 = o; int x = 1; delete o; o2.m(x); });
    check("method call on nothing, literal argument", sub () { C o(); delete o; o.m(1); });

    # calls whose target is valid evaluate their arguments and are made
    check("valid call reference", sub () { C o(); code r = \o.m(); r(new A()); });
    check("valid method call", sub () { C o(); o.m(new A()); });
    check("existing pseudo-method", sub () { string s = "abc"; printf("  %y\n", s.substr(arg_value())); });
    check("hash member call reference", sub () {
        hash<auto> h = {"f": sub (auto a) { printf("  closure called\n"); }};
        h.f(new A());
    });
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    cases();
}
