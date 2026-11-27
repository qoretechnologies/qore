# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# With %broken-loop-statement, a switch statement takes a continue as it takes a break (qore#7606): ARGV[0] is the
# function to call, ARGV[1] the request ("break" or "continue") that a debugger makes at the first step event of the
# statement tagged "### <function>" in a handler that a return runs; prints the function's result and the log, in which
# the destructor of the object of the return that the request abandons records the moment of its release
%new-style
%require-types
%strict-args
%broken-loop-statement
%allow-debugger

class FlowDebugger inherits DebugProgram {
    public {
        int flow;
        int line;
        int hits = 0;
    }
    constructor(int flow, int line) {
        self.flow = flow;
        self.line = line;
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugStep; }
    onDetach(ProgramControl pgm, reference rs, reference rts) { rs = DebugDetach; }
    onStep(ProgramControl pgm, int blk, *int sid, *int bid, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (!sid) {
            return;
        }
        if (pgm.getStatementIdInfo(sid).start_line == line && ++hits == 1) {
            flow = self.flow;
        }
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) { rs = DebugStep; }
    onFunctionExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {}
    onExit(ProgramControl pgm, int sid, reference rv, reference rs, reference rts) {}
}

our list<string> log = ();

class ReturnMarker {
    destructor() {
        push log, "released";
    }
}

# a switch statement with no loop around it takes the request: the return is abandoned where the request leaves the
# case's block, before the code after the switch statement runs
auto sub noLoop() {
    switch (1) {
        case 1:
            on_exit {
                push log, "handler";                ### noLoop
            }
            return new ReturnMarker();
    }
    push log, "after";
    return -7;
}

# the same in a block of the case
auto sub nestedBlock() {
    switch (1) {
        case 1: {
            {
                on_exit {
                    push log, "handler";            ### nestedBlock
                }
                return new ReturnMarker();
            }
        }
    }
    push log, "after";
    return -7;
}

# the handler of a block of the case that the request passes runs after the abandoned return is released, as with a
# break
auto sub passedHandler() {
    switch (1) {
        case 1: {
            on_exit {
                push log, "passed";
            }
            {
                on_exit {
                    push log, "handler";            ### passedHandler
                }
                return new ReturnMarker();
            }
        }
    }
    push log, "after";
    return -7;
}

# the same in a handler's body
auto sub passedInHandler() {
    on_exit {
        switch (1) {
            case 1: {
                on_exit {
                    push log, "passed";
                }
                {
                    on_exit {
                        push log, "handler";        ### passedInHandler
                    }
                    return new ReturnMarker();
                }
            }
        }
        push log, "after";
    }
    return -7;
}

# a switch statement in a loop takes the request, not the loop
auto sub inLoop() {
    for (int i = 0; i < 1; ++i) {
        switch (i) {
            case 0:
                on_exit {
                    push log, "handler";            ### inLoop
                }
                return new ReturnMarker();
        }
        push log, "after";
    }
    return -7;
}

# a switch statement in the body of a handler that a return runs: the function's return resumes after the switch
# statement takes the request
auto sub inHandler() {
    on_exit {
        switch (1) {
            case 1:
                on_exit {
                    push log, "handler";            ### inHandler
                }
                return new ReturnMarker();
        }
        push log, "after";
    }
    return -7;
}

const Flows = {"break": DebugFlowBreak, "continue": DebugFlowContinue};

int line;
{
    list<string> src = ReadOnlyFile::readTextFile(get_script_path()).split("\n");
    for (int i = 0; i < src.size(); ++i) {
        *list<*string> tag = src[i] =~ x/### ([A-Za-z]+)$/;
        if (tag && tag[0] == ARGV[0]) {
            line = i + 1;
            break;
        }
    }
    if (!line) {
        throw "TEST-ERROR", sprintf("no statement is tagged %y", ARGV[0]);
    }
}

FlowDebugger dbg(Flows{ARGV[1]}, line);
ProgramControl pc = ProgramControl::getProgram();
string result;
Counter done(1);
background sub () {
    on_exit done.dec();
    dbg.addProgram(pc);
    on_exit dbg.removeProgram(pc);
    try {
        result = sprintf("%y", call_function(ARGV[0]));
    } catch (hash<ExceptionInfo> ex) {
        result = sprintf("%s: %s", ex.err, ex.desc);
    }
}();
done.waitForZero();
printf("RESULT:%s LOG:%y HITS:%d\n", result, log, dbg.hits);
