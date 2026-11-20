%modern
%allow-debugger
# a handler declared in the root block of a handler that runs inline at an exit of a loop (qore#6905): with no
# debugger attached, ARGV[0] is a function to call; with a request, the debugger requests a flow at the hit-th step
# event of the statement on a line in that function (the lines of the statements tagged "### <tag>")
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

# the shape of qore#6905: a loop body leaves by an exception
auto sub loopThrow(int n) {
    on_exit {
        on_exit {
            push log, "inner";                      ### throw_inner
            push log, "inner2";
        }
        push log, "outer";
    }
    int total = 0;
    while (True) {
        on_exit {
            on_exit {
                push log, "loop-inner" + total;     ### throw_loop_inner
            }
        }
        if (++total == n) {
            throw "X", "loop";
        }
    }
}

# a return in a loop body
auto sub loopReturn(int n) {
    on_exit {
        on_exit {
            push log, "inner";                      ### return_inner
        }
        push log, "outer";
    }
    int total = 0;
    for (int i = 0; i < 10; ++i) {
        on_exit {
            on_exit {
                push log, "loop-inner" + i;         ### return_loop_inner
            }
        }
        if (++total == n) {
            return total;
        }
    }
    return -1;
}

# a return from a case of a switch statement in a loop body
auto sub switchBreak(int n) {
    on_exit {
        on_exit {
            push log, "inner";                      ### switch_inner
        }
    }
    int total = 0;
    foreach int i in (xrange(10)) {
        switch (i) {
            case 2:
                on_exit {
                    on_exit {
                        push log, "case-inner" + i; ### switch_case_inner
                    }
                }
                return i;
            default:
                ++total;
        }
    }
    return total;
}

# a handler declared in the body of an outer loop, run inline at a return out of an inner loop: a request in the
# handler of its body does not take the inner loop, which the return left, but takes the outer loop, which encloses
# the handler
auto sub nestedLoops(int n) {
    int total = 0;
    for (int o = 0; o < 3; ++o) {
        on_exit {
            on_exit {
                push log, "outer-inner" + o;        ### nested_inner
            }
            push log, "outer" + o;
        }
        for (int i = 0; i < 3; ++i) {
            if (o == 1 && i == n - 2) {
                return total;
            }
            ++total;
        }
    }
    return -1;
}

const Flows = {"return": DebugFlowReturn, "break": DebugFlowBreak, "continue": DebugFlowContinue};
string result;
*FlowDebugger dbg;
if (ARGV.size() > 1) {
    dbg = new FlowDebugger(Flows{ARGV[1]}, ARGV[2].toInt(), ARGV[3].toInt());
}
ProgramControl pc = ProgramControl::getProgram();
Counter done(1);
background sub () {
    on_exit done.dec();
    if (dbg) {
        dbg.addProgram(pc);
    }
    on_exit if (dbg) {
        dbg.removeProgram(pc);
    }
    try {
        result = sprintf("%y", call_function(ARGV[0], 3));
    } catch (hash<ExceptionInfo> ex) {
        result = sprintf("%s: %s", ex.err, ex.desc);
    }
}();
done.waitForZero();
# the steps of the functions (the lines from the first function to the top-level code); the top-level code of AOT code
# is native code without step events
list<string> src = ReadOnlyFile::readTextFile(get_script_path()).split("\n");
int first_line = 0;
int last_line = 0;
for (int i = 0; i < src.size(); ++i) {
    if (!first_line && src[i] =~ /^auto sub /) {
        first_line = i + 1;
    } else if (src[i] =~ /^const Flows /) {
        last_line = i + 1;
        break;
    }
}
if (!first_line || !last_line) {
    throw "TEST-ERROR", sprintf("the lines of the functions were not found (%d, %d)", first_line, last_line);
}
printf("RESULT:%s LOG:%y STEPS:%y\n", result, log, dbg
    ? (select dbg.steps, $1 >= first_line && $1 < last_line) : ());
