#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# worker of qmod-subclass-parse.qtest (issue #6106): two threads create and destroy objects of the classes of a
# compiled module while Programs parse and run subclasses of them

%modern
%requires QmodSubclassParseBase

const Programs = 20;

our bool stop = False;

int sub worker() {
    int n = 0;
    while (!stop) {
        QmodSubclassParseBase::A15 a();
        n += a.f();
        QmodSubclassParseBase::A7 b();
        n += b.ab();
    }
    return n;
}

Counter running(2);
Counter started(2);
for (int i = 0; i < 2; ++i) {
    background sub () {
        on_exit running.dec();
        started.dec();
        worker();
    }();
}
started.waitForZero();

int total = 0;
for (int i = 0; i < Programs; ++i) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    p.loadModule("QmodSubclassParseBase");
    p.parse("class X inherits QmodSubclassParseBase::A15 { int f() { return 100; } } "
        "class Y inherits QmodSubclassParseBase::A7 {} "
        "class Z inherits QmodSubclassParseBase::M8 {} "
        "int sub t() { X x(); Y y(); Z z(); return x.f() + y.f() + y.ab() + z.mm8() + x.ab(); }", "subclasses");
    total += p.callFunction("t");
}
stop = True;
running.waitForZero();
# per Program: 100 + 7 + 7 + 8 + 15
printf("total=%d\n", total);
