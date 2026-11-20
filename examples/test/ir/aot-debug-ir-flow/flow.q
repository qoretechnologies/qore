%modern
%allow-debugger
# the debugger requests a flow at the hit-th step event of the statement on a line, in a function that runs while it
# is attached; the lines of the requests are those of the statements tagged "### <tag>" (see AOTDebugIRFlow.qtest)
class FlowDebugger inherits DebugProgram {
    public {
        list<auto> steps = ();
        int flow;
        int line;
        int hit;
        int hits = 0;
    }
    constructor(int flow, int line, int hit) {
        self.flow = flow;
        self.line = line;
        self.hit = hit;
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) { rs = DebugDetach; }
    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (!sid) {
            return;
        }
        int l = pgm.getStatementIdInfo(sid).start_line;
        push steps, l;
        if (l == line && ++hits == hit) {
            flow = self.flow;
        }
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
}

our list<string> log = ();

auto sub loop(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += 10;                                ### loop_first
        total += 1;
    }
    return total;
}

auto sub handler(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        on_exit {
            push log, "exit" + i;                   ### handler_body
            push log, "exit2-" + i;
        }
        total += 10;
        total += 1;
    }
    return total;
}

auto sub mixed(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        on_error push log, "error" + i;
        on_exit {
            push log, "mixed" + i;                  ### mixed_body
            push log, "mixed2-" + i;
        }
        total += 10;
        total += 1;
    }
    return total;
}

# a loop body that the AOT compiler outlines with the thresholds of AOTDebugIRFlow.qtest
auto sub outlined(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += 10;                                ### outlined_first
        total += 2;
        if (total % 3 == 1) { push log, "o0"; }
        total += 3;
        if (total % 4 == 1) { push log, "o1"; }
        total += 4;
        if (total % 5 == 1) { push log, "o2"; }
        total += 5;
        if (total % 6 == 1) { push log, "o3"; }
        total += 6;
        if (total % 7 == 1) { push log, "o4"; }
        total += 7;
        if (total % 8 == 1) { push log, "o5"; }
        total += 8;
        if (total % 9 == 1) { push log, "o6"; }
        total += 9;
        if (total % 10 == 1) { push log, "o7"; }
        total += 10;
        if (total % 11 == 1) { push log, "o8"; }
        total += 11;
        if (total % 12 == 1) { push log, "o9"; }
        total += 12;
        if (total % 13 == 1) { push log, "o10"; }
        total += 13;
        if (total % 14 == 1) { push log, "o11"; }
        total += 14;
        if (total % 15 == 1) { push log, "o12"; }
        total += 15;
        if (total % 16 == 1) { push log, "o13"; }
        total += 1;                                 ### outlined_last
    }
    return total;
}

# the return facts of callees that the native code of their callers can rely on: the code of each callee returns a
# value, while the path of a debugger's return request does not
auto sub facts_string(int i) {
    string s = "abc" + i;                           ### facts_string_first
    return s + "d";
}

string sub facts_typed_callee(int i) {
    string s = "abc" + i;                           ### facts_typed_first
    return s + "d";
}

int sub facts_take(string s) {
    return s.size();
}

auto sub facts_argument(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        try {
            total += facts_take(facts_string(i));
        } catch (hash<ExceptionInfo> ex) {
            push log, ex.err;
        }
    }
    return total;
}

auto sub facts_typed(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        total += facts_typed_callee(i).size();
    }
    return total;
}

# an immediately called closure, whose return facts the native code of a caller can rely on
int sub facts_take_int(int v) {
    return v + 1;
}

auto sub facts_closure(int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) {
        try {
            total += facts_take_int((auto sub () {
                int v = 40;                         ### facts_closure_first
                return v + 2;
            })());
        } catch (hash<ExceptionInfo> ex) {
            push log, ex.err;
        }
    }
    return total;
}

const Flows = {"return": DebugFlowReturn, "break": DebugFlowBreak, "continue": DebugFlowContinue};
FlowDebugger dbg(Flows{ARGV[1]}, ARGV[2].toInt(), ARGV[3].toInt());
ProgramControl pc = ProgramControl::getProgram();
string result;
Counter done(1);
background sub () {
    on_exit done.dec();
    dbg.addProgram(pc);
    on_exit dbg.removeProgram(pc);
    try {
        result = sprintf("%y", call_function(ARGV[0], 4));
    } catch (hash<ExceptionInfo> ex) {
        result = sprintf("%s: %s", ex.err, ex.desc);
    }
}();
done.waitForZero();
# the steps of the functions; the top-level code of AOT code is native code without step events
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, select dbg.steps, $1 > 38 && $1 < 169);
