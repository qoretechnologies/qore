#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a debugger that steps through the code returns from a function at its start: the function's body does not run, and
# it returns no value; run by debugger-exception-events.qtest in every execution mode and compiled ahead of time,
# where the function runs its debug IR while the debugger is attached
#
# the debugger attaches to every thread of the program: the main thread, which runs top-level code, steps too once
# the debugger is attached, and the block of top-level code has no statement ID (onStep() gets 0 for it); the main
# thread waits until the debugger is attached before its next statement, and the thread that calls f_entry() waits
# until the main thread has run the step of that statement, so that the main thread always steps while the debugger
# is attached (top-level code compiled ahead of time makes no step event)

%modern
%allow-debugger

#! returns from f_entry() at the step of its block, before its first statement
class EntryReturn inherits DebugProgram {
    public {
        bool returned = False;
        #! the TID of the main thread, which runs top-level code
        int main_tid;
        #! decremented when the main thread has run the step of a statement while the debugger is attached
        Counter main_stepped(1);
    }

    constructor(int main_tid) {
        self.main_tid = main_tid;
    }

    #! called by the main thread when it has run the step of a statement, with or without a step event
    mainStepped() {
        if (main_stepped.getCount()) {
            main_stepped.dec();
        }
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
        rs = DebugStep;
        # also when the callback raises an exception: the thread that calls f_entry() waits for the main thread's step
        on_exit if (gettid() == main_tid) {
            mainStepped();
        }
        # the step of a block before its first statement; the block of top-level code has no statement ID
        if (exists statementId || !blockStatementId) {
            return;
        }
        hash<auto> i = pgm.getStatementIdInfo(blockStatementId);
        if (i.start_line + i.offset == EntryLine) {
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
const EntryLine = 79;

*int sub f_entry() {
    print("the body of f_entry() ran\n");
    return 1;
}

EntryReturn r(gettid());
ProgramControl pc = ProgramControl::getProgram();
Counter cnt(1);
Counter attached(1);
background sub () {
    on_exit cnt.dec();
    r.addProgram(pc);
    on_exit r.removeProgram(pc);
    attached.dec();
    r.main_stepped.waitForZero();
    try {
        *int rv = call_function("f_entry");
        printf("returned=%y rv=%y\n", r.returned, rv);
    } catch (hash<ExceptionInfo> ex) {
        printf("returned=%y %s: %s\n", r.returned, ex.err, ex.desc);
    }
}();
# the next statement of the main thread is stepped by the attached debugger
attached.waitForZero();
r.mainStepped();
cnt.waitForZero();
