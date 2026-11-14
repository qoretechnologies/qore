# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The local variables of the debug IR of top-level code compiled ahead of time (#6043); see
# aot-toplevel-debug-ir-local-slots.qtest.  A constant attaches a debugger to this Program before its top-level code
# runs; each loop below instantiates a closure-bound local before the locals declared after it in the code are used

%modern
%allow-debugger

class D inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugRun; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int b, *int s, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugRun;
    }
    onFunctionEnter(ProgramControl pgm, int s, reference rs, reference rts) { rs = DebugRun; }
    onFunctionExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) { rs = DebugRun; }
    onException(ProgramControl pgm, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugRun;
    }
    onExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {}
}

our D R;
bool sub attach() {
    R = new D();
    R.addProgram(ProgramControl::getProgram());
    return True;
}
const Attached = attach();

sub t(string err) { throw err; }
sub u() {}

# a foreach variable and a catch variable, with a background closure after them (the shape of #6043)
for (int k = 0; k < 2; ++k) {
    foreach code f in ((\t(),)) {
        try { f("A" + k); } catch (hash<ExceptionInfo> ex) { printf("1: %s\n", ex.err); }
    }
    Counter c(1); background sub () { on_exit c.dec(); u(); }(); c.waitForZero();
}

# nested foreach loops, a catch variable used in two try statements, and a block after the closure
int w = 0;
while (w < 2) {
    foreach code g in ((\t(), \t())) {
        foreach int i in ((1, 2)) {
            try { g(sprintf("B%d%d", w, i)); } catch (hash<ExceptionInfo> ex) { printf("2: %s\n", ex.err); }
            try { g(sprintf("C%d%d", w, i)); } catch (hash<ExceptionInfo> ex) { printf("3: %s\n", ex.err); }
        }
    }
    Counter c(1); background sub () { on_exit c.dec(); u(); }(); c.waitForZero();
    {
        string s = sprintf("w=%d", w);
        softint n = w * 2;
        printf("4: %s n=%d\n", s, n);
    }
    ++w;
}

# a closure capturing the foreach variable
for (int k = 0; k < 2; ++k) {
    list<code> l = ();
    foreach code h in ((\t(),)) {
        try { h("D" + k); } catch (hash<ExceptionInfo> ex) { printf("5: %s\n", ex.err); }
        l += sub () { try { h("E" + k); } catch (hash<ExceptionInfo> e2) { printf("6: %s\n", e2.err); } };
    }
    map $1(), l;
    Counter c(1); background sub () { on_exit c.dec(); u(); }(); c.waitForZero();
}
