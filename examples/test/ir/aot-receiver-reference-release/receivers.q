#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The functions of aot-receiver-reference-release.qtest: each calls a method through a local that holds a reference
# to the only other holder of the object, which the method removes; the destructor logs where the receiver is released

%modern

string log;

class T {
    public { string n; }
    constructor(string n) { self.n = n; }
    destructor() { log += "~" + n; }
    auto clear() { remove g_h{n}; log += "|m"; return n; }
    auto clearArg(string s) { remove g_h{n}; log += "|m" + s; return n; }
    T clearSelf() { remove g_h{n}; log += "|m"; return self; }
}

our hash<auto> g_h;

string sub mark(auto v) { log += "|p" + v; return v; }

# an untyped foreach variable that holds a reference
sub untyped(list<auto> l) {
    foreach auto r in (l) {
        mark(r.clear());
        log += "|a";
    }
}

# with an argument, and with the result used in the rest of the statement
sub withArgs(list<auto> l) {
    foreach auto r in (l) {
        string s = mark(r.clearArg("x")) + mark("y");
        log += "|a" + s;
    }
}

# a chain: the receiver is kept for the whole chain
sub chain(list<auto> l) {
    foreach auto r in (l) {
        mark(r.clearSelf().n);
        log += "|a";
    }
}

# a local declared with a reference (it is read through the reference on every load: a regression check)
sub refLocal() {
    auto r = \g_h.r;
    mark(r.clear());
    log += "|a";
}

# a method call in the condition of an if statement
sub condition(list<auto> l) {
    foreach auto r in (l) {
        if (mark(r.clear()) == "i") {
            log += "|t";
        }
        log += "|a";
    }
}

# a closure called through a reference: the closure, which holds the object, is released at the end of the
# statement
code sub makeClosure() {
    T o = new T("k");
    return string sub () { remove g_h.k; log += "|m"; return o.n; };
}

sub closure(list<auto> l) {
    foreach auto c in (l) {
        mark(c());
        log += "|a";
    }
}

# in a loop over two references: each receiver is released at the end of its own statement
sub loop(list<auto> l) {
    foreach auto r in (l) {
        mark(r.clear());
        log += "|a" + $#;
    }
}

log = "";
g_h.u = new T("u");
untyped((\g_h.u,));
log += " ";
g_h.w = new T("w");
withArgs((\g_h.w,));
log += " ";
g_h.c = new T("c");
chain((\g_h.c,));
log += " ";
g_h.r = new T("r");
refLocal();
log += " ";
g_h.i = new T("i");
condition((\g_h.i,));
log += " ";
g_h.k = makeClosure();
closure((\g_h.k,));
log += " ";
g_h.l0 = new T("l0");
g_h.l1 = new T("l1");
loop((\g_h.l0, \g_h.l1));
printf("%s\n", log);
