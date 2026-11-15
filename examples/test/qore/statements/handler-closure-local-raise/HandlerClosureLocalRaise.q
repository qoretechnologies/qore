#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# a closure-bound local that a handler declares is released at the end of its block when the handler raises, before
# the handlers that run after it (#6012); the output is that of the AST interpreter in every execution mode and
# compiled with qcc

%modern

our list<string> log;
our list<code> keep;

class T {
    public { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
}

T sub mk(string n) { return new T(n); }

# a reference takes the local
sub f1() {
    on_exit { log += "x"; }
    on_error { T z = mk("z1"); reference<T> r = \z; throw "H"; }
    throw "E";
}

# a closure that the handler creates and calls takes the local
sub f2() {
    on_exit { log += "x"; }
    on_error { T z = mk("z2"); code c = auto sub () { return z; }; c(); throw "H"; }
    throw "E";
}

# the handler raises before the declaration; the function has a closure-bound local of its own
sub f3() {
    T fz = mk("f3"); reference<T> rf = \fz;
    on_exit { log += "x"; }
    on_error { if (True) throw "H"; T hz = mk("z3"); reference<T> r = \hz; }
    throw "E";
}

# a closure that outlives the handler keeps the local
sub f4() {
    on_exit { log += "x"; }
    on_error { T z = mk("z4"); keep += auto sub () { return z; }; throw "H"; }
    throw "E";
}

# a loop in the handler
sub f5() {
    on_exit { log += "x"; }
    on_error { for (int i = 0; i < 2; ++i) { T z = mk("z5-" + i); reference<T> r = \z; if (i) throw "H"; } }
    throw "E";
}

# nested blocks in the handler
sub f6() {
    on_exit { log += "x"; }
    on_error { T y = mk("y6"); reference<T> ry = \y; { T z = mk("z6"); reference<T> r = \z; throw "H"; } }
    throw "E";
}

# a handler that does not raise
sub f7() {
    on_exit { log += "x"; }
    on_error { T z = mk("z7"); reference<T> r = \z; log += "h"; }
    throw "E";
}

# a recursive call whose handler raises
sub f8(int n) {
    on_exit { log += "x" + n; }
    on_error { T z = mk("z8-" + n); reference<T> r = \z; throw "H" + n; }
    if (n) {
        f8(n - 1);
    }
    throw "E" + n;
}

# an on_exit handler that raises without an exception before it
sub f9() {
    on_exit { log += "x"; }
    on_exit { T z = mk("z9"); reference<T> r = \z; throw "H"; }
    log += "body";
}

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
    run("f1", \f1());
    run("f2", \f2());
    run("f3", \f3());
    run("f4", \f4());
    log = ();
    keep = ();
    printf("f4 kept: %y\n", log);
    run("f5", \f5());
    run("f6", \f6());
    run("f7", \f7());
    run("f8", sub () { f8(1); });
    run("f9", \f9());
}
