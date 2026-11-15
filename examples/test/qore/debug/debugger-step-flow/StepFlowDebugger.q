#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the debugger of debugger-step-flow.qtest: it parses StepFlowTarget.q in a Program that does not allow a debugger,
# calls a function of it once, which compiles the function to native code with JIT and tiered execution
# (QORE_JIT_SYNC_COMPILE=1), and calls it again with a debugger that the function attaches in the iteration i == 2 of
# its loop, while it runs: as native code with JIT and tiered execution, with the IR interpreter with IR execution and
# with the AST interpreter with AST execution; the debugger steps, and requests a flow at a step event
#
# usage: StepFlowDebugger.q <target file> <function> <return|break|continue> <line> <hit> [block]
# - the flow is requested at the <hit>th step event of the statement on <line>, or with "block", of the block that
#   starts on <line>
#
# output: RESULT:<value returned or exception raised> LOG:<values logged by the function> STEPS:<step events>
# - a step event of a statement is its line, that of a block "b<line>"

%modern
%allow-debugger

class StepFlowDebugger inherits DebugProgram {
    public {
        list<auto> steps = ();
        int flow_value;
        int flow_line;
        int flow_hit;
        bool at_block;
        int hits = 0;
    }

    constructor(int flow_value, int flow_line, int flow_hit, bool at_block) {
        self.flow_value = flow_value;
        self.flow_line = flow_line;
        self.flow_hit = flow_hit;
        self.at_block = at_block;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }

    onDetach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugDetach;
    }

    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        *hash<auto> loc = pgm.getStatementIdInfo(sid ?? blk);
        int line = loc.start_line ?? -1;
        push steps, sid ? line : sprintf("b%d", line);
        if (line == flow_line && (at_block ? !sid : exists sid) && ++hits == flow_hit) {
            flow = flow_value;
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

const Flows = {
    "return": DebugFlowReturn,
    "break": DebugFlowBreak,
    "continue": DebugFlowContinue,
};

StepFlowDebugger dbg(Flows{ARGV[2]}, ARGV[3].toInt(), ARGV[4].toInt(), ARGV[5] == "block");
Program p(PO_NEW_STYLE);
# a child Program can inherit it
p.disableParseOptions(PO_ALLOW_DEBUGGER);
p.parse(ReadOnlyFile::readTextFile(ARGV[0]), "target");
list<string> log = ();
code logger = bool sub (auto v) {
    push log, v;
    return True;
};
code noop = sub () {};
string func = ARGV[1];
# compiles the function to native code with JIT and tiered execution
p.callFunction(func, 5, noop, noop, logger);
log = ();
ProgramControl pc = ProgramControl::resolveProgramId(p.getProgramId());
string result;
try {
    result = sprintf("%y", p.callFunction(func, 5, sub () { dbg.addProgram(pc); }, sub () { dbg.removeProgram(pc); },
        logger));
} catch (hash<ExceptionInfo> ex) {
    result = sprintf("%s: %s", ex.err, ex.desc);
}
# the function does not detach the debugger if it returns at a step event
dbg.removeProgram(pc);
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, dbg.steps);
