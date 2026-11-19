#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# handlers run out of line that read some locals of their function and write others directly and through a reference
# (qore#6314): the values and the order of the destructors must be those of the AST in every mode

%modern

our string log;

class D {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
    string get() { return n; }
}

int sub may_throw(int i) { if (i < 0) { throw "NEG", "negative"; } return i; }

# handlers that run out of line, as an exception leaves their block: they read k and keep, write w directly and v
# through a reference
string sub handlers(int n) {
    int k = n + 2;
    D keep = new D("hk");
    D w = new D("w0");
    D v = new D("v0");
    auto rv = \v;
    string out;
    try {
        on_exit { out += sprintf("x%d:%s", k, keep.get()); }
        on_error { w = new D("w1"); rv = new D("v1"); log += "|e"; }
        may_throw(-k);
    } catch (hash<ExceptionInfo> ex) {
        out += "|caught";
    }
    return sprintf("%s:%s:%s:%s", out, keep.get(), w.get(), v.get());
}

for (int c = 0; c < 2; ++c) {
    log = "";
    string v = handlers(c);
    printf("handlers %d: %s %s\n", c, v, log);
}
