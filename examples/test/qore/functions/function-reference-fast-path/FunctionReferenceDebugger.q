#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# A debugger attached to the Program while a loop calls a function through a reference (qore#5578): the calls made
# before it attached take the fast path in the compiled execution modes, and every call made after it attached must
# give the debugger the function's entry, as the AST interpreter does; the loop's frame keeps running.
#
# With the argument "report" (debug builds), it also prints how many calls took the fast path after the debugger
# attached (dbg_function_reference_fast_calls(), and dbg_function_reference_native_calls() for the call-reference
# entries of AOT code), which must be none.

%modern
%allow-debugger

class EntryRecorder inherits DebugProgram {
    public {
        int entries = 0;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }

    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }

    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
    }

    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) {
        ++entries;
        rs = DebugStep;
    }

    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {
    }

    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
    }

    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {
    }
}

bool report = ARGV[0] == "report";

int sub step(int x) {
    return x + 1;
}

int sub run_loop(code f, int n, int k, *Queue ready, *Queue go) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        if (i == k) {
            ready.push(True);
            go.get();
        }
        sum += f(i);
    }
    return sum;
}

int sub fast_calls() {
    # the calls through call-reference entries of AOT code (qore#5578) skip the generic call path too
    return report ? call_function("dbg_function_reference_fast_calls")
        + call_function("dbg_function_reference_native_calls") : 0;
}

# the warm-up compiles the code in the compiled execution modes and takes the facts of the fast path
int warm = run_loop(\step(), 500, -1);

EntryRecorder dbg();
ProgramControl pc = ProgramControl::getProgram();
Queue ready();
Queue go();
Queue result();
background result.push(run_loop(\step(), 100, 40, ready, go));
# the loop has made 40 calls, and waits
ready.get();
dbg.addProgram(pc);
int before = fast_calls();
go.push(True);
int r = result.get();
int after = fast_calls();
dbg.removeProgram(pc);
printf("warm %d result %d entries after attach %d\n", warm, r, dbg.entries);
if (report) {
    printf("fast calls after attach: %d\n", after - before);
}
