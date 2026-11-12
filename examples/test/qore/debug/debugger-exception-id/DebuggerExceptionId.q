# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The debug_id key of the exceptions that a debugger gets (#5906); see debugger-exception-id.qtest

%modern
%allow-debugger

#! records the exceptions of the functions below, each once, with the line of its first event and its ID, numbered in
#! the order that the IDs are first seen: an exception whose events had different IDs would be recorded again
class Recorder inherits DebugProgram {
    public {
        list<string> seen = ();
        hash<string, int> ids = {};
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugRun;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int b, *int s, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugRun;
    }
    onFunctionEnter(ProgramControl pgm, int s, reference rs, reference rts) {
        rs = DebugRun;
    }
    onFunctionExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {
        rs = DebugRun;
    }
    onException(ProgramControl pgm, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugRun;
        hash<auto> i = pgm.getStatementIdInfo(s);
        int l = i.start_line + i.offset;
        if (l < FirstLine || l > LastLine) {
            return;
        }
        string key = exists ex.debug_id ? ex.debug_id.toString() : "none";
        if (!exists ids{key}) {
            ids{key} = ids.size() + 1;
            seen += sprintf("%s@%d#%d", ex.err, l, ids{key});
        } else if (!inlist(sprintf("%s#%d", ex.err, ids{key}), (map regex_subst($1, "@[0-9]+", ""), seen))) {
            # the same ID for another exception
            seen += sprintf("%s@%d#%d", ex.err, l, ids{key});
        }
    }
    onExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {
    }
}

const FirstLine = 50;
const LastLine = 90;

# two levels: one exception, which leaves the statement that raised it and the statement that called its function
sub inner() {
    *int src;
    int count = src;
}
sub two_levels() {
    inner();
}
# a loop that raises from the same statement: a new exception in each iteration
sub loop() {
    for (int i = 0; i < 2; ++i) {
        try {
            throw "LOOP", i;
        } catch () {
        }
    }
}
# a rethrow keeps the exception's ID
sub rethrow_it() {
    try {
        throw "RE";
    } catch () {
        rethrow;
    }
}
# an exception that an on_error handler raises is a new exception
sub in_handler() {
    if (True) {
        on_error {
            throw "HANDLER";
        }
        throw "BODY";
    }
}

Recorder r();
ProgramControl pc = ProgramControl::getProgram();
Counter cnt(1);
background sub () {
    on_exit cnt.dec();
    r.addProgram(pc);
    on_exit r.removeProgram(pc);
    # the functions are called through call_function(), so that each call is dispatched through the tiered entry
    foreach string fn in (("two_levels", "loop", "rethrow_it", "in_handler")) {
        r.seen = ();
        r.ids = {};
        try {
            call_function(fn);
        } catch () {
        }
        printf("%s: %y\n", fn, r.seen);
    }
}();
cnt.waitForZero();
