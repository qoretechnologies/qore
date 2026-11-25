%modern
%allow-debugger
%no-child-restrictions
%disable-warning unreferenced-variable
# with a debugger attached, the exception that leaves a function from the body of a foreach restores the caller's $#:
# an AOT function then runs its debug IR in the IR interpreter, which restores the element as the IR that it was made
# from; see foreach-implicit-element-exception-exit.qtest (qore#7379)
# Copyright (C) 2026 Qore Technologies, s.r.o.

our list<string> log;
const Data = {"a": (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)};

sub thrower() {
    throw "X";
}

# a call from the body throws; the body reads $#
sub callee(list<auto> l) {
    foreach auto a in (l) {
        if ($# == 1) {
            thrower();
        }
    }
}

# a handler in the body reads the loop's $# as the exception leaves the function
sub handlers_callee() {
    foreach auto a in ((1, 2, 3, 4)) {
        on_error log += sprintf("on_error %d %d", $#, (find %a in Data where (%a == $#)));
        if ($# == 2) {
            thrower();
        }
    }
}

# the iterator of a map in a body: the operator's implicit context
sub map_callee() {
    foreach auto a in ((1, 2, 3)) {
        if ($# == 1) {
            list<auto> l = map ($# == 2 ? thrower() : $1), (1, 2, 3);
        }
    }
}

sub run() {
    foreach auto o in ((10, 20, 30)) {
        try {
            callee((1, 2, 3));
        } catch () {
            log += "b" + $#;
        }
        log += "k" + (find %a in Data where (%a == $#));
        try {
            handlers_callee();
        } catch () {
        }
        log += "h" + (find %a in Data where (%a == $#));
        try {
            map_callee();
        } catch () {
        }
        log += "m" + (find %a in Data where (%a == $#));
    }
}

class StepDebugger inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugDetach;
    }
    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {
    }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
    }
    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {
    }
}

StepDebugger dbg();
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
# the program runs in a thread that the debugger is attached to with the argument "debug"
background sub () {
    on_exit done.dec();
    if (ARGV[0] == "debug") {
        dbg.addProgram(pc);
    }
    on_exit if (ARGV[0] == "debug") {
        dbg.removeProgram(pc);
    }
    run();
}();
done.waitForZero();
map printf("%s\n", $1), log;
