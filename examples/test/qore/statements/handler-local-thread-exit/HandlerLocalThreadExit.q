#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# a local that an on_error handler declares is released at the end of its block when the handler ends in a
# thread_exit, before the handlers that run after it, as when it throws (#6065); the output is that of the AST
# interpreter in every execution mode and compiled with qcc

%modern

our list<string> log;

class T {
    public { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
}

T sub mk(string n) { return new T(n); }

# a local of the handler
sub t1() {
    on_exit { log += "x"; }
    on_error { T z = mk("z1"); thread_exit; }
    throw "E";
}

# a closure-bound local of the handler (#6012)
sub t2() {
    on_exit { log += "x"; }
    on_error { T z = mk("z2"); reference<T> r = \z; thread_exit; }
    throw "E";
}

# nested blocks in the handler, and a local of the function
sub t3() {
    T a = mk("a3");
    on_exit { log += "x"; }
    on_error { T y = mk("y3"); { T z = mk("z3"); thread_exit; } }
    throw "E";
}

# an on_exit handler without an exception before it
sub t4() {
    on_exit { log += "x"; }
    on_exit { T z = mk("z4"); thread_exit; }
    log += "body";
}

# a thread_exit in the body, with locals in the function's blocks
sub t5() {
    T a = mk("a5");
    on_exit { log += "x"; }
    {
        T z = mk("z5");
        thread_exit;
    }
}

for (int i = 0; i < 3; ++i) {
    foreach hash<auto> h in (("k": "t1", "f": \t1()), ("k": "t2", "f": \t2()), ("k": "t3", "f": \t3()),
            ("k": "t4", "f": \t4()), ("k": "t5", "f": \t5())) {
        code f = h.f;
        log = ();
        Counter cnt(1);
        background sub () {
            on_exit cnt.dec();
            try {
                f();
            } catch (hash<ExceptionInfo> ex) {
                log += "caught:" + ex.err;
            }
        }();
        cnt.waitForZero();
        printf("%s: %y\n", h.k, log);
    }
}
