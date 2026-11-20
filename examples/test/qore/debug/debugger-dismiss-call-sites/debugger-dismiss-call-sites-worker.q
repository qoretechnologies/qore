#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the worker of debugger-dismiss-call-sites.qtest: a debugger that attaches while a function runs dismisses the
# exception events of calls in its statements (in map, foldl and select operators, a concatenation and a hash), raised
# in try blocks; the function continues after each statement as the AST interpreter does, without the values of the
# statement, which the IR interpreter took from an earlier execution of the code (issue #6626)

%modern
%allow-debugger

#! the lines of the statements whose exception events are dismissed: those between the markers in run_cases()
const Lines = get_case_lines();

#! returns the first and the last line of the cases, between the markers of run_cases()
list<int> sub get_case_lines() {
    list<string> src = ReadOnlyFile::readTextFile(get_script_path()).split("\n");
    int first = -1;
    int last = -1;
    foreach string line in (src) {
        if (line =~ /# cases: begin$/) {
            first = $# + 2;
        } else if (line =~ /# cases: end$/) {
            last = $#;
        }
    }
    return (first, last);
}

#! dismisses the exception events of the statements of run_cases()
class Dismisser inherits DebugProgram {
    onAttach(ProgramControl pgm, reference rs, reference rts) {
        rs = DebugRun;
    }
    onDetach(ProgramControl pgm, reference rs, reference rts) {
    }
    onStep(ProgramControl pgm, int bid, *int sid, *int bkid, reference flow, reference rs, reference rts) {
    }
    onFunctionEnter(ProgramControl pgm, int sid, reference rs, reference rts) {
    }
    onFunctionExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) {
    }
    onException(ProgramControl pgm, int sid, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        hash<auto> i = pgm.getStatementIdInfo(sid);
        int line = i.start_line + i.offset;
        if (ex.err == "T" && line >= Lines[0] && line <= Lines[1]) {
            dismiss = True;
        }
    }
    onExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) {
    }
}

#! an object whose destructor raises an exception
class D {
    destructor() {
        throw "DTOR";
    }
    string get() {
        return "d";
    }
}

#! raises an exception for an odd number
string sub thrower(int i) {
    if (i % 2) {
        throw "T";
    }
    return "v" + i;
}

Queue attach();
Queue attached();
Queue done();

our string out = "";

#! runs the statements after the debugger attached: the frame runs its code since before, in every execution mode;
#! each case runs twice, so that a value of the first execution of a statement is there for the second; the output
#! goes to "out", as the last case, a return statement, ends the function
*string sub run_cases(string a1) {
    attach.push(1);
    attached.get();
    string s = "";
    for (int i = 0; i < 4; ++i) {
        # cases: begin
        # i odd: the first element raises; i even: the second
        try {
            string v = (map thrower($1 + i) + "y", (0, 1, 2)).join(",");
            s += "|map:" + v;
        } catch (hash<ExceptionInfo> ex) {
            s += "|map:C";
        }
        try {
            string v = (map thrower($1 + i), (0, 1, 2)).join(",");
            s += "|join:" + v;
        } catch (hash<ExceptionInfo> ex) {
            s += "|join:C";
        }
        try {
            string v = foldl $1 + thrower($2 + i), ("a", 1, 2);
            s += "|foldl:" + v;
        } catch (hash<ExceptionInfo> ex) {
            s += "|foldl:C";
        }
        try {
            string v = (select (1, 2, 3), thrower($1 + i) == "v2").join(",");
            s += "|select:" + v;
        } catch (hash<ExceptionInfo> ex) {
            s += "|select:C";
        }
        try {
            string v = thrower(i) + "z";
            s += "|concat:" + v;
        } catch (hash<ExceptionInfo> ex) {
            s += "|concat:C";
        }
        try {
            hash<auto> h = {"a": thrower(i), "b": i};
            s += "|hash:" + h.size();
        } catch (hash<ExceptionInfo> ex) {
            s += "|hash:C";
        }
        # the list of a foreach, iterated lazily: from its second element on, the statements of the body that ran are
        # recorded inside the foreach
        try {
            foreach auto x in (map thrower($1 + i), (0, 1, 2)) {
                s += sprintf("|fe:%y", x);
            }
        } catch (hash<ExceptionInfo> ex) {
            s += "|fe:C";
        }
        # the implicit arguments of a map whose expression uses them at run time are restored
        try {
            string v = (map thrower($1 + i) + ($2 ?? "x"), (0, 1, 2)).join(",");
            s += "|args:" + v + "/" + $1;
        } catch (hash<ExceptionInfo> ex) {
            s += "|args:C/" + $1;
        }
        # the temporaries of the statement are released with the exception, before its event, as the AST interpreter
        # releases them: the exception of a destructor is dismissed with it; one raised before (by the destructor of
        # the temporary of an element that ended) is caught
        try {
            string v = (map (new D()).get() + thrower($1 + i), (0, 1, 2)).join(",");
            s += "|dtor:" + v;
        } catch (hash<ExceptionInfo> ex) {
            s += "|dtor:C:" + ex.err;
        }
        s += "\n";
    }
    out = s;
    # a return statement: the function ends without a value
    try {
        return "R:" + (map thrower($1) + "y", (0, 1, 2)).join(",");
        # cases: end
    } catch (hash<ExceptionInfo> ex) {
        out += "return:C\n";
    }
    out += "after return\n";
    return "end";
}

Dismisser d();
ProgramControl pc = ProgramControl::getProgram();
background sub () {
    attach.get();
    d.addProgram(pc);
    on_exit d.removeProgram(pc);
    attached.push(1);
    done.get();
}();
try {
    # called through a call reference, so that the function's frame runs its IR in the jit and tiered modes too: such
    # a call runs a function on the IR tier and compiles it when it returns (see ExecModeHarness)
    code run = \run_cases();
    *string rv = run("ARG1");
    print(out);
    printf("rv: %y\n", rv);
} catch (hash<ExceptionInfo> ex) {
    printf("EXC %s: %s\n", ex.err, ex.desc);
}
done.push(1);
