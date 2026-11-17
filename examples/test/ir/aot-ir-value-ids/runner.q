# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# loads AotIrIdsMod from the module path, runs its closures and prints "result: <results>" or the error; with
# AOT_IR_VALUE_IDS_DEBUGGER set in the environment, a debugger is attached to the Program of the module while they run,
# which makes its AOT code run its IR (debug IR); run by aot-ir-value-ids.qtest (qore#6451); the environment is used
# rather than an option, as an executable compiled with qcc takes the options of qore.  The code runs in a thread of
# its own: if it does not end within RunTimeoutMs (a patched module can make it wait forever), "error: TIMEOUT" is
# printed and the process exits with status 3
%modern
%allow-debugger

const RunTimeoutMs = 60000;

class Attached inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugRun; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
        rs = DebugRun;
    }
    onFunctionEnter(ProgramControl pgm, int statementId, reference rs, reference rts) { rs = DebugRun; }
    onFunctionExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
        rs = DebugRun;
    }
    onException(ProgramControl pgm, int statementId, hash<auto> ex, reference dismiss, reference rs,
            reference rts) {
        rs = DebugRun;
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
        rs = DebugRun;
    }
}

string out;
Counter done(1);
background sub () {
    on_exit done.dec();
    try {
        load_module("AotIrIdsMod");
        if (ENV.AOT_IR_VALUE_IDS_DEBUGGER) {
            # the debugger is attached to the Program of the module, which runs the module's code
            Attached d();
            ProgramControl pc = call_function("AotIrIdsMod::program");
            d.addProgram(pc);
            on_exit d.removeProgram(pc);
            out = sprintf("result: %y\n", call_function("AotIrIdsMod::run"));
        } else {
            out = sprintf("result: %y\n", call_function("AotIrIdsMod::run"));
        }
    } catch (hash<ExceptionInfo> ex) {
        out = sprintf("error: %s: %s\n", ex.err, ex.desc);
    }
}();
if (done.waitForZero(RunTimeoutMs)) {
    printf("error: TIMEOUT: the module's code did not end within %d ms\n", RunTimeoutMs);
    exit(3);
}
print(out);
