#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a debugger of debugger-step-flow.qtest that another thread attaches while spin_loop() of StepFlowTarget.q runs in a
# thread of its own: it requests a break at the second step event of the statement on <line>, and stops the loop
# after the fourth one; then spin_loop() is called again with the debugger attached, with the same request
#
# usage: SpinFlowDebugger.q <target file> <line>
#
# output: RUNNING:<f and total of the call running when the debugger attached> AGAIN:<f and total of the next call>

%modern
%allow-debugger

class SpinFlowDebugger inherits DebugProgram {
    public {
        int flow_line;
        int hits = 0;
        bool stop = False;
    }

    constructor(int flow_line) {
        self.flow_line = flow_line;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }

    onDetach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugDetach;
    }

    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (sid && pgm.getStatementIdInfo(sid).start_line == flow_line) {
            ++hits;
            if (hits == 2) {
                flow = DebugFlowBreak;
            } else if (hits == 4) {
                stop = True;
            }
        }
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

SpinFlowDebugger dbg(ARGV[1].toInt());
Program p(PO_NEW_STYLE);
p.disableParseOptions(PO_ALLOW_DEBUGGER);
p.parse(ReadOnlyFile::readTextFile(ARGV[0]), "target");
code noop = sub () {};
# compiles the function to native code with JIT and tiered execution
p.callFunction("spin_loop", noop, bool sub (int i) { return i < 5; });
ProgramControl pc = ProgramControl::resolveProgramId(p.getProgramId());

Counter started(1);
Counter done(1);
string running;
background sub () {
    on_exit done.dec();
    running = p.callFunction("spin_loop", sub () { started.dec(); }, bool sub (int i) { return !dbg.stop; });
}();
# the loop runs: this thread attaches the debugger to its Program
started.waitForZero();
dbg.addProgram(pc);
done.waitForZero();

# a new call with the debugger attached
dbg.hits = 0;
dbg.stop = False;
string again = p.callFunction("spin_loop", noop, bool sub (int i) { return i < 5; });
dbg.removeProgram(pc);
printf("RUNNING:%s AGAIN:%s\n", running, again);
