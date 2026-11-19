# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%allow-debugger

int sub leaf(int value) {
    int n = value + 1;
    return n * 2;
}

int sub length(string value) { return -1; } # Same name as a builtin and a different overload.

int sub length(int value, float scale, bool adjust, string label) {
    int n = leaf(value) + (adjust ? int(scale) + label.size() : 0);
    while (n < 0) { ++n; }
    return n + 10;
}

int sub throughHelper(int value, ...) { return ::length(value, 2.0, True, "abc"); }

class Recorder inherits DebugProgram {
    public {
        list<string> events = ();
    }
    private { string action; }
    constructor(string action) { self.action = action; }

    onAttach(ProgramControl p, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl p, reference rs, reference rts) {}
    onStep(ProgramControl p, int b, *int s, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (s) {
            hash<auto> i = p.getStatementIdInfo(s);
            int line = i.start_line + i.offset;
            # Only calls made after the attach are under test.
            if (line >= 7 && line <= 16) {
                events += sprintf("step %d", line);
                if (line == 14) {
                    events += sprintf("local %y", get_local_vars(1).value.value);
                }
                if (line == 14 && action == "next") { rs = DebugStepOver; }
                if (line == 7 && action == "out") { rs = DebugUntilReturn; }
            }
        }
    }
    onFunctionEnter(ProgramControl p, int s, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl p, int s, reference result, reference rs, reference rts) { rs = DebugStep; }
    onException(ProgramControl p, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl p, int s, reference result, reference rs, reference rts) {}
}

int sub runProbe(Recorder debugger, bool attach, bool extra, int valueArg) {
    if (attach) { debugger.addProgram(ProgramControl::getProgram()); }
    # An extra argument takes the AOT direct-call helper; the other call can use the typed entry.
    int value = extra ? throughHelper(valueArg, 0) : ::length(valueArg, 2.0, True, "abc");
    if (attach) { debugger.removeProgram(ProgramControl::getProgram()); }
    return value;
}

foreach string action in ("step", "next", "out") {
    Recorder debugger(action);
    foreach bool extra in (True, False) {
        debugger.events = ();
        runProbe(debugger, False, extra, 3);
        printf("%s %s %d %y\n", extra ? "helper" : "fast", action, runProbe(debugger, True, extra, 3), debugger.events);
    }
}
