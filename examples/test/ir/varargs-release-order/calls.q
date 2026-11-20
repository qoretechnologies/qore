#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The calls of varargs-release-order.qtest: a function with a parameter and extra arguments ($argv) whose local's
# destructor raises an exception after its return statement, called in every call form; the destructors log the order
# in which the extra argument (x), the return value (t) and the parameter (p) are released (qore#6052)

%modern

our list<string> log = ();

class T {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
}

class D {
    destructor() { throw "DX"; }
}

auto sub fv(T p, ...) { D dd(); return new T("t"); }

auto sub gv(T p, ...) { return new T("t"); }

class C {
    static auto sfv(T p, ...) { D dd(); return new T("t"); }
    auto mfv(T p, ...) { D dd(); return new T("t"); }
}

sub run(string name, code c) {
    log = ();
    try {
        c();
    } catch () {
        log += "c";
    }
    printf("%s: %y\n", name, log);
}

# twice, so that a function that native code runs from its second call is run there too
for (int i = 0; i < 2; ++i) {
    run("direct", sub () { fv(new T("p"), new T("x")); });
    run("static method", sub () { C::sfv(new T("p"), new T("x")); });
    run("method", sub () { (new C()).mfv(new T("p"), new T("x")); });
    run("call_function", sub () { call_function(\fv(), new T("p"), new T("x")); });
    run("call_function_args", sub () { call_function_args(\fv(), (new T("p"), new T("x"))); });
    run("call_function by name", sub () { call_function("fv", new T("p"), new T("x")); });
    run("call_function_args by name", sub () { call_function_args("fv", (new T("p"), new T("x"))); });
    run("call reference", sub () { code f = \fv(); f(new T("p"), new T("x")); });
    run("closure", sub () {
        code f = auto sub (T p, ...) { D dd(); return new T("t"); };
        f(new T("p"), new T("x"));
    });
    # without an exception
    run("no exception, direct", sub () { gv(new T("p"), new T("x")); });
    run("no exception, call_function", sub () { call_function(\gv(), new T("p"), new T("x")); });
    run("no exception, call reference", sub () { code g = \gv(); g(new T("p"), new T("x")); });
}
