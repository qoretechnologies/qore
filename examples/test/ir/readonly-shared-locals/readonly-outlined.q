#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# outlined helpers that read some locals of their function and write others directly, through a reference and through
# a closure (qore#6314): the values and the order of the destructors must be those of the AST in every mode; qcc
# outlines the function with the thresholds of the test

%modern

our string log;

class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
    string get() { return n; }
}

int sub may_throw(int i) { if (i < 0) { throw "NEG", "negative"; } return i; }

# a function that qcc outlines with the thresholds of the test: the regions read k and keep, write r directly, b
# through a reference created before them and c through a closure
string sub outlined(int n) {
    int r = 0;
    int k = n + 1;
    D keep = new D("keep");
    D b = new D("b0");
    D c = new D("c0");
    auto rb = \b;
    code set_c = sub (string v) { c = new D(v); };
    for (int i = 0; i < 3; ++i) {
        try {
            r += may_throw(k) + keep.get().size();
        } catch (hash<ExceptionInfo> ex) {
            r -= 1;
        }
        # the function reads b and c before the helpers that write them, so that its own storage holds their values
        log += "<" + b.get() + c.get();
        try {
            rb = new D("b" + (i + 1));
            log += "|b";
        } catch (hash<ExceptionInfo> ex) {
            r -= 2;
        }
        log += "<" + c.get();
        try {
            set_c("c" + (i + 1));
            log += "|c";
        } catch (hash<ExceptionInfo> ex) {
            r -= 3;
        }
        r += k;
    }
    return sprintf("%d:%s:%s:%s", r, keep.get(), b.get(), c.get());
}

for (int c = 0; c < 2; ++c) {
    log = "";
    string v = outlined(c);
    printf("outlined %d: %s %s\n", c, v, log);
}
