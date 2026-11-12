#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the exception events that a debugger gets for functions that raise exceptions in different statements, as
# "<code>@<line>", one function per line, and the result of a function whose exception the debugger dismisses; with
# the argument "step", the debugger steps through the code and the exit events of the functions follow, as
# "exit@<line>"; run
# by debugger-exception-events.qtest in every execution mode and compiled ahead of time, where the functions run
# their debug IR while the debugger is attached

%modern
%allow-debugger

#! records the exception events of the functions in this file; dismisses the given event of an exception
class ExceptionRecorder inherits DebugProgram {
    public {
        list<string> seen = ();
        #! the code of the exception whose event dismiss_nth is dismissed
        *string dismiss_err;
        #! the event of the exception to dismiss, from 1
        int dismiss_nth;
        #! the events of the exception so far
        int dismiss_seen;
        #! the run state that the debugger gives the thread
        int run_state = Step ? DebugStep : DebugRun;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = run_state;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int blockStatementId, *int statementId, *int breakpointId, reference flow,
            reference rs, reference rts) {
        rs = run_state;
    }
    onFunctionEnter(ProgramControl pgm, int statementId, reference rs, reference rts) {
        rs = run_state;
    }
    onFunctionExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
        hash<auto> i = pgm.getStatementIdInfo(statementId);
        if (i.start_line + i.offset >= FirstLine && i.start_line + i.offset <= LastLine) {
            seen += sprintf("exit@%d", i.start_line + i.offset);
        }
        rs = run_state;
    }
    onException(ProgramControl pgm, int statementId, hash<auto> ex, reference dismiss, reference rs,
            reference rts) {
        rs = run_state;
        hash<auto> i = pgm.getStatementIdInfo(statementId);
        # only the statements of the functions below, not those of the calls in the thread that runs them
        if (i.start_line + i.offset < FirstLine || i.start_line + i.offset > LastLine) {
            return;
        }
        seen += sprintf("%s@%d", ex.err, i.start_line + i.offset);
        if (ex.err == dismiss_err && ++dismiss_seen == dismiss_nth) {
            dismiss = True;
        }
    }
    onExit(ProgramControl pgm, int statementId, reference result, reference rs, reference rts) {
    }
}

#! True if the debugger steps through the code
our bool Step = ARGV[0] == "step";

