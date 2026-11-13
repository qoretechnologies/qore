# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The block step events of handler bodies and catch blocks with a debugger attached (#5870); see
# aot-debug-ir-handler-block-steps.qtest

%modern
%allow-debugger

#! records the steps of the functions below: the line of each statement, "b" for a block step and ERR@line for an
#! exception event
class Recorder inherits DebugProgram {
    public {
        list<string> seen = ();
    }
    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugStep;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int b, *int s, *int bp, reference flow, reference rs, reference rts) {
        rs = DebugStep;
        if (!s) {
            # a block step is recorded within the functions only, which the statement steps around it show
            if (inFunctions) {
                seen += "b";
            }
            return;
        }
        hash<auto> i = pgm.getStatementIdInfo(s);
        int l = i.start_line + i.offset;
        inFunctions = l >= FirstLine && l <= LastLine;
        if (inFunctions) {
            seen += l.toString();
        }
    }
    onFunctionEnter(ProgramControl pgm, int s, reference rs, reference rts) {
        rs = DebugStep;
    }
    onFunctionExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {
        rs = DebugStep;
    }
    onException(ProgramControl pgm, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugStep;
        hash<auto> i = pgm.getStatementIdInfo(s);
        int l = i.start_line + i.offset;
        if (l >= FirstLine && l <= LastLine) {
            seen += sprintf("%s@%d", ex.err, l);
        }
    }
    onExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {
    }

    private {
        bool inFunctions;
    }
}

const FirstLine = 74;
const LastLine = 168;

# the debugger is attached before the top-level code runs, so that the top-level code compiled with qcc runs its debug
# IR, and calls the functions through it
our Recorder R;
bool sub attach() {
    R = new Recorder();
    R.addProgram(ProgramControl::getProgram());
    return True;
}
const Attached = attach();

# the lines from here to the end of the top-level block below are recorded

# the handlers of a block that ends normally, and a call in the block
int sub twice(int v) {
    return v * 2;
}
int sub normal() {
    int a = 1;
    {
        on_exit a += 10;
        on_error a += 100;
        on_success a += 1000;
        a = twice(a);
    }
    return a;
}

# the handlers of a block that an exception leaves, and the catch block
string sub error() {
    string s = "";
    try {
        on_exit s += "x";
        on_error {
            s += "e";
            if (True) {
                s += "n";
            }
        }
        throw "ERR";
    } catch (hash<ExceptionInfo> ex) {
        s += "c";
        if (True) {
            s += "i";
        }
    }
    return s;
}

# a handler in a handler, and a catch block in a handler
string sub nested() {
    string s = "";
    {
        on_exit {
            on_exit s += "2";
            s += "1";
            try {
                throw "IN";
            } catch () {
                s += "c";
            }
        }
        s += "0";
    }
    return s;
}

# a handler compiled to an IR function of its own in another one
string sub nestedc() {
    string s = "";
    try {
        on_error {
            on_error s += "E2";
            on_exit s += "X2";
            s += "E1";
            throw "N2";
        }
        throw "N1";
    } catch () {
        s += "c";
    }
    return s;
}

# the handlers of a closure
string sub clos() {
    string s = "";
    code c = sub () {
        on_exit s += "cx";
        on_error s += "ce";
        s += "cb";
    };
    c();
    return s;
}


printf("normal: %d\n", normal());
printf("error: %s\n", error());
printf("nested: %s\n", nested());
printf("nestedc: %s\n", nestedc());
printf("clos: %s\n", clos());
# the handlers of a block of the top-level code
{
    on_exit printf("top on_exit\n");
    on_success printf("top on_success\n");
    on_error printf("top on_error\n");
    printf("top\n");
}
R.removeProgram(ProgramControl::getProgram());
printf("%s\n", R.seen.join(" "));
