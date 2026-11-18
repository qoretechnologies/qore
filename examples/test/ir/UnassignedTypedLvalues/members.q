#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): members without a value read in the methods of their class; its
# output in every execution mode and as a qcc executable must be that of the AST interpreter

%modern
class C {
    private { int u; string s; }
    list<auto> t1() { return (u + u,); }
    list<auto> t2() { return (s + s,); }
    list<auto> t3() { return (u,); }
    bool t4() { return s == s; }
}
C c();
foreach code f in ((\c.t1(), \c.t2(), \c.t3(), \c.t4())) {
    try {
        auto v = f();
        printf("%y %s\n", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}
