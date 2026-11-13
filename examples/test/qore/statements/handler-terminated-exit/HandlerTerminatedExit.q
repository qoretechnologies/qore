# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The exits of a block after a handler lowered inline that throws are not lowered (#6030); see handler-terminated-exit.qtest

%modern
%enable-all-warnings
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
string sub tag(string s) { return s; }
# an on_exit handler lowered inline at a return throws: the handler that the return runs after it, which declares a
# local, is not lowered after the throw
int sub g1() {
    T z("g1-z");
    if (True) {
        on_exit { string s = tag("ox"); log += s; throw "G1-OUTER"; }
        T a("g1-a");
        if (True) {
            on_exit { log += tag("ix"); throw "G1-INNER"; }
            return 1;
        }
    }
}
# a break
sub g2() {
    T z("g2-z");
    for (int i = 0; i < 2; ++i) {
        on_exit { string s = tag("ox" + i); log += s; }
        if (True) {
            on_exit { log += tag("ix" + i); if (i == 1) throw "G2"; }
            if (i == 1) break;
        }
    }
}
# a continue, and a return without a value
sub g3() {
    for (int i = 0; i < 2; ++i) {
        on_exit { string s = tag("ox" + i); log += s; }
        if (True) {
            on_exit { log += tag("ix" + i); throw "G3"; }
            continue;
        }
    }
}
sub g4() {
    T z("g4-z");
    if (True) {
        on_exit { string s = tag("ox"); log += s; }
        if (True) {
            on_exit { log += tag("ix"); throw "G4"; }
            return;
        }
    }
}
# the value of a return that a handler's throw or thread_exit interrupts is still released after the locals, and
# before the catch block of a try statement of the function that catches the exception (qore#5920, qore#5941)
auto sub r1() {
    T z("r1-z");
    if (True) {
        on_exit { string s = tag("ox"); log += s; }
        if (True) {
            on_exit { log += "ix"; throw "R1"; }
            return new T("r1-rv");
        }
    }
}
auto sub r2() {
    T z("r2-z");
    if (True) {
        on_exit { string s = tag("ox"); log += s; }
        if (True) {
            on_exit { log += "ix"; thread_exit; }
            return new T("r2-rv");
        }
    }
}
auto sub r3() {
    T z("r3-z");
    try {
        for (int i = 0; i < 2; ++i) {
            on_exit { log += "ix" + i; throw "R3"; }
            return new T("r3-rv" + i);
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 1;
}

foreach string fn in (("g1", "g2", "g3", "g4", "r1", "r3")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        try {
            call_function(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y\n", fn, log);
    }
}
# the thread_exit in a thread of its own
for (int i = 0; i < 2; ++i) {
    log = ();
    Counter c(1);
    background sub () {
        on_exit c.dec();
        r2();
    }();
    c.waitForZero();
    printf("r2: %y\n", log);
}
