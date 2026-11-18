#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the debugger of debugger-step-flow.qtest: it parses StepFlowTarget.q in a Program that does not allow a debugger,
# calls a function of it once, which compiles the function to native code with JIT and tiered execution
# (QORE_JIT_SYNC_COMPILE=1), and calls it again with a debugger that the function attaches in the iteration i == 2 of
# its loop, while it runs: as native code with JIT and tiered execution, with the IR interpreter with IR execution and
# with the AST interpreter with AST execution; the debugger steps, and requests a flow at a step event
#
# usage: StepFlowDebugger.q <target file> <function> <return|break|continue|none> <line> <hit> [block|-]
#            [<return|break|continue> <line> <hit> | again]
# - the flow is requested at the <hit>th step event of the statement on <line>, or with "block", of the block that
#   starts on <line>; a second request can follow, at the <hit>th step event of a statement after the first request
# - the function is called in a thread of its own, so that a thread_exit that the requests leave pending ends only it
# - with "again", the debugger stays attached when the call ends, and the function is called a second time in the
#   thread, with the debugger attached when the call starts (a new call, which runs on the AST interpreter in every
#   execution mode), with the same request, and then a third time after the debugger detached; the debugger requests
#   the flow in each of the first two calls
#
# output: RESULT:<value returned or exception raised> LOG:<values logged by the function> STEPS:<step events>
# - a step event of a statement is its line, that of a block "b<line>"
# - a line REQUEST-STACK:<the user code functions of the call stack at the first request, innermost first>
#   FRAME:<the local variables of the frame of the code at the first request>
# - with "again", a second line AGAIN-RESULT:... LOG:... STEPS:... for the second call, and a third line
#   DETACHED-RESULT:<value> COMPILED:<the number of functions compiled to native code by the third call, which runs the
#   code compiled before the debugger attached, or "-" without library debugging>

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
        bool requested = False;
        *int flow2_value;
        *int flow2_line;
        *int flow2_hit;
        int hits2 = 0;
        #! false while the step events are not recorded and no flow is requested
        bool recording = True;
        #! the user code functions of the call stack at the first request, innermost first
        list<string> request_stack = ();
        #! the local variables of the frame of the code at the first request
        list<auto> request_locals = ();
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
        if (!recording) {
            return;
        }
        *hash<auto> loc = pgm.getStatementIdInfo(sid ?? blk);
        int line = loc.start_line ?? -1;
        push steps, sid ? line : sprintf("b%d", line);
        if (line == flow_line && (at_block ? !sid : exists sid) && ++hits == flow_hit) {
            flow = flow_value;
            requested = True;
            # the user code functions of the call stack of the thread at the request, innermost first
            request_stack = map $1.function, get_thread_call_stack(), $1.type == "user";
            # the local variables of the frame of the code that made the step event (frame 0 is this method's)
            request_locals = sort(keys get_local_vars(1) ?? {});
        } else if (requested && exists flow2_value && sid && line == flow2_line && ++hits2 == flow2_hit) {
            flow = flow2_value;
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
    # no request: the steps only
    "none": 0,
};

StepFlowDebugger dbg(Flows{ARGV[2]}, ARGV[3].toInt(), ARGV[4].toInt(), ARGV[5] == "block");
bool again = ARGV[6] == "again";
if (ARGV[6] && !again) {
    dbg.flow2_value = Flows{ARGV[6]};
    dbg.flow2_line = ARGV[7].toInt();
    dbg.flow2_hit = ARGV[8].toInt();
}
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
code attach = sub () { dbg.addProgram(pc); };
code detach = sub () { dbg.removeProgram(pc); };
code stop_recording = sub () { dbg.recording = False; };
# compiles the closures to native code with JIT and tiered execution before the calls count what they compile
detach();
stop_recording();
dbg.recording = True;

# returns the number of functions compiled to native code, with library debugging
*int sub compiled_count() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_jit_compiled_function_count") : NOTHING;
}

# returns the value that the call returned, or the exception that it raised
code call = string sub (code attach, code detach) {
    try {
        return sprintf("%y", p.callFunction(func, 5, attach, detach, logger));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s", ex.err, ex.desc);
    }
};

string result = "no result: the thread of the call ended";
string again_result = result;
list<auto> again_log;
list<auto> again_steps;
string detached_result = result;
*int compiled_before;
*int compiled_after;
Counter done(1);
background sub () {
    on_exit done.dec();
    if (!again) {
        result = call(attach, detach);
        return;
    }
    # the debugger stays attached after the first call, which attaches it while it runs, and records nothing after the
    # call detached it
    result = call(attach, stop_recording);
    again_log = log;
    again_steps = dbg.steps;
    # the second call starts with the debugger attached, and the debugger requests the flow again
    (log, dbg.steps, dbg.hits, dbg.hits2, dbg.requested, dbg.recording) = ((), (), 0, 0, False, True);
    # swaps the results of the two calls back after the second call
    again_result = call(noop, detach);
    (again_log, log) = (log, again_log);
    (again_steps, dbg.steps) = (dbg.steps, again_steps);
    # the debugger is detached (also if the second call returned at a step event before it detached it): the code
    # runs as before the debugger attached, without being compiled again
    dbg.removeProgram(pc);
    compiled_before = compiled_count();
    list<auto> saved_log = log;
    detached_result = call(noop, noop);
    log = saved_log;
    compiled_after = compiled_count();
}();
done.waitForZero();
# the function does not detach the debugger if it returns at a step event
dbg.removeProgram(pc);
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, dbg.steps);
printf("REQUEST-STACK:%y FRAME:%y\n", dbg.request_stack, dbg.request_locals);
if (again) {
    printf("AGAIN-RESULT:%s LOG:%y STEPS:%y\n", again_result, again_log, again_steps);
    printf("DETACHED-RESULT:%s COMPILED:%s\n", detached_result, exists compiled_before
        ? sprintf("%d", compiled_after - compiled_before) : "-");
}
