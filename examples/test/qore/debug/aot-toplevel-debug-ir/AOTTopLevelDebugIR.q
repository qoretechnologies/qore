# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The debugger's events of top-level code compiled ahead of time (#5851); see aot-toplevel-debug-ir.qtest.  A
# constant attaches the recorder to this Program before its top-level code runs; the recorder records the step events
# with the values of the top-level local variables a and b, and the exception events

%modern
%allow-debugger

class Recorder inherits DebugProgram {
    public {
        list<string> seen = ();
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
        if (l >= FirstLine) {
            hash<auto> tl = pgm.getTopLevelLocalVars();
            seen += sprintf("step@%d a=%y b=%y", l, tl.a.value, tl.b.value);
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
        seen += sprintf("%s@%d", ex.err, i.start_line + i.offset);
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
}

sub thrower(int n) {
    throw "TH", n;
}

sub add(reference<int> r, int n) {
    r += n;
}

our Recorder R;

bool sub attach() {
    R = new Recorder();
    R.addProgram(ProgramControl::getProgram());
    return True;
}
# attaches the debugger before the top-level code runs
const Attached = attach();
# the first line of the top-level code that is recorded
const FirstLine = 68;

int a = 1;
a += 2;
int b = a * 2;
# a closure and a function that change the local variables
code c = sub () {
    a += 10;
};
c();
add(\b, a);
try {
    if (True) {
        throw "TL";
    }
} catch () {
}
# a loop with a caught exception in each iteration
for (int i = 0; i < 2; ++i) {
    try {
        thrower(i);
    } catch () {
        a += i;
    }
}
code d = sub () {
    if (True) {
        throw "CL";
    }
};
try {
    d();
} catch () {
}
R.removeProgram(ProgramControl::getProgram());
printf("a=%d b=%d\n%s\n", a, b, R.seen.join("\n"));
