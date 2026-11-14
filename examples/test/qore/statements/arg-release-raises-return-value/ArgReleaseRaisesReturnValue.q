#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the value of a call is released when the release of an argument raises an exception after the call returned it
# (#6051): the argument is an object that only the parameter refers to, whose destructor raises; the script counts the
# live objects; the output is that of the AST interpreter in every execution mode and compiled with qcc

%modern

our list<string> log = ();
our int live = 0;

class T {
    private { string n; }
    constructor(string n) { self.n = n; ++live; }
    destructor() { --live; log += "~" + n; }
}

class P {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; throw "PX"; }
}

T sub mk(string n) { return new T(n); }

class C {
    auto m(P p) { return mk("m-t"); }
    static auto s(P p) { return mk("s-t"); }
}

auto sub f(P p) { return mk("f-t"); }
auto sub fl(P p) { T t("fl-t"); return t; }
T sub ft(P p, int x) { return mk("ft-t"); }
code cl = auto sub (P p) { return mk("cl-t"); };

sub run(string k, code c) {
    log = ();
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    printf("%s: %y live=%d\n", k, log, live);
}

sub df() { f(new P("df-p")); }
sub dfl() { fl(new P("dfl-p")); }
sub dft() { ft(new P("dft-p"), 1); }
sub das() { auto v = f(new P("das-p")); log += "after"; }

for (int i = 0; i < 3; ++i) {
    C o();
    # calls from functions: a returned call value, a returned local, typed parameters, an assigned value
    run("df", \df());
    run("dfl", \dfl());
    run("dft", \dft());
    run("das", \das());
    # calls from closures: a function, a method, a static method, a closure and a call reference (a method call
    # reference is not here: tiered execution releases its value before the argument without an exception, #6053)
    run("f", sub () { f(new P("f-p")); });
    run("m", sub () { o.m(new P("m-p")); });
    run("s", sub () { C::s(new P("s-p")); });
    run("cl", sub () { cl(new P("cl-p")); });
    run("cr", sub () { call_function(\f(), new P("cr-p")); });
}
