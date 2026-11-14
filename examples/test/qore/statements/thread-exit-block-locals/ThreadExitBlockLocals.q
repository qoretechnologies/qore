#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# a thread_exit releases the locals of the blocks that it leaves before the on_exit handlers of an enclosing block run,
# as the AST interpreter releases them, and as an exception releases them (#6041); the output is that of the AST
# interpreter in every execution mode and compiled with qcc

%modern

our list<string> log;

class T {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
}

# a thread_exit in a catch block in a block without handlers, in a catch block with an on_exit handler
sub e() {
    try {
        throw "O";
    } catch (hash<ExceptionInfo> o) {
        on_exit { log += "eh"; }
        {
            T d("d");
            try {
                throw "A";
            } catch () {
                T d2("d2");
                thread_exit;
            }
        }
    }
}

# a thread_exit in a block without handlers in a block with an on_exit handler
sub f() {
    if (True) {
        on_exit { log += "fh"; }
        {
            T d("fd");
            thread_exit;
        }
    }
}

# two nested blocks without handlers, and an on_error handler, which a thread_exit does not run
sub g() {
    T a("ga");
    if (True) {
        on_exit { log += "gh"; }
        on_error { log += "ge"; }
        {
            T b("gb");
            {
                T c("gc");
                thread_exit;
            }
        }
    }
}

# the same shape with a throw
sub h() {
    if (True) {
        on_exit { log += "hh"; }
        {
            T d("hd");
            throw "X";
        }
    }
}

# a thread_exit in a loop body
sub l() {
    for (int i = 0; i < 2; ++i) {
        on_exit { log += "lh" + i; }
        {
            T d("ld" + i);
            if (i) {
                thread_exit;
            }
        }
    }
}

for (int i = 0; i < 3; ++i) {
    foreach code c in ((\e(), \f(), \g(), \h(), \l())) {
        log = ();
        Counter cnt(1);
        background sub () {
            on_exit cnt.dec();
            try {
                c();
            } catch (hash<ExceptionInfo> ex) {
                log += "caught:" + ex.err;
            }
        }();
        cnt.waitForZero();
        printf("%y\n", log);
    }
}
