#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the value of a return that an exception raised after it is released before the arguments of the call (#6039): the
# destructor of a local or a handler raises after the return; each argument is a temporary that only the call refers
# to, so its destructor runs when the call releases it; the output is that of the AST interpreter in every execution
# mode and compiled with qcc

%modern

our list<string> log = ();

class T {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
}

class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; throw "DX"; }
}

T sub mk(string n) { return new T(n); }

class C {
    auto m(T p) { D dd("m-d"); return mk("m-t"); }
    static auto s(T p) { D dd("s-d"); return mk("s-t"); }
    T tm(T p, int x) { D dd("tm-d"); return mk("tm-t"); }
}

auto sub f(T p) { D dd("f-d"); return mk("f-t"); }
T sub ft(T p, int x) { D dd("ft-d"); return mk("ft-t"); }
auto sub fq(T p) { T q = p; D dd("fq-d"); return mk("fq-t"); }
auto sub fh(T p) { on_exit throw "HX"; return mk("fh-t"); }
string sub fs(T p) { D dd("fs-d"); return "str"; }
code cl = auto sub (T p) { D dd("cl-d"); return mk("cl-t"); };
auto sub rec(T p, int n) { if (n) return rec(new T("rec-p" + n), n - 1); D dd("rec-d"); return mk("rec-t"); }

sub run(string k, code c) {
    log = ();
    try {
        c();
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    printf("%s: %y\n", k, log);
}

for (int i = 0; i < 3; ++i) {
    C o();
    # a function, a function with typed parameters, and one whose local refers to the argument
    run("f", sub () { f(new T("f-p")); });
    run("ft", sub () { ft(new T("ft-p"), 1); });
    run("fq", sub () { fq(new T("fq-p")); });
    # an on_exit handler raises after the return
    run("fh", sub () { fh(new T("fh-p")); });
    # a value that holds no reference
    run("fs", sub () { fs(new T("fs-p")); });
    # a method, a static method and a method with typed parameters
    run("m", sub () { o.m(new T("m-p")); });
    run("s", sub () { C::s(new T("s-p")); });
    run("tm", sub () { o.tm(new T("tm-p"), 2); });
    # a closure, and call references to a function and a method
    run("cl", sub () { cl(new T("cl-p")); });
    run("cr", sub () { call_function(\f(), new T("cr-p")); });
    run("mr", sub () { call_function(\o.m(), new T("mr-p")); });
    # a self-recursive call
    run("rec", sub () { rec(new T("rec-p"), 1); });
    # the value is assigned
    run("as", sub () { auto v = f(new T("as-p")); log += "after"; });
    # a direct call from a loop body
    log = ();
    try {
        f(new T("lp-p"));
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    printf("lp: %y\n", log);
}
