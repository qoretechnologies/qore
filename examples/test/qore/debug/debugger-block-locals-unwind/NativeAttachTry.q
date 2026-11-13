# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A try statement of native code catches an exception after a debugger attaches in its frame (#5954); see
# debugger-block-locals-unwind.qtest

%modern
%allow-debugger
class D inherits DebugProgram {
    public { list<string> seen = (); }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugRun; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int b, *int s, *int bp, reference flow, reference rs, reference rts) { rs = DebugRun; }
    onFunctionEnter(ProgramControl pgm, int s, reference rs, reference rts) { rs = DebugRun; }
    onFunctionExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) { rs = DebugRun; }
    onException(ProgramControl pgm, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugRun;
        hash<auto> i = pgm.getStatementIdInfo(s);
        int l = i.start_line + i.offset;
        log += sprintf("%s@%d", ex.err, l);
    }
    onExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {}
}
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
our *D R;
sub f(bool att) {
    try {
        if (True) {
            on_exit log += "x";
            T a("f");
            if (att && !R) { R = new D(); R.addProgram(ProgramControl::getProgram()); }
            if (True) {
                throw "F";
            }
        }
    } catch () {
        log += "c";
    }
    log += "after";
}
# f() runs as native code in the jit modes when the debugger attaches in its last call
for (int i = 0; i < 30; ++i) {
    log = ();
    try {
        f(i == 29);
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
}
printf("%y\n", log);
