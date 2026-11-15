#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of background-deleted-self-call.qtest: a background expression calls a method of the object through self
# after the object was deleted; the thread runs the expression up to the call, which raises OBJECT-ALREADY-DELETED, as
# with the AST interpreter (qore#5973)
# each case joins its thread before the next one starts, so the reports of the threads' exceptions, which they write
# to stderr, do not interleave

%modern

int sub side(string s) { printf("side %s\n", s); return 1; }

class A {
    int am(int x) { printf("A::am(%d)\n", x); return x; }
}

class B inherits A {
    int m(int x) { printf("B::m(%d)\n", x); return x; }
    static int sm(int x) { printf("B::sm(%d)\n", x); return x; }
    # the issue's case: a list whose second element calls the method
    int pre() {
        delete self;
        return background (side("first"), m(side("pre")));
    }
    # the call is an operand of an operator in the argument of a function call
    int op() {
        delete self;
        return background printf("%d\n", side("op") + m(side("op-arg")));
    }
    # the object is alive: the calls are made
    int alive() {
        return background (side("alive"), m(side("alive-arg")));
    }
    # a static method needs no object
    int stat() {
        delete self;
        return background (side("static"), sm(2));
    }
    # a qualified call through self elsewhere in the expression
    int qual() {
        delete self;
        return background (side("qual"), A::am(side("qual-arg")));
    }
    # a copy() call elsewhere in the expression
    int cp() {
        delete self;
        return background (side("copy"), copy());
    }
    # the expression is a call of a static method whose argument calls a method of the object
    int sroot() {
        delete self;
        return background sm(side("sroot") + m(1));
    }
    # the expression is the call: the thread checks the object when it starts
    int root() {
        delete self;
        return background m(side("root"));
    }
}

sub run(string label, code c) {
    thread_join(c());
    printf("%s done\n", label);
}

run("pre", int sub () { B b(); return b.pre(); });
run("op", int sub () { B b(); return b.op(); });
run("alive", int sub () { B b(); return b.alive(); });
run("static", int sub () { B b(); return b.stat(); });
run("qualified", int sub () { B b(); return b.qual(); });
run("copy", int sub () { B b(); return b.cp(); });
run("static root", int sub () { B b(); return b.sroot(); });
run("root", int sub () { B b(); return b.root(); });
