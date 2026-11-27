%modern
%allow-debugger
# a second debugger request in a handler that the code of a first request runs, in the body of a handler of a loop
# body (qore#7008): with no debugger attached, ARGV[0] is a function to call; with a request, the debugger requests a
# flow at the hit-th step event of the statement on a line in that function (the lines of the statements tagged
# "### <tag>"), and optionally a second one (ARGV[4..6]); in the functions, O is the handler of a loop body, "in" the
# handler of the first request and "mid" that of the second, which the code of the first request runs (qore#7008)
class FlowDebugger inherits DebugProgram {
    public {
        list<auto> steps = ();
        int flow;
        int line;
        int hit;
        int hits = 0;
        #! the second request, if any
        int flow2 = -1;
        int line2;
        int hit2;
        int hits2 = 0;
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
        if (flow2 >= 0 && l == line2 && ++hits2 == hit2) {
            flow = flow2;
        }
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
}

our list<string> log = ();

# L4: a loop handler O; in its body a block with mid (no return) and an inner block with a handler
auto sub L4(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            {
                on_exit {
                    push log, "mid" + i;            ### l4_mid
                }
                {
                    on_exit {
                        push log, "in" + i;         ### l4_in
                    }
                    push log, "x" + i;
                }
                push log, "y" + i;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# L4R: as L4, mid can return
auto sub L4R(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            {
                on_exit {
                    push log, "mid" + i;            ### l4r_mid
                    if (n > 100) {
                        return -5;
                    }
                }
                {
                    on_exit {
                        push log, "in" + i;         ### l4r_in
                    }
                    push log, "x" + i;
                }
                push log, "y" + i;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# L1: mid in the top block of O's body
auto sub L1(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            on_exit {
                push log, "mid" + i;                ### l1_mid
            }
            {
                on_exit {
                    push log, "in" + i;             ### l1_in
                }
                push log, "x" + i;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# L1R: as L1, mid can return
auto sub L1R(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            on_exit {
                push log, "mid" + i;                ### l1r_mid
                if (n > 100) {
                    return -5;
                }
            }
            {
                on_exit {
                    push log, "in" + i;             ### l1r_in
                }
                push log, "x" + i;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# L5: deeper blocks between mid and the inner handler
auto sub L5(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            {
                on_exit {
                    push log, "mid" + i;            ### l5_mid
                }
                {
                    {
                        on_exit {
                            push log, "in" + i;     ### l5_in
                        }
                        push log, "x" + i;
                    }
                    push log, "z" + i;
                }
                push log, "y" + i;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# loopRet: the loop handler can return; a handler in its body runs a request, then mid in a block around it
auto sub loopRet(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            push log, "h" + i;
            if (n > 100) {
                return -5;
            }
            {
                on_exit {
                    push log, "mid" + i;            ### loop_ret_mid
                }
                {
                    on_exit {
                        push log, "in" + i;         ### loop_ret_in
                    }
                    push log, "x" + i;
                }
            }
            push log, "h2" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# loopRet2: as loopRet, mid can return too
auto sub loopRet2(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            push log, "h" + i;
            if (n > 100) {
                return -5;
            }
            {
                on_exit {
                    push log, "mid" + i;            ### loop_ret2_mid
                    if (n > 100) {
                        return -6;
                    }
                }
                {
                    on_exit {
                        push log, "in" + i;         ### loop_ret2_in
                    }
                    push log, "x" + i;
                }
            }
            push log, "h2" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# midNestLoop: a loop of O's body around mid's block
auto sub midNestLoop(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            for (int j = 0; j < 2; ++j) {
                on_exit {
                    push log, "mid" + i + j;        ### mid_nest_loop_mid
                }
                {
                    on_exit {
                        push log, "in" + i + j;     ### mid_nest_loop_in
                    }
                    push log, "x" + i + j;
                }
                push log, "y" + i + j;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# midNestLoopR: as midNestLoop, mid can return
auto sub midNestLoopR(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            for (int j = 0; j < 2; ++j) {
                on_exit {
                    push log, "mid" + i + j;        ### mid_nest_loop_r_mid
                    if (n > 100) {
                        return -5;
                    }
                }
                {
                    on_exit {
                        push log, "in" + i + j;     ### mid_nest_loop_r_in
                    }
                    push log, "x" + i + j;
                }
                push log, "y" + i + j;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# twoLoopHandlers: a second handler of the loop body, which runs after O
auto sub twoLoopHandlers(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            push log, "a" + i;                      ### two_loop_mid
            if (n > 100) {
                return -5;
            }
        }
        on_exit {
            {
                on_exit {
                    push log, "in" + i;             ### two_loop_in
                }
                push log, "x" + i;
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

# retWalk: as L4, at a return out of the loop
auto sub retWalk(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            {
                on_exit {
                    push log, "mid" + i;            ### ret_walk_mid
                }
                {
                    on_exit {
                        push log, "in" + i;         ### ret_walk_in
                    }
                    push log, "x" + i;
                }
            }
            push log, "o" + i;
        }
        if (i == 1) {
            return n + 10;
        }
        push log, "b" + i;
    }
    return n;
}

# nonLastOuter: a return request in a handler that is not the last of a loop body, then a request in the handler of
# the function's block that the return runs
auto sub nonLastOuter(int n) {
    on_exit {
        push log, "f";                              ### non_last_outer_mid
        push log, "f2";
    }
    for (int i = 0; i < 3; ++i) {
        on_exit {
            push log, "a" + i;
        }
        on_exit {
            push log, "z" + i;                      ### non_last_outer_in
        }
        push log, "b" + i;
    }
    return n;
}

# twoLoopTop: a return request in O, then a request in A, which runs after O
auto sub twoLoopTop(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            push log, "a" + i;                      ### two_loop_top_mid
            push log, "a2" + i;
        }
        on_exit {
            push log, "o" + i;                      ### two_loop_top_in
        }
        push log, "b" + i;
    }
    return n;
}

# retWalkR: as retWalk, mid can return
auto sub retWalkR(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            {
                on_exit {
                    push log, "mid" + i;            ### ret_walk_r_mid
                    if (n > 100) {
                        return -5;
                    }
                }
                {
                    on_exit {
                        push log, "in" + i;         ### ret_walk_r_in
                    }
                    push log, "x" + i;
                }
            }
            push log, "o" + i;
        }
        if (i == 1) {
            return n + 10;
        }
        push log, "b" + i;
    }
    return n;
}

# handOver: in the block of mid, a handler that returns a value runs before mid, so that, past the budget of the
# code of the requests, the handlers run out of line hand that value over before mid's request
auto sub handOver(int n) {
    for (int i = 0; i < 3; ++i) {
        on_exit {
            {
                on_exit {
                    push log, "mid" + i;                ### hand_over_mid
                }
                on_exit {
                    push log, "r" + i;
                    if (n > 1) {
                        return -5;
                    }
                }
                {
                    on_exit {
                        push log, "in" + i;             ### hand_over_in
                    }
                    push log, "x" + i;
                }
            }
            push log, "o" + i;
        }
        push log, "b" + i;
    }
    return n;
}

const Flows ={"return": DebugFlowReturn, "break": DebugFlowBreak, "continue": DebugFlowContinue};
string result;
*FlowDebugger dbg;
if (ARGV.size() > 1) {
    dbg = new FlowDebugger(Flows{ARGV[1]}, ARGV[2].toInt(), ARGV[3].toInt());
    if (ARGV.size() > 4) {
        dbg.flow2 = Flows{ARGV[4]};
        dbg.line2 = ARGV[5].toInt();
        dbg.hit2 = ARGV[6].toInt();
    }
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
