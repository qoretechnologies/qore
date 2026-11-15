# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A break, continue or return out of a catch block releases the caught exception, and the objects in its argument,
# when it leaves the catch block (#6079); see catch-jump-release.qtest

%modern
%enable-all-warnings

our list<string> log;
class T {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
}
# the issue's reproducer: a break out of a catch block in a for loop
sub b1() {
    T z("z");
    for (int i = 0; i < 3; ++i) {
        try { throw "A" + i, "d", new T("A" + i); } catch () { if (i == 2) break; }
    }
    log += "after-loop";
}
# with a handler in the catch block, and with a catch variable
sub b2() {
    T z("z");
    for (int i = 0; i < 3; ++i) {
        try { throw "A" + i, "d", new T("A" + i); } catch (hash<ExceptionInfo> ex) {
            on_exit log += "oe" + i;
            if (i == 2) break;
        }
    }
    log += "after-loop";
}
# continue
sub c1() {
    T z("z");
    for (int i = 0; i < 2; ++i) {
        try { throw "A" + i, "d", new T("A" + i); } catch () { continue; }
        log += "not reached";
    }
    log += "after-loop";
}
# return
sub r1i() {
    try { throw "A", "d", new T("A"); } catch (hash<ExceptionInfo> ex) { return; }
}
sub r1() {
    T z("z");
    r1i();
    log += "after-call";
}
sub r2i() {
    T z("z");
    try { throw "A", "d", new T("A"); } catch () { return; }
}
sub r2() {
    r2i();
    log += "after-call";
}
# nested catch blocks
sub n1() {
    T z("z");
    while (True) {
        try { throw "O", "d", new T("O"); } catch () {
            try { throw "A", "d", new T("A"); } catch () { break; }
        }
    }
    log += "after-loop";
}
# foreach and while loops
sub l1() {
    T z("z");
    foreach int i in (0..2) {
        try { throw "A" + i, "d", new T("A" + i); } catch () { if (i == 1) break; }
    }
    log += "after-foreach";
    int j = 0;
    while (True) {
        try { throw "W" + j, "d", new T("W" + j); } catch () { if (++j == 2) break; continue; }
    }
    log += "after-while";
}

# the catch block is left before the locals of the blocks that enclose it
sub i1() {
    for (int i = 0; i < 1; ++i) {
        T b("b");
        try { throw "A", "d", new T("A"); } catch () { break; }
    }
    log += "after-loop";
}
sub r3i() {
    try { throw "O", "d", new T("O"); } catch () {
        T m("m");
        try { throw "A", "d", new T("A"); } catch (hash<ExceptionInfo> ex) { return; }
    }
}
sub r3() {
    r3i();
    log += "after-call";
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each twice, so
# that the second call runs the code that the first one compiled in the tiered mode
sub call(string fn) {
    switch (fn) {
        case "b1": b1(); break;
        case "b2": b2(); break;
        case "c1": c1(); break;
        case "r1": r1(); break;
        case "r2": r2(); break;
        case "n1": n1(); break;
        case "l1": l1(); break;
        case "i1": i1(); break;
        case "r3": r3(); break;
    }
}
foreach string fn in (("b1", "b2", "c1", "r1", "r2", "n1", "l1", "i1", "r3")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        call(fn);
        printf("%s: %y\n", fn, log);
    }
}
