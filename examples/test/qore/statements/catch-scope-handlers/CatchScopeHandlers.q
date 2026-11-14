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

# a handler in a reference foreach in a catch block: the catch block is left after the foreach releases its values
sub w1() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch () {
            list<auto> li = (new D("e0"),);
            foreach auto x in (\li) {
                on_exit { try { rethrow; } catch (hash<ExceptionInfo> r) { log += "oh:" + r.err; } }
                x = new D("e1");
                throw "B";
            }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub w2() {
    list<auto> li = (1,);
    try {
        on_exit { log += "enclosing"; }
        try { throw "A", "", new D("arg"); } catch () {
            foreach auto x in (\li) {
                on_exit { log += "inner"; }
                x = new D("w");
                throw "B";
            }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    remove li;
}
sub w3i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch () {
        list<auto> li = (new D("h0"),);
        foreach auto x in (\li) {
            on_exit { log += "oh"; }
            x = new D("h1");
            thrower();
        }
    }
}
sub w3() {
    try { w3i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a call that raises in a nested catch block before the handler of the enclosing catch block is registered
sub c1() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch () {
            try { throw "N", "", new D("n"); } catch () { thrower(); }
            on_exit { log += "oh"; }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub c2i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch () {
        try { throw "N", "", new D("n"); } catch () { thrower(); }
        on_exit { log += "oh"; }
    }
}
sub c2() {
    try { c2i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# without handlers
sub c3() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch () {
            try { throw "N", "", new D("n"); } catch () { thrower(); }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# the catch variable is released with the catch block, before the handlers and locals of the enclosing blocks
sub v1i() {
    on_exit log += "fh";
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
        on_exit { log += "h"; }
        throw "B";
    }
}
sub v1() {
    try { v1i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub v2() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
            on_exit { log += "h"; }
            throw "B";
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub v3() {
    try {
        D loc("local");
        try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
            thrower();
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}

# a statement of a catch block that can raise (whose exception goes to the catch block's exit) after a try block whose
# partial removal uses the values of its selectors on its exception path
our auto nothing_bound;
int sub sel(string s, int v) { log += "sel " + s; return v; }
sub s1() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        remove m[sel("a", 1)..sel("b", 2), nothing_bound, sel("c", 4)..nothing_bound];
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    log += sprintf("%y", m);
}

# a catch block with only simple statements in no exception region of its frame
sub x1i(int k) {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
        if (k == 0) throw "B";
        if (k == 1) rethrow;
        rethrow "A2", "";
    }
}
sub x1() {
    for (int k = 0; k < 3; ++k) {
        try { x1i(k); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    }
}
sub x2i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) {
        throw "B";
    }
}
sub x2() {
    try { x2i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}

# a call that raises an exception in a catch block with only simple statements in no exception region of its frame:
# the catch block is left with the frame, before its locals are released
sub k1i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { thrower(); }
}
sub k1() {
    try { k1i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub k2i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch () { thrower(); }
}
sub k2() {
    try { k2i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub k3i() {
    D loc("local");
    {
        D inner("inner");
        try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { log += "c:" + ex.err; thrower(); }
    }
}
sub k3() {
    try { k3i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub k4i() {
    D loc("local");
    for (int i = 0; i < 3; ++i) {
        try { throw "A" + i, "", new D("arg" + i); } catch (hash<ExceptionInfo> ex) { log += "c:" + ex.err; }
        if (i == 1) {
            try { throw "B" + i, "", new D("b" + i); } catch (hash<ExceptionInfo> ex) { thrower(); }
        }
    }
}
sub k4() {
    try { k4i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}

# a thread_exit in a catch block with only simple statements in no exception region of its frame
sub k5i() {
    D loc("local");
    try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { thread_exit; }
}
sub k5() {
    Counter c(1);
    background sub () { on_exit c.dec(); k5i(); }();
    c.waitForZero();
}
# a call that raises an exception in a catch block with only simple statements in a try block: the catch block is
# left before the exception is caught, and before the locals of the blocks around it are released
sub k6() {
    try {
        try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { thrower(); }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    log += "after";
}
sub k7() {
    try {
        D loc("local");
        {
            D inner("inner");
            try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { thrower(); }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
# a thread_exit in a catch block with only simple statements in a loop in a try block
sub k8i() {
    try {
        D loc("local");
        while (True) {
            try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { thread_exit; }
        }
    } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
}
sub k8() {
    Counter c(1);
    background sub () { on_exit c.dec(); k8i(); }();
    c.waitForZero();
}
# a statement of a catch block that can raise in a try statement in another try block, after a partial removal
sub s2() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        try {
            remove m[sel("a", 1)..sel("b", 2), nothing_bound, sel("c", 4)..nothing_bound];
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "outer:" + ex.err;
    }
    log += sprintf("%y", m);
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
        case "w1": w1(); break;
        case "w2": w2(); break;
        case "w3": w3(); break;
        case "c1": c1(); break;
        case "c2": c2(); break;
        case "c3": c3(); break;
        case "v1": v1(); break;
        case "v2": v2(); break;
        case "v3": v3(); break;
        case "s1": s1(); break;
        case "x1": x1(); break;
        case "x2": x2(); break;
        case "k1": k1(); break;
        case "k2": k2(); break;
        case "k3": k3(); break;
        case "k4": k4(); break;
        case "k5": k5(); break;
        case "k6": k6(); break;
        case "k7": k7(); break;
        case "k8": k8(); break;
        case "s2": s2(); break;
    }
}
foreach string fn in (("h1", "h2", "h3", "h4", "h5", "n1", "n2", "n3", "n4", "d1", "d2", "e1", "f1", "f2", "t1", "o1",
        "o2", "a1", "r1", "r2", "r3", "r4", "w1", "w2", "w3", "c1", "c2", "c3", "v1", "v2", "v3", "s1",
        "x1", "x2", "k1", "k2", "k3", "k4", "k5", "k6",
        "k7", "k8", "s2")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        call(fn);
        printf("%s: %y\n", fn, log);
    }
}
