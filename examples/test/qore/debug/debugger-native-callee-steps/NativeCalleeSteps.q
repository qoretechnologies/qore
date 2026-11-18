# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The step events of the calls that native code makes after a debugger attaches in a frame of it (#6289); see
# debugger-native-callee-steps.qtest

%modern
%allow-debugger

#! logs the step events of the functions in the lines of their statements (a block step event as "b<line>") and the
#! function entries ("e<line>")
class StepLogger inherits DebugProgram {
    public {
        list<auto> steps = ();
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int b, *int s, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        steps += s ? line(pgm, s) : "b" + line(pgm, b);
    }
    onFunctionEnter(ProgramControl pgm, int s, reference rs, reference rts) {
        rs = DebugStep;
        steps += "e" + line(pgm, s);
    }
    onFunctionExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {
        rs = DebugStep;
    }
    onException(ProgramControl pgm, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {
    }

    static int line(ProgramControl pgm, int id) {
        hash<auto> i = pgm.getStatementIdInfo(id);
        return i.start_line + i.offset;
    }
}

our *StepLogger logger;

sub attach() {
    logger = new StepLogger();
    logger.addProgram(ProgramControl::getProgram());
}

sub detach() {
    if (logger) {
        logger.removeProgram(ProgramControl::getProgram());
    }
}

int sub callee(bool a) {
    if (a) {
        attach();
    }
    int v = 40;
    return v + 2;
}

# calls callee() again after it attached the debugger
int sub caller(int n, int at) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += callee(i == at);
    }
    detach();
    return total;
}

# calls itself again after it attached the debugger
int sub recurse(int n, int at) {
    if (n == at) {
        attach();
    }
    if (!n) {
        detach();
        return 0;
    }
    return recurse(n - 1, at) + 1;
}

sub run(string label, code c) {
    logger = NOTHING;
    int rv = c();
    printf("%s: %d %y\n", label, rv, logger.steps);
}

# the functions run as native code in the jit modes when the debugger attaches in their last calls
for (int i = 0; i < 3; ++i) {
    caller(2, -1);
    recurse(2, -1);
}
run("callee called again", int sub () { return caller(5, 2); });
run("recursive call", int sub () { return recurse(4, 2); });
