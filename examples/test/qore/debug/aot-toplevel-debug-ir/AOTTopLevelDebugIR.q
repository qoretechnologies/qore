# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The debugger's events of top-level code compiled ahead of time (#5851); see aot-toplevel-debug-ir.qtest.  The
# recorder is in a Program of its own, which a constant attaches to this Program before its top-level code runs; the
# top-level code has no local variables of its own (see the note in the test)
%modern
%allow-debugger

const RecorderSrc = "
class Recorder inherits DebugProgram {
    public {
        list<string> seen = ();
        int set_line;
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
        rs = DebugStep;
        if (!statementId) {
            return;
        }
        hash<auto> i = pgm.getStatementIdInfo(statementId);
        int l = i.start_line + i.offset;
        if (l >= set_line) {
            seen += sprintf(\"step@%d\", l);
        }
    }
    onFunctionEnter(ProgramControl pgm, int statementId, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
        rs = DebugStep;
    }
    onException(ProgramControl pgm, int statementId, hash<auto> ex, reference dismiss, reference rs,
            reference rts) {
        rs = DebugStep;
        hash<auto> i = pgm.getStatementIdInfo(statementId);
        seen += sprintf(\"%s@%d\", ex.err, i.start_line + i.offset);
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
}
Recorder sub make(int set_line, ProgramControl target) {
    Recorder r();
    r.set_line = set_line;
    r.addProgram(target);
    return r;
}
";

sub thrower(int n) {
    throw "TH", n;
}

our Program DP;
our object R;

bool sub attach() {
    DP = new Program(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    DP.parse(RecorderSrc, "recorder");
    R = DP.callFunction("make", SetLine, ProgramControl::getProgram());
    return True;
}
const SetLine = 72;
# attaches the debugger before the top-level code runs
const Attached = attach();

printf("start\n");
try {
    if (True) {
        throw "TL";
    }
} catch () {
}
our int g = 1;
g += 1;
# a loop with a caught exception in each iteration
for (our int i = 0; i < 2; ++i) {
    try {
        thrower(i);
    } catch () {
    }
}
# a closure created and called in the top-level code
our code c = sub () {
    if (True) {
        throw "CL";
    }
};
try {
    c();
} catch () {
}
printf("events: %y\n", R.seen);
