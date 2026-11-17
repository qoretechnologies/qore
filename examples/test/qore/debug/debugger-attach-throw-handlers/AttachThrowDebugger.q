#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the debugger of debugger-attach-throw-handlers.qtest: it parses AttachThrowTarget.q in a Program that does not allow
# a debugger, calls a function of it once, which compiles the function to native code with JIT and tiered execution
# (QORE_JIT_SYNC_COMPILE=1), and calls it again with a debugger that its callee attaches in the iteration i == 2 of
# its loop, before it raises an exception; the debugger steps
#
# usage: AttachThrowDebugger.q <target file> <function>
# - the function is called in a thread of its own
#
# output: RESULT:<value returned or exception raised> LOG:<values logged by the function> STEPS:<step events>
# - a step event of a statement is its line, that of a block "b<line>"

%modern
%allow-debugger

class AttachThrowDebugger inherits DebugProgram {
    public {
        list<auto> steps = ();
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

AttachThrowDebugger dbg();
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
string result = "no result: the thread of the call ended";
Counter done(1);
background sub () {
    on_exit done.dec();
    try {
        result = sprintf("%y", p.callFunction(func, 5, sub () { dbg.addProgram(pc); },
            sub () { dbg.removeProgram(pc); }, logger));
    } catch (hash<ExceptionInfo> ex) {
        result = sprintf("%s: %s", ex.err, ex.desc);
    }
}();
done.waitForZero();
# removes the debugger if the function did not
try {
    dbg.removeProgram(pc);
} catch (hash<ExceptionInfo> ex) {
}
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, dbg.steps);