#! the first and the last line of the functions
const FirstLine = 72;
const LastLine = 350;

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
sub f_closure() {
    code c = sub () { throw "CL"; };
    c();
}
sub f_on_exit_throw() {
    on_exit throw "C2";
    throw "C";
}
sub f_on_error_rethrow_caught() {
    try {
        on_error rethrow;
        throw "R";
    } catch (hash<ExceptionInfo> ex) {
    }
}
class Thrower {
    destructor() {
        throw "DTOR";
    }
}
sub f_dtor(int x) {
    if (x) {
        new Thrower();
    }
}
*int sub f_dismiss() {
    int z = 0;
    int y = 1 / z;
    return 2;
}
*int sub f_dismiss_enclosing(int x) {
    if (x) {
        throw "DE";
    }
    return 5;
}
# an exception in a foreach in a function with an on_exit handler: the exception leaves the block through its cleanup
# code, which runs the handler; the events of the foreach and the if come before it
sub f_foreach_on_exit(int x) {
    on_exit {
        x = 2;
    }
    if (x) {
        foreach int i in (1) {
            throw "FO", i;
        }
    }
}
# the same with an on_exit handler that calls a closure that raises an exception
sub f_on_exit_closure(int x) {
    on_exit {
        code c = sub () {
            if (x) {
                throw "OC";
            }
        };
        if (x) {
            c();
        }
    }
    if (x) {
        foreach int i in (1) {
            throw "BODY", i;
        }
    }
}
# handlers that raise an exception on the normal exit of their block: the statements that contain the block get
# events, up to a try statement whose try block it is
sub f_obe_normal(int x) {
    if (x) {
        while (x) {
            on_error { x = 3; }
            on_exit { throw "ON"; }
            x = 0;
        }
    }
}
sub f_obe_normal_try(int x) {
    try {
        while (x) {
            on_error { x = 3; }
            on_exit { throw "ONT"; }
            x = 0;
        }
    } catch () {
    }
}
sub f_obe_normal_catch(int x) {
    try {
        throw "C0";
    } catch () {
        while (x) {
            on_error { x = 3; }
            on_exit { throw "ONC"; }
            x = 0;
        }
    }
}
# the exception of the destructor of a local of a block with a handler comes before the events of the statements
# that contain the block, as the block's locals are released before them
class ThrowerD {
    destructor() {
        throw "DTD";
    }
}
sub f_dtor_order(int x) {
    if (x) {
        foreach int i in (1) {
            on_exit { x = 2; }
            ThrowerD t();
            if (t) {
                throw "DO", i;
            }
        }
    }
}
# handlers that raise on the exit of a block left by its end, a break or a return, in a try or a catch block
sub f_empty(int x) {
    if (x) {
        if (x) {
            on_error { x = 3; }
            on_exit { throw "EM"; }
        }
    }
}
sub f_catch_direct(int x) {
    if (x) {
        try {
            throw "CD0";
        } catch () {
            on_error { x = 3; }
            on_exit { throw "CD"; }
            x = 2;
        }
    }
}
sub f_break(int x) {
    if (x) {
        while (True) {
            on_error { x = 3; }
            on_exit { throw "BR"; }
            if (x) {
                break;
            }
        }
    }
}
sub f_break_try(int x) {
    if (x) {
        while (True) {
            on_error { x = 3; }
            on_exit { throw "BT"; }
            try {
                break;
            } catch () {
            }
        }
        x = 2;
    }
}
sub f_return_try(int x) {
    if (x) {
        while (True) {
            on_error { x = 3; }
            on_exit { throw "RTT"; }
            try {
                return;
            } catch () {
            }
        }
    }
}
# inline handlers in a catch block, in a scope of the frame, and lowered at an exit inside an inner catch block
sub f_catch_inline(int x) {
    if (x) {
        try {
            throw "C0";
        } catch () {
            on_exit { throw "CI"; }
            x = 2;
        }
    }
}
sub f_outer_scope_try(int x) {
    on_exit { x = 1; }
    try {
        if (x) {
            if (x) {
                on_exit { throw "G"; }
                x = 2;
            }
        }
    } catch () {
    }
    if (x) {
        throw "G2";
    }
}
sub f_catch_inner_return(int x) {
    if (x) {
        try {
            throw "C0";
        } catch () {
            on_exit { throw "KR"; }
            try {
                throw "C1";
            } catch () {
                if (x) {
                    return;
                }
            }
        }
    }
}
# handlers that raise on the normal exit of a block with a local in a try: the local is released, raising its
# destructor's exception, before the try statement's catch block (#5855)
sub f_obe_try_local(int x) {
    try {
        if (x) {
            on_error { x = 3; }
            on_exit { throw "OTL"; }
            ThrowerD t();
            if (t) {
                x = 0;
            }
        }
    } catch () {
    }
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
            "f_builtin_caught", "f_closure", "f_on_exit_throw", "f_on_error_rethrow_caught", "f_dtor", "f_foreach_on_exit",
            "f_on_exit_closure", "f_obe_normal", "f_obe_normal_try", "f_obe_normal_catch", "f_dtor_order",
            "f_empty", "f_catch_direct", "f_break", "f_break_try", "f_return_try",
            "f_catch_inline", "f_outer_scope_try", "f_catch_inner_return", "f_obe_try_local")) {
        r.seen = ();
        try {
            fn =~ /^f_(nested|dtor|foreach_on_exit|on_exit_closure|obe_normal|obe_normal_try|obe_normal_catch|dtor_order|empty|catch_direct|break|break_try|return_try|catch_inline|outer_scope_try|catch_inner_return|obe_try_local)$/ ? call_function(fn, 1) : call_function(fn);
        } catch (hash<ExceptionInfo> ex) {
        }
        printf("%s: %y\n", fn, r.seen);
    }
    # an exception of an operator dismissed at its statement, and a throw dismissed at the if statement that contains
    # it: the function continues after the statement
    foreach list<auto> d in ((("f_dismiss", "DIVISION-BY-ZERO", 1), ("f_dismiss_enclosing", "DE", 2))) {
        r.seen = ();
        r.dismiss_err = d[1];
        r.dismiss_nth = d[2];
        r.dismiss_seen = 0;
        *int rv = d[0] == "f_dismiss_enclosing" ? call_function(d[0], 1) : call_function(d[0]);
        printf("%s: %y rv=%y\n", d[0], r.seen, rv);
    }
}();
cnt.waitForZero();
