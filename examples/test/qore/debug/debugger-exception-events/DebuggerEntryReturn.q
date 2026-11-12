#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a debugger that steps through the code returns from a function at its start: the function's body does not run, and
# it returns no value; run by debugger-exception-events.qtest in every execution mode and compiled ahead of time,
# where the function runs its debug IR while the debugger is attached

%modern
%allow-debugger

#! returns from f_entry() at the step of its block, before its first statement
class EntryReturn inherits DebugProgram {
    public {
        bool returned = False;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
        rs = DebugStep;
        hash<auto> i = pgm.getStatementIdInfo(blockStatementId);
        if (!exists statementId && i.start_line + i.offset == EntryLine) {
            flow = DebugFlowReturn;
            returned = True;
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
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
}

#! the line of the block of f_entry(), which starts at its first statement
const EntryLine = 50;

*int sub f_entry() {
    print("the body of f_entry() ran\n");
    return 1;
}

EntryReturn r();
ProgramControl pc = ProgramControl::getProgram();
Counter cnt(1);
background sub () {
    on_exit cnt.dec();
    r.addProgram(pc);
    on_exit r.removeProgram(pc);
    try {
        *int rv = call_function("f_entry");
        printf("returned=%y rv=%y\n", r.returned, rv);
    } catch (hash<ExceptionInfo> ex) {
        printf("returned=%y %s: %s\n", r.returned, ex.err, ex.desc);
    }
}();
cnt.waitForZero();
