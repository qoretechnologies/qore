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
    it runs with a debugger attached (in the IR interpreter), so the executable skips the checks that need either
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
# (f() declares a local variable: a call of a function that declares none has no frame of its own in IR and native
# code while no debugger is attached)
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

int top = 1;
int dtop = 1;
int ctop = 1;
StepDebugger debugger();
ProgramControl pgm = ProgramControl::getProgram();

# the calls of the top-level code
if (!aot) {
    g("top", 42);
    printf("top: %d\n", top);
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
