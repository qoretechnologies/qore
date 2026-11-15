# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A catch block that an exception or a thread_exit leaves for a try statement of the same function, or for the caller,
# is left before the handlers of the enclosing blocks and the code after the try statement run (#5969); see
# catch-scope-left.qtest

%modern
%enable-all-warnings

our list<string> log;
sub thrower() { throw "CALL"; }
# the issue's reproducer
sub c1() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        try {
            try { throw "A"; } catch (hash<ExceptionInfo> ex) { thrower(); }
        } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# a handler of the outer try body outside the inner catch block
sub c2() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        try {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "tb:" + r.err; } }
            try { throw "A"; } catch (hash<ExceptionInfo> ex) { thrower(); }
        } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# a handler in the inner catch block sees the inner exception
sub c3() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        try {
            try { throw "A"; } catch (hash<ExceptionInfo> ex) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "ih:" + r.err; } }
                thrower();
            }
        } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# the exception leaves the frame from the inner catch block
sub c4i() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        try { throw "A"; } catch (hash<ExceptionInfo> ex) { thrower(); }
    }
}
sub c4() {
    try { c4i(); } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
}
# a caller's catch block calls a function whose catch block calls one that raises
sub c5() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        try { c4i(); } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# a thread_exit in a catch block
sub t1() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        try {
            try { throw "A"; } catch (hash<ExceptionInfo> ex) { thread_exit; }
        } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        log += "not reached";
    }
}
# a break out of a catch block through a block with an on_exit handler, and through a try block, leaves the catch
# block only: the enclosing catch block stays open
sub b1() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        while (True) {
            on_exit log += "x";
            try { throw "A"; } catch (hash<ExceptionInfo> a) { break; }
        }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
sub b2() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        while (True) {
            try {
                try { throw "A"; } catch (hash<ExceptionInfo> a) { break; }
            } catch (hash<ExceptionInfo> e) { log += "never"; }
        }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# a continue out of a catch block through a try block
sub b3() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        for (int i = 0; i < 2; ++i) {
            try {
                try { throw "A"; } catch (hash<ExceptionInfo> a) { continue; }
            } catch (hash<ExceptionInfo> e) { log += "never"; }
        }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# handlers that the runtime runs without an exception (a return in a catch block of a handler lowered inline) leave
# the catch blocks of the caller open
sub b4i() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        {
            on_exit { }
            on_exit { try { throw "H"; } catch (hash<ExceptionInfo> h) { return; } }
        }
    }
}
sub b4() {
    try { throw "C"; } catch (hash<ExceptionInfo> c) {
        b4i();
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "caller:" + r.err; }
    }
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each twice, so
# that the second call runs the code that the first one compiled in the tiered mode
sub call(string fn) {
    switch (fn) {
        case "c1": c1(); break;
        case "c2": c2(); break;
        case "c3": c3(); break;
        case "c4": c4(); break;
        case "c5": c5(); break;
        case "b1": b1(); break;
        case "b2": b2(); break;
        case "b3": b3(); break;
        case "b4": b4(); break;
        case "t1": {
            Counter c(1);
            background sub () { on_exit c.dec(); t1(); }();
            c.waitForZero();
            break;
        }
    }
}
foreach string fn in (("c1", "c2", "c3", "c4", "c5", "b1", "b2", "b3", "b4", "t1")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        call(fn);
        printf("%s: %y\n", fn, log);
    }
}
