# Copyright 2026 Qore Technologies, s.r.o.

# the IR that a qcc executable builds for a generic function once a debugger is attached resolves the
# type parameters of the function (qore#5885)
%modern
%allow-debugger

class Stepper inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int block, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugStep;
    }
    onExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) { rs = DebugStep; }
}

sub show(string label, code c) {
    try {
        printf("%s => %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s: %s\n", label, ex.err, ex.desc);
    }
}

int sub optLocal<T>(T x, auto y) {
    *T z = x;
    z = y;
    return 1;
}

int sub onExitLocal<T>(auto y) {
    {
        on_exit {
            *T z;
            z = y;
        }
    }
    return 1;
}

int sub innerTyped<T>(auto y) {
    *T z;
    z = y;
    return 1;
}

int sub outerCall<T>(auto y) {
    return innerTyped<T>(y);
}

sub runCases() {
    show("debug forwarded *string float", auto sub () { return outerCall<string>(2.5); });
    show("debug forwarded *string string", auto sub () { return outerCall<string>("s"); });
    show("debug forwarded *int string", auto sub () { return outerCall<int>("s"); });
    show("debug *string float", auto sub () { return optLocal<string>("a", 2.5); });
    show("debug *string string", auto sub () { return optLocal<string>("a", "b"); });
    show("debug on_exit *string int", auto sub () { return onExitLocal<string>(1); });
    show("debug on_exit *string string", auto sub () { return onExitLocal<string>("s"); });
}

Stepper stepper();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
background sub () {
    on_exit done.dec();
    stepper.addProgram(pc);
    on_exit stepper.removeProgram(pc);
    call_function("runCases");
}();
done.waitForZero();
