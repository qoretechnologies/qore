#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of background-deleted-self-call.qtest: a background expression calls a method of the object through self
# after the object was deleted; the thread runs the expression up to the call, which raises OBJECT-ALREADY-DELETED, as
# with the AST interpreter (qore#5973)

%modern

# counts down a Counter when it is released, after the call that it is the argument of returns
class Signal {
    private { Counter c; }
    constructor(Counter c) { self.c = c; }
    destructor() { c.dec(); }
}

int sub side(string s, *Signal sig) { printf("side %s\n", s); return 1; }

class A {
    int am(int x, *Signal sig) { printf("A::am(%d)\n", x); return x; }
}

class B inherits A {
    int m(int x, *Signal sig) { printf("B::m(%d)\n", x); return x; }
    static int sm(int x, *Signal sig) { printf("B::sm(%d)\n", x); return x; }
    # the issue's case: a list whose second element calls the method
    pre(Counter c) {
        delete self;
        background (side("first", new Signal(c)), m(side("pre")));
    }
    # the call is an operand of an operator in the argument of a function call
    op(Counter c) {
        delete self;
        background printf("%d\n", side("op", new Signal(c)) + m(side("op-arg")));
    }
    # the object is alive: the calls are made; the Signal is released after the method's call
    alive(Counter c) {
        background (side("alive"), m(side("alive-arg"), new Signal(c)));
    }
    # a static method needs no object
    stat(Counter c) {
        delete self;
        background (side("static"), sm(2, new Signal(c)));
    }
    # a qualified call through self elsewhere in the expression
    qual(Counter c) {
        delete self;
        background (side("qual", new Signal(c)), A::am(side("qual-arg")));
    }
    # a copy() call elsewhere in the expression
    cp(Counter c) {
        delete self;
        background (side("copy", new Signal(c)), copy());
    }
    # the expression is a call of a static method whose argument calls a method of the object
    sroot(Counter c) {
        delete self;
        background sm(side("sroot", new Signal(c)) + m(1));
    }
    # the expression is the call: the thread checks the object when it starts
    root() {
        delete self;
        background m(side("root"));
    }
}

sub run(string label, code c) {
    Counter cnt(1);
    c(cnt);
    # a thread that does not start never releases its Signal
    if (cnt.waitForZero(30s)) {
        printf("%s: the thread did not run\n", label);
        return;
    }
    printf("%s started\n", label);
}

run("pre", sub (Counter c) { B b(); b.pre(c); });
run("op", sub (Counter c) { B b(); b.op(c); });
run("alive", sub (Counter c) { B b(); b.alive(c); });
run("static", sub (Counter c) { B b(); b.stat(c); });
run("qualified", sub (Counter c) { B b(); b.qual(c); });
run("copy", sub (Counter c) { B b(); b.cp(c); });
run("static root", sub (Counter c) { B b(); b.sroot(c); });
B b();
b.root();
