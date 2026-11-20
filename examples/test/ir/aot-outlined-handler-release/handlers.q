#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The functions of aot-outlined-handler-release.qtest: each reassigns locals of the function (and a parameter) in
# on_exit, on_error and on_success handlers that run on an exception, on a return and at the end of their block,
# nested and in a loop; the destructors that run log the order in which the values are released

%modern

string log;

class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
    string get() { return n; }
}

int sub may_throw(int i) { if (i < 0) { throw "NEG", "negative"; } return i; }

# on_error that an exception runs, which reassigns a local of the function
string sub onError(int n) {
    D w = new D("w0");
    try {
        on_error { w = new D("w1"); log += "|e"; }
        may_throw(n - 1);
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a";
    return w.get();
}

# on_error that an exception runs, which reassigns a parameter (whose value only the parameter holds)
string sub paramError(D p, int n) {
    p = new D("p1");
    log += "|b";
    try {
        on_error { p = new D("p2"); log += "|e"; }
        may_throw(n - 1);
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a";
    return p.get();
}

# on_exit that an exception or the end of the block runs
string sub exitThrow(int n) {
    D w = new D("w0");
    try {
        on_exit { w = new D("w1"); log += "|x"; }
        may_throw(n - 1);
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a";
    return w.get();
}

# on_success that the end of the block runs
string sub successOnly(int n) {
    D v = new D("v0");
    try {
        on_success { v = new D("v1"); log += "|s"; }
        may_throw(n - 1);
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a";
    return v.get();
}

# on_exit that a return runs, which reassigns a local and a parameter
string sub onExitReturn(D p, int n) {
    D w = new D("w0");
    p = new D("p1");
    {
        on_exit { w = new D("w1"); p = new D("p2"); log += "|x"; }
        log += "|b" + w.get();
        if (n > 1) {
            return "r" + w.get();
        }
        log += "|n";
    }
    log += "|a" + w.get() + p.get();
    return w.get() + p.get();
}

# on_exit and on_success in a block that an exception leaves
string sub onExitException(int n) {
    D w = new D("w0");
    D v = new D("v0");
    try {
        on_exit { w = new D("w1"); log += "|x"; }
        on_success { v = new D("v1"); log += "|s"; }
        may_throw(n - 1);
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a" + w.get() + v.get();
    return w.get() + v.get();
}

# nested handlers, the inner one reassigning what the outer one reads and reassigns
string sub nested(int n) {
    D w = new D("w0");
    try {
        on_error { log += "|o" + w.get(); w = new D("w2"); }
        {
            on_error { w = new D("w1"); log += "|i"; }
            may_throw(n - 1);
        }
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a";
    return w.get();
}

# a handler in a loop, which reassigns the local in more than one iteration
string sub loop(int n) {
    D w = new D("w");
    int i = 0;
    while (i < 3) {
        try {
            on_error { w = new D("w" + i); log += "|e" + i; }
            may_throw(n - i);
            log += "|n" + i;
        } catch (hash<ExceptionInfo> ex) {
            log += "|c" + i;
        }
        ++i;
    }
    log += "|a";
    return w.get();
}

# a handler with a statement whose locals the outliner cannot collect (a break), which keeps its block out of the
# helpers
string sub breakHandler(int n) {
    D w = new D("w0");
    try {
        on_error {
            for (int i = 0; i < 3; ++i) {
                w = new D("w" + (i + 1));
                if (i == n) {
                    break;
                }
            }
            log += "|e";
        }
        may_throw(n - 1);
        log += "|n";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c";
    }
    log += "|a";
    return w.get();
}

for (int c = 0; c < 3; ++c) {
    log = "";
    string v = onError(c);
    {
        D q = new D("q0");
        log += " ";
        v += ":" + paramError(q, c);
        log += " ";
        v += ":" + onExitReturn(q, c);
    }
    log += " ";
    v += ":" + exitThrow(c);
    log += " ";
    v += ":" + successOnly(c);
    log += " ";
    v += ":" + onExitException(c);
    log += " ";
    v += ":" + nested(c);
    log += " ";
    v += ":" + loop(c);
    log += " ";
    v += ":" + breakHandler(c);
    printf("rv%d:%s %s\n", c, v, log);
}
