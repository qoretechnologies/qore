#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the debugger of debugger-inline-method-calls.qtest: it parses InlineCallsTarget.q in a Program without
# PO_ALLOW_DEBUGGER (which can be debugged; the option would give its code frames that the IR interpreter does not run
# inline), calls its function run() once without a debugger, so that the IR interpreter resolves its call sites, and
# then again in a thread of its own, where run() attaches this debugger in its iteration i == 2 and removes it in its
# iteration i == 4; the debugger steps
#
# usage: InlineCallsDebugger.q <target file> <request>
# - request: "none", or "break" or "continue": the flow request given at each step of the statement in the loop of a
#   callee (tags m_body and s_body)
#
# output: RESULT:<value returned> STEPS:<step events of the callees' statements, by tag> FRAMES:<the variables of the
# callee's frame at its first statement, sorted>
# with a DEBUG build of the library, a second line:
# SITES:<the IR inline call sites suspended after the attach> <suspended after the run> <eligible sites gained>

%modern
%allow-debugger

class InlineCallsDebugger inherits DebugProgram {
    public {
        list<string> steps = ();
        list<string> frames = ();
    }

    private {
        hash<string, string> tags;
        string request;
    }

    constructor(hash<string, string> tags, string request) {
        self.tags = tags;
        self.request = request;
    }

    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }

    onDetach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugDetach;
    }

    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (!sid) {
            return;
        }
        *string tag = tags{string(pgm.getStatementIdInfo(sid).start_line)};
        if (!tag) {
            return;
        }
        push steps, tag;
        if (tag =~ /_first$/) {
            # frame 0 is this method's; frame 1 is the code stopped
            push frames, sprintf("%s:%s", tag, (sort(keys get_local_vars(1))).join(","));
        } else if (request == "break") {
            flow = DebugFlowBreak;
        } else if (request == "continue") {
            flow = DebugFlowContinue;
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

#! returns the numbers of the IR inline call sites by state with a DEBUG build of the library
*hash<auto> sub site_states() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_ir_inline_site_states") : NOTHING;
}

string src = ReadOnlyFile::readTextFile(ARGV[0]);
hash<string, string> tags = {};
{
    list<string> lines = src.split("\n");
    for (int i = 0; i < lines.size(); ++i) {
        *list<*string> tag = lines[i] =~ x/### ([a-z_]+)$/;
        if (tag) {
            tags{string(i + 1)} = tag[0];
        }
    }
}

InlineCallsDebugger dbg(tags, ARGV[1]);
Program p(PO_NEW_STYLE);
# a child Program can inherit it
p.disableParseOptions(PO_ALLOW_DEBUGGER);
p.parse(src, "target");
code noop = sub () {};
# the call sites of run() are resolved
p.callFunction("run", 2, noop, noop);
ProgramControl pc = ProgramControl::resolveProgramId(p.getProgramId());
*hash<auto> attached;
string result = "no result: the thread of the call ended";
Counter done(1);
background sub () {
    on_exit done.dec();
    try {
        result = sprintf("%y", p.callFunction("run", 6, sub () {
            dbg.addProgram(pc);
            attached = site_states();
        }, sub () {
            dbg.removeProgram(pc);
        }));
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
printf("RESULT:%s STEPS:%y FRAMES:%y\n", result, dbg.steps, dbg.frames);
if (attached) {
    hash<auto> after = site_states();
    printf("SITES:%d %d %d\n", attached.suspended, after.suspended, after.eligible - attached.eligible);
}
