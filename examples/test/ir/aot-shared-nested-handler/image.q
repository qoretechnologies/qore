# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-debugger

our bool throwInSibling;
our int observed;
sub throwIfRequested() {
    if (throwInSibling) {
        throw "CLEANUP";
    }
}

auto sub handler_writes(auto tens, bool raise) {
%include handler.qi
}

code sub makeHandler() {
    return auto sub (auto tens, bool raise) {
%include handler.qi
    };
}

class Recorder inherits DebugProgram {
    public {
        list<string> steps = ();
        hash<string, string> names;
        string stop;
    }
    onAttach(ProgramControl p, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl p, reference rs, reference rts) {}
    onStep(ProgramControl p, int b, *int id, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (id) {
            hash<auto> info = p.getStatementIdInfo(id);
            *string name = names{info.start_line.toString()};
            if (name && info.file.endsWith("handler.qi")) {
                push steps, name;
                if (name == stop) {
                    flow = DebugFlowReturn;
                }
            }
        }
    }
    onFunctionEnter(ProgramControl p, int id, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl p, int id, reference result, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl p, int id, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugStep;
    }
    onExit(ProgramControl p, int id, reference result, reference rs, reference rts) {}
}

code closure = makeHandler();
Recorder dbg();
ProgramControl pc = ProgramControl::getProgram();
bool stepping = ARGV[0] == "step";
if (stepping) {
    int i = 1;
    foreach string name in (("inner", "sibling", "early", "late")) {
        dbg.names{ARGV[i++]} = name;
    }
    dbg.addProgram(pc);
}
foreach string target in (("function", "closure")) {
    foreach string path in (("normal", "exception", "early", "late", "unwind", "unwind-early", "unwind-late")) {
        string stop = path.split("-").last();
        if (!stepping && (stop == "early" || stop == "late")) {
            continue;
        }
        dbg.steps = ();
        observed = 0;
        throwInSibling = path.startsWith("unwind");
        dbg.stop = stop;
        # Dynamic dispatch lets an attached debugger select the function's serialized debug IR.
        bool raise = path == "exception" || dbg.stop == "late";
        auto value;
        try {
            value = target == "function" ? call_function("handler_writes", 0, raise) : closure(0, raise);
        } catch (hash<ExceptionInfo> e) {
            value = e.err;
        }
        printf("%s %s %y %d [%s]\n", target, path, value, observed, dbg.steps.join(" "));
    }
}
if (stepping) {
    dbg.removeProgram(pc);
}
