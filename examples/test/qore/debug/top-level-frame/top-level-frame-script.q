#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# reads and sets the local variables of the frame of the top-level code of the main Program and of a child Program
# run with Program::run(), from the calls that the top-level code makes and from the step callback of a debugger;
# top-level-frame.qtest runs it in every execution mode and as an AOT-compiled executable, which it runs with the
# argument "aot"

%modern
%allow-debugger

#! true if the script runs as an AOT-compiled executable
/** AOT-compiled top-level code is native code without step events, and an AOT-compiled function has a frame only when
    it runs with a debugger attached (in the IR interpreter), so the executable skips the checks that need either; it
    checks instead that the frames of the calls that it makes directly cannot be accessed (#5890)
*/
our bool aot = ARGV[0] == "aot";

#! set by the top-level code before the statement where the debugger reads and sets its variables
our bool step_armed;

#! the debugger of the main Program, whose step callback reads and sets the variables of the top-level code
class StepDebugger inherits DebugProgram {
    public {
        list<string> out = ();
    }

    onAttach(ProgramControl pgm, reference run_state, reference run_to_statement_id) {
        run_state = DebugStep;
    }

    onDetach(ProgramControl pgm, reference run_state, reference run_to_statement_id) {
    }

    onStep(ProgramControl pgm, int block_statement_id, *int statement_id, *int breakpoint_id, reference flow,
            reference run_state, reference run_to_statement_id) {
        run_state = DebugStep;
        if (!statement_id || !step_armed) {
            return;
        }
        step_armed = False;
        # frame 0 is the frame of this callback, frame 1 the frame of the top-level code
        push out, sprintf("step frame 1: %y", sort(keys get_local_vars(1)));
        set_local_var_value(1, "dtop", 7);
        push out, sprintf("step frame 1 dtop: %y", get_local_vars(1).dtop.value);
    }

    onFunctionEnter(ProgramControl pgm, int statement_id, reference run_state, reference run_to_statement_id) {
    }

    onFunctionExit(ProgramControl pgm, int statement_id, reference return_value, reference run_state,
            reference run_to_statement_id) {
    }

    onException(ProgramControl pgm, int statement_id, hash<auto> ex, reference dismiss, reference run_state,
            reference run_to_statement_id) {
    }

    onExit(ProgramControl pgm, int statement_id, reference return_value, reference run_state,
            reference run_to_statement_id) {
    }
}

# frame 0 is the frame of f(), frame 1 that of g(), frame 2 that of the top-level code, and there is no frame 3
sub f(string var, int val) {
    int y = 2;
    printf("%s: call frame 2: %y\n", var, sort(keys get_local_vars(2)));
    printf("%s: call frame 3: %y\n", var, get_local_vars(3));
    set_local_var_value(2, var, val);
}

sub g(string var, int val) {
    int z = 3;
    f(var, val);
}

# the calls of a function, a method and a closure that declare no local variables of their own, only parameters, and
# of a function without parameters, have frames of their own as well, which the IR interpreter and native code give
# them by not inlining them (#5887): frame 0 is the frame of the call, frame 1 that of pg(), frame 2 that of the
# top-level code
sub pf(string var, int val) {
    printf("function frame 0: %y\n", sort(keys get_local_vars(0)));
    printf("function frame 1: %y\n", sort(keys get_local_vars(1)));
    printf("function frame 2: %y\n", sort(keys get_local_vars(2)));
    set_local_var_value(2, var, val);
}

sub nf() {
    printf("no-parameter frame 0: %y\n", sort(keys get_local_vars(0)));
    printf("no-parameter frame 1: %y\n", sort(keys get_local_vars(1)));
}

class ParamFrames {
    pm(string var, int val) {
        printf("method frame 0: %y\n", sort(keys get_local_vars(0)));
        printf("method frame 1: %y\n", sort(keys get_local_vars(1)));
        printf("method frame 2: %y\n", sort(keys get_local_vars(2)));
        set_local_var_value(2, var, val);
    }
}

sub pg(string var, int val) {
    int z = 3;
    ParamFrames pfo();
    code pc = sub (string var, int val) {
        printf("closure frame 0: %y\n", sort(keys get_local_vars(0)));
        printf("closure frame 1: %y\n", sort(keys get_local_vars(1)));
        printf("closure frame 2: %y\n", sort(keys get_local_vars(2)));
        set_local_var_value(2, var, val);
    };
    # the calls are made twice: the IR interpreter inlines a closure from its second call on
    for (int i = 0; i < 2; ++i) {
        pf(var, val);
        printf("%s: %d\n", var, get_local_vars(1){var}.value);
        nf();
        pfo.pm(var, val + 1);
        printf("%s: %d\n", var, get_local_vars(1){var}.value);
        pc(var, val + 2);
    }
}

#! the number of times reenter() was called
our int reentered;

#! the Program whose top-level code runs again in a call that it makes
our Program reentrant;

# prints the names of the local variables of the frames of the calls that called it
public sub dump_frames() {
    int dl = 1;
    for (int i = 1; i < 8; ++i) {
        printf("reentrant frame %d: %y\n", i, sort(keys get_local_vars(i)));
    }
}

