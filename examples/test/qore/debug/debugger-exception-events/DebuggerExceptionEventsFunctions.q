#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the exception events that a debugger gets for functions that raise exceptions in different statements, as
# "<code>@<line>", one function per line, and the result of a function whose exception the debugger dismisses; run
# by debugger-exception-events.qtest in every execution mode and compiled ahead of time, where the functions run
# their debug IR while the debugger is attached

%modern
%allow-debugger

#! records the exception events of the functions in this file; dismisses the exceptions of f_dismiss()
class ExceptionRecorder inherits DebugProgram {
    public {
        list<string> seen = ();
        bool dismiss_division = False;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugRun;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
    }
    onFunctionEnter(ProgramControl pgm, int statementId, reference rs, reference rts) {
    }
    onFunctionExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
    onException(ProgramControl pgm, int statementId, hash<auto> ex, reference dismiss, reference rs,
            reference rts) {
        hash<auto> i = pgm.getStatementIdInfo(statementId);
        # only the statements of the functions below, not those of the calls in the thread that runs them
        if (i.start_line + i.offset < FirstLine || i.start_line + i.offset > LastLine) {
            return;
        }
        seen += sprintf("%s@%d", ex.err, i.start_line + i.offset);
        if (dismiss_division && ex.err == "DIVISION-BY-ZERO") {
            dismiss = True;
        }
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
}

#! the first and the last line of the functions
const FirstLine = 52;
const LastLine = 97;

sub f_throw() {
    throw "B";
}
sub f_nested(int x) {
    if (x) {
        throw "C";
    }
}
sub f_builtin() {
    int z = 0;
    int y = 1 / z;
}
sub f_caught() {
    try {
        throw "E";
    } catch (hash<ExceptionInfo> ex) {
    }
    throw "F";
}
sub f_loop() {
    for (int i = 0; i < 1; ++i) {
        throw "H";
    }
}
sub f_caller() {
    f_throw();
}
sub f_catch_rethrow() {
    try {
        throw "J";
    } catch (hash<ExceptionInfo> ex) {
        throw "K";
    }
}
sub f_builtin_caught() {
    try {
        int z = 0;
        int y = 1 / z;
    } catch (hash<ExceptionInfo> ex) {
    }
}
*int sub f_dismiss() {
    int z = 0;
    int y = 1 / z;
    return 2;
}

ExceptionRecorder r();
ProgramControl pc = ProgramControl::getProgram();
Counter cnt(1);
background sub () {
    on_exit cnt.dec();
    r.addProgram(pc);
    on_exit r.removeProgram(pc);
    # the functions are called through call_function(), so that each call is dispatched through the tiered entry of
    # the variant, as calls between separately compiled AOT objects are
    foreach string fn in (("f_throw", "f_nested", "f_builtin", "f_caught", "f_loop", "f_caller", "f_catch_rethrow",
            "f_builtin_caught")) {
        r.seen = ();
        try {
            fn == "f_nested" ? call_function(fn, 1) : call_function(fn);
        } catch (hash<ExceptionInfo> ex) {
        }
        printf("%s: %y\n", fn, r.seen);
    }
    r.seen = ();
    r.dismiss_division = True;
    *int rv = call_function("f_dismiss");
    printf("f_dismiss: %y rv=%y\n", r.seen, rv);
}();
cnt.waitForZero();
