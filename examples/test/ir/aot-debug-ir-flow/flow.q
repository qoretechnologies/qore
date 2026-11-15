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
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, select dbg.steps, $1 > 38 && $1 < 75);
