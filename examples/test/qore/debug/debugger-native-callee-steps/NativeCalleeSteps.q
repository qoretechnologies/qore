# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The step events of the calls that native code makes after a debugger attaches in a frame of it (#6289), and the
# exit events of the native frames that were running when it attached (#6602); see debugger-native-callee-steps.qtest

%modern
%allow-debugger

#! logs the step events of the functions in the lines of their statements (a block step event as "b<line>"), the
#! function entries ("e<line>") and the function exits with their results ("x<line>=<result>")
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
        steps += sprintf("x%d=%y", line(pgm, s), result);
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

# calls callee() in a loop and returns the total, with the debugger still attached
int sub loop(int n, int at) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += callee(i == at);
    }
    return total;
}

# calls loop(), which attaches the debugger, and returns after it
int sub outer(int at) {
    int rv = loop(3, at);
    return rv + 1;
}

# raises an exception after it attached the debugger: it makes no exit event, as with the interpreters
int sub thrower(bool a) {
    if (a) {
        attach();
    }
    int v = 1;
    if (v) {
        throw "X";
    }
    return v;
}

# calls thrower() in a loop and catches its exceptions
int sub catcher(int at) {
    int total = 0;
    for (int i = 0; i < 2; ++i) {
        try {
            total += thrower(i == at);
        } catch (hash<ExceptionInfo> ex) {
            total += 10;
        }
    }
    return total;
}

# another Program, whose functions run as native code without a debugger
Program other(PO_NEW_STYLE);
# (its code runs in the execution mode of this Program: as native code in the jit modes)
other.parse("int sub g() { return 40; } int sub f() { return g(); }", "other");

# calls a function of the other Program after it attached the debugger and returns at once: the native return of the
# other Program does not take the exit event of this frame, which has no step event after it
int sub xcallee(bool a) {
    if (a) {
        attach();
    }
    return other.callFunction("f") + 2;
}

# calls xcallee() in a loop
int sub xcaller(int n, int at) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += xcallee(i == at);
    }
    return total;
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
    outer(-1);
    catcher(-1);
    xcaller(2, -1);
}
run("callee called again", int sub () { return caller(5, 2); });
run("recursive call", int sub () { return recurse(4, 2); });
# the native frames of callee(), loop() and outer() that were running when the debugger attached make their exit events
run("exits of the frames running at the attach", int sub () { int rv = outer(1); detach(); return rv; });
# a frame running at the attach that ends with an exception makes no exit event
run("exception", int sub () { int rv = catcher(0); detach(); return rv; });
# the native return of another Program in the thread does not take the exit events of the frames of this one
run("return of another Program", int sub () { int rv = xcaller(2, 0); detach(); return rv; });
