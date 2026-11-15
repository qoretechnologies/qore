# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The handlers of a catch block, and of the blocks in it, that an exception leaves run with the catch block's exception
# as the current one, which a rethrow in them raises again, and the catch block is left after them (#5968); see
# catch-scope-handlers.qtest

%modern
%enable-debug
%enable-all-warnings

our list<string> log;
class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "del:" + n; }
}
sub thrower() { throw "B"; }
# the issue's reproducer: a throw in a catch block with an on_exit handler that rethrows
sub h1() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            throw "B";
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# an on_error handler gets the exception that leaves the block, an on_exit handler the caught one
sub h2() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_error { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "eh:" + r.err; } }
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            throw "B";
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a call that raises
sub h3() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a rethrow with new arguments and a plain rethrow
sub h4() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            rethrow "A2", "x";
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub h5() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            rethrow;
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# nested catch blocks, each with handlers, with handlers in the outer one only, and in the inner one only
sub n1() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "h1:" + r.err; } }
            try { throw "C"; } catch (hash<ExceptionInfo> ex2) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "h2:" + r.err; } }
                throw "B";
            }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub n2() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "h1:" + r.err; } }
            try { throw "C"; } catch (hash<ExceptionInfo> ex2) { throw "B"; }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub n3() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            try { throw "C"; } catch (hash<ExceptionInfo> ex2) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "h2:" + r.err; } }
                throw "B";
            }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a handler in a block in the catch block
sub n4() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            if (True) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "h3:" + r.err; } }
                throw "B";
            }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a try statement in no block with handlers: the caught exception (and the object in its argument) is released after
# the handlers and before the locals of the function
sub d1i(bool h) {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
        if (h) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            throw "B";
        }
        throw "B";
    }
}
sub d1() {
    try { d1i(True); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    try { d1i(False); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub d2i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        D cl("catchlocal");
        throw "B";
    }
}
sub d2() {
    try { d2i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a handler of the catch block that throws itself
sub e1() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } throw "H"; }
            throw "B";
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a break, continue and return out of a catch block with handlers
sub f1() {
    while (True) {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            break;
        }
    }
    for (int i = 0; i < 1; ++i) {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            continue;
        }
    }
}
sub f2i() {
    try { throw "A"; } catch (hash<ExceptionInfo> ex) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        return;
    }
}
sub f2() {
    f2i();
    try { throw "O"; } catch (hash<ExceptionInfo> ex) {
        f2i();
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
# a thread_exit in a catch block with handlers
sub t1() {
    try { throw "A"; } catch (hash<ExceptionInfo> ex) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        thread_exit;
    }
}
# an enclosing catch block stays open after the inner one, and its handlers see its exception
sub o1() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        try {
            try { throw "A"; } catch (hash<ExceptionInfo> ex) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "ih:" + r.err; } }
                throw "B";
            }
        } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
        try { rethrow; } catch (hash<ExceptionInfo> r) { log += "after:" + r.err; }
    }
}
sub o2i() {
    try { throw "OUTER"; } catch (hash<ExceptionInfo> o) {
        on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "ih:" + r.err; } }
            throw "B";
        }
    }
}
sub o2() {
    try { o2i(); } catch (hash<ExceptionInfo> e) { log += "caught:" + e.err; }
}
# an assertion that fails in a catch block
sub a1() {
    try {
        try { throw "A"; } catch (hash<ExceptionInfo> ex) {
            on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
            @assert(False);
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}

# a throw in a reference foreach or context statement in a catch block with no handlers closes the catch block before
# it raises the exception: the caught exception is released before the locals of the enclosing blocks
sub r1() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch () {
            list<int> li = (1,);
            foreach int x in (\li) { if (x) throw "B"; }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub r2() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch () {
            hash<auto> h = {"a": (1, 2)};
            context (h) { throw "B"; }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub r3i(bool ctx) {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch () {
        if (ctx) {
            hash<auto> h = {"a": (1, 2)};
            context (h) { throw "B"; }
        }
        list<int> li = (1,);
        foreach int x in (\li) { if (x) throw "B"; }
    }
}
sub r3() {
    try { r3i(False); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    try { r3i(True); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a handler in a reference foreach in a catch block
sub r4() {
    try {
        try { throw "A"; } catch () {
            list<int> li = (1,);
            foreach int x in (\li) {
                x = 2;
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
                throw "B";
            }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each twice, so
# that the second call runs the code that the first one compiled in the tiered mode
sub call(string fn) {
    switch (fn) {
        case "h1": h1(); break;
        case "h2": h2(); break;
        case "h3": h3(); break;
        case "h4": h4(); break;
        case "h5": h5(); break;
        case "n1": n1(); break;
        case "n2": n2(); break;
        case "n3": n3(); break;
        case "n4": n4(); break;
        case "d1": d1(); break;
        case "d2": d2(); break;
        case "e1": e1(); break;
        case "f1": f1(); break;
        case "f2": f2(); break;
        case "t1": {
            Counter c(1);
            background sub () { on_exit c.dec(); t1(); }();
            c.waitForZero();
            break;
        }
        case "o1": o1(); break;
        case "o2": o2(); break;
        case "a1": a1(); break;
        case "r1": r1(); break;
        case "r2": r2(); break;
        case "r3": r3(); break;
        case "r4": r4(); break;
    }
}
foreach string fn in (("h1", "h2", "h3", "h4", "h5", "n1", "n2", "n3", "n4", "d1", "d2", "e1", "f1", "f2", "t1", "o1",
        "o2", "a1", "r1", "r2", "r3", "r4")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        call(fn);
        printf("%s: %y\n", fn, log);
    }
}
