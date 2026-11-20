#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# The functions of weak-receiver-call-lifetime.qtest: each calls a method through a local assigned with a weak
# reference (:=) to an object whose only strong reference is in g_h, which the method removes; the destructor logs
# where the object is released.  The receiver of a running call is kept until the call returns (qore#6590)

%modern
%allow-weak-references

string log;

class T {
    public { string n; bool raise; }
    constructor(string n, bool raise = False) { self.n = n; self.raise = raise; }
    destructor() { log += "~" + n; if (raise) { throw "DTOR", n; } }
    string clear() { remove g_h{n}; log += "|m"; return n; }
    string clearArg(T a) { remove g_h{n}; log += "|m" + a.n; return n; }
    T clearSelf() { remove g_h{n}; log += "|m"; return self; }
    string clearRaise() { remove g_h{n}; log += "|r" + n; throw "ERR", n; }
    string keep() { log += "|k"; return n; }
    static string sclear(T t) { remove g_h{t.n}; log += "|s"; return t.n; }
}

our hash<string, T> g_h;

string sub mark(auto v) { log += "|p" + v; return v; }

string sub q() { log += "|q"; return ""; }

sub put(string n, bool raise = False) { g_h{n} = new T(n, raise); }

sub report(string name) { printf("%s: %s\n", name, log); log = ""; }

# a typed local, outside a try block
sub typed() {
    put("a");
    T w := g_h.a;
    mark(w.clear());
    log += "|e";
}

# a typed local, in a try block (an invoke)
sub inTry() {
    put("a");
    T w := g_h.a;
    try {
        mark(w.clear());
        log += "|e";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
}

# an untyped local
sub untyped() {
    put("a");
    auto w := g_h.a;
    mark(w.clear());
    log += "|e";
}

# with an argument that is a temporary
sub withArg() {
    put("a");
    T w := g_h.a;
    mark(w.clearArg(new T("x")));
    log += "|e";
}

# the method returns the object: the result keeps it
sub chain() {
    put("a");
    T w := g_h.a;
    mark(w.clearSelf().n);
    log += "|e";
}

# in a loop
sub loop() {
    foreach string n in (("a", "b", "c")) {
        put(n);
        T w := g_h{n};
        mark(w.clear());
    }
    log += "|e";
}

# the method raises: the exception releases the receiver
sub raising() {
    put("a");
    T w := g_h.a;
    try {
        mark(w.clearRaise());
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the method raises, with no handler in the function
sub raisingOut() {
    put("a");
    T w := g_h.a;
    mark(w.clearRaise());
}

sub raisingCaller() {
    try {
        raisingOut();
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# a local captured by a closure, which makes the call
sub closure() {
    put("a");
    T w := g_h.a;
    code c = string sub () { return w.clear(); };
    mark(c());
    log += "|e";
}

# a local captured by a closure, which makes the call in an expression that goes on after it returns (qore#6645)
sub closureStatement() {
    put("a");
    T w := g_h.a;
    code c = sub () { mark(w.clear() + q()); log += "|e"; };
    c();
}

# an untyped local captured by a closure, which makes the call in an expression that goes on after it returns
sub closureStatementUntyped() {
    put("a");
    auto w := g_h.a;
    code c = sub () { mark(w.clear() + q()); log += "|e"; };
    c();
}

# a local that a closure assigns with :=, and the function that captures it makes the call (qore#6645)
sub closureAssigns() {
    put("a");
    T w;
    code c = sub () { w := g_h.a; };
    c();
    mark(w.clear() + q());
    log += "|e";
}

# a local captured by a closure, called in the function itself
sub closureLocal() {
    put("a");
    T w := g_h.a;
    code c = string sub () { return w.n; };
    mark(w.clear());
    log += "|e";
    delete c;
}

# a static method that takes the object as its argument
sub staticArg() {
    put("a");
    T w := g_h.a;
    mark(T::sclear(w));
    log += "|e";
}

# the method does not release the object
sub kept() {
    put("a");
    T w := g_h.a;
    mark(w.keep());
    log += "|e";
    remove g_h.a;
    log += "|f";
}

# the object was deleted before the call
sub deleted() {
    put("a");
    T w := g_h.a;
    remove g_h.a;
    try {
        mark(w.keep());
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the destructor of the receiver raises when the call returns
sub dtorRaise() {
    put("a", True);
    T w := g_h.a;
    try {
        mark(w.clear());
        log += "|x";
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the same with no handler in the function
sub dtorRaiseOut() {
    put("a", True);
    T w := g_h.a;
    mark(w.clear());
    log += "|x";
}

sub dtorRaiseCaller() {
    try {
        dtorRaiseOut();
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err;
    }
    log += "|e";
}

# the method and the destructor of the receiver raise
sub dtorRaiseBoth() {
    put("a", True);
    T w := g_h.a;
    try {
        mark(w.clearRaise());
    } catch (hash<ExceptionInfo> ex) {
        log += "|c" + ex.err + (ex.next ? "+" + ex.next.err : "");
    }
    log += "|e";
}

# a function with many statements, whose groups qcc can outline
sub large(int x) {
    put("a");
    T wa := g_h.a;
    if (x > 0) {
        mark(wa.clear());
        log += "|1";
    } else {
        log += "|0";
    }
    for (int i = 0; i < 2; ++i) {
        log += "|i" + i;
    }
    put("b");
    T wb := g_h.b;
    if (x > 1) {
        mark(wb.clear());
        log += "|2";
    } else {
        log += "|0";
    }
    for (int i = 0; i < 2; ++i) {
        log += "|j" + i;
    }
    put("c");
    T wc := g_h.c;
    if (x > 2) {
        try {
            mark(wc.clearRaise());
        } catch (hash<ExceptionInfo> ex) {
            log += "|c" + ex.err;
        }
        log += "|3";
    } else {
        log += "|0";
    }
    for (int i = 0; i < 2; ++i) {
        log += "|k" + i;
    }
    log += "|e";
}

typed();
report("typed");
inTry();
report("inTry");
untyped();
report("untyped");
withArg();
report("withArg");
chain();
report("chain");
loop();
report("loop");
raising();
report("raising");
raisingCaller();
report("raisingCaller");
closure();
report("closure");
closureStatement();
report("closureStatement");
closureStatementUntyped();
report("closureStatementUntyped");
closureAssigns();
report("closureAssigns");
closureLocal();
report("closureLocal");
staticArg();
report("staticArg");
kept();
report("kept");
deleted();
report("deleted");
dtorRaise();
report("dtorRaise");
dtorRaiseCaller();
report("dtorRaiseCaller");
dtorRaiseBoth();
report("dtorRaiseBoth");
large(3);
report("large");