# called by the top-level code of the Program "reentrant" through rf(): the first call runs that top-level code again,
# so the Program is entered again on the thread; a frame of the top-level code of the second entry would be the frame
# of the call that runs it, so the second entry has none of its own, and its top-level code has the frame of the
# first entry
public sub reenter() {
    int rl = ++reentered;
    if (rl == 1) {
        reentrant.run();
    } else {
        dump_frames();
    }
}

# a call that AOT-compiled code makes directly to an AOT-compiled function has no frame of its own, as the function
# keeps its local variables in native code: get_local_vars() and set_local_var_value() raise AOT-FRAME-ERROR for the
# frame of such a call and for the frames after it (#5890)
hash<auto> sub aot_frame0() {
    int y = 2;
    return get_local_vars(0);
}

sub aot_set_frame0() {
    int y = 2;
    set_local_var_value(0, "y", 3);
}

hash<auto> sub aot_frame1_callee() {
    return get_local_vars(1);
}

hash<auto> sub aot_frame1() {
    int y = 2;
    return aot_frame1_callee();
}

# a closure of a child Program called in a new thread through functions that call each other directly in AOT-compiled
# code: the thread's first entry into the child Program has a frame after that of the closure, which has no call of
# its own on the call stack, so no frame after the call from the main Program is matched with a call (#5890)
sub bg_h(code c) {
    int hv = 1;
    c();
}

sub bg_g(code c) {
    int gv = 1;
    bg_h(c);
}

sub bg_run(code c, Counter cnt) {
    on_exit cnt.dec();
    bg_g(c);
}

sub bg_check() {
    Program bgchild(PO_NEW_STYLE | PO_ALLOW_DEBUGGER);
    bgchild.parse("code sub getf() {
    return sub () {
        int cv = 1;
        for (int i = 0; i < 2; ++i) {
            printf(\"bg closure frame %d: %y\\n\", i, sort(keys get_local_vars(i)));
        }
    };
}", "bgchild");
    code cf = bgchild.callFunction("getf");
    Counter cnt(1);
    background bg_run(cf, cnt);
    cnt.waitForZero();
}

# makes the calls directly, without a debugger, and prints the codes of the exceptions raised
sub aot_checks() {
    try {
        aot_frame0();
        printf("aot frame 0: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("aot frame 0: %s\n", ex.err);
    }
    try {
        aot_set_frame0();
        printf("aot set frame 0: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("aot set frame 0: %s\n", ex.err);
    }
    try {
        aot_frame1();
        printf("aot frame 1: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("aot frame 1: %s\n", ex.err);
    }
}

int top = 1;
int ptop = 1;
int dtop = 1;
int ctop = 1;
StepDebugger debugger();
ProgramControl pgm = ProgramControl::getProgram();

# the calls of the top-level code
if (!aot) {
    g("top", 42);
    printf("top: %d\n", top);
    pg("ptop", 42);
    printf("ptop: %d\n", ptop);
} else {
    aot_checks();
    # the frame of the top-level code is accessible
    printf("aot top: %y\n", get_local_vars(0).top.value);
}

debugger.addProgram(pgm);
# the step callback of the debugger, before the statement after the one that arms it
if (!aot) {
    step_armed = True;
    dtop += 0;
}
# the calls of the top-level code while the debugger is attached; called through call_function() so that an
# AOT-compiled function is entered through its tiered entry, which runs it in the IR interpreter
call_function("g", "ctop", 43);
debugger.removeProgram(pgm);
map printf("%s\n", $1), debugger.out;
printf("dtop: %d\n", dtop);
printf("ctop: %d\n", ctop);

# in a child Program: frame 2 is the frame of its top-level code, and frame 3 that of the top-level code of the main
# Program, which runs Program::run() (the frame of the top-level code of the child Program also has its own instances
# of the top-level local variables of the main Program, which it inherits, so the values tell the frames apart)
Program child(PO_NEW_STYLE | PO_ALLOW_DEBUGGER);
child.parse("sub f() {
    int y = 2;
    printf(\"child frame 2 x: %y\\n\", get_local_vars(2).x.value);
    printf(\"child frame 3: %y\\n\", sort(keys get_local_vars(3)));
    printf(\"child frame 3 top: %y\\n\", get_local_vars(3).top.value);
    set_local_var_value(2, 'x', 43);
    set_local_var_value(3, 'top', 44);
}
sub g() {
    int z = 3;
    f();
}
int x = 1;
g();
printf(\"child x: %d\\n\", x);
", "child");
child.run();
printf("top: %d\n", top);

# a Program whose top-level code runs again in a call that it makes; AOT-compiled functions called without a debugger
# have no frames (see aot above)
if (!aot) {
    reentrant = new Program(PO_NEW_STYLE | PO_ALLOW_DEBUGGER);
    reentrant.parse("sub rf() { int rfl = 1; reenter(); } int rx = 1; rf();", "reentrant");
    reentrant.run();
}

# a closure of a child Program called in a new thread (see bg_check())
bg_check();
