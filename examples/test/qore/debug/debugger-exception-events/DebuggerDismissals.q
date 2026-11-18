#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a debugger dismisses the n-th exception event of functions that raise exceptions in nested statements, loops, try and
# catch blocks, on_error code, closures, switch statements and recursive calls, and the script prints the events and
# the result of each function; run by debugger-exception-events.qtest in every execution mode and compiled ahead of
# time, where the functions run their debug IR and must continue after the statement whose exception was dismissed as
# the AST interpreter does (#5816)

%modern
%allow-debugger

#! records the exception events of the functions below and dismisses the given event of an exception
class Dismisser inherits DebugProgram {
    public {
        list<string> seen = ();
        *string derr;
        int nth;
        int cnt;
    }

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
        # only the statements of the functions below, not those of the calls in the thread that runs them
        if (i.start_line + i.offset < FirstLine || i.start_line + i.offset > LastLine) {
            return;
        }
        seen += sprintf("%s@%d", ex.err, i.start_line + i.offset);
        if (ex.err == derr && ++cnt == nth) {
            dismiss = True;
        }
    }
    onExit(ProgramControl pgm, int sid, reference result, reference rs, reference rts) {
    }
}

#! the first and the last line of the functions
const FirstLine = 53;
const LastLine = 172;

*string sub a_t_local(int x) {
    string s = "kept";
    if (x) {
        throw "A";
    }
    return s;
}
*string sub a_t_loop() {
    string r = "";
    foreach int i in (1, 2, 3) {
        if (i == 2) {
            throw "B";
        }
        r += i;
    }
    return r;
}
*string sub a_t_try(int x) {
    string s = "k";
    try {
        if (x) {
            throw "C";
        }
        s += "t";
    } catch (hash<ExceptionInfo> ex) {
        s += "c";
    }
    return s;
}
*string sub a_t_onerr(int x) {
    string s = "k";
    {
        on_error s += "E";
        on_exit s += "X";
        if (x) {
            throw "D";
        }
        s += "n";
    }
    return s + "|";
}
*string sub a_t_while() {
    int i = 0;
    string r = "";
    while (i < 3) {
        ++i;
        r += i;
        if (i == 2) { throw "W"; }
    }
    return r;
}
*string sub b_t_local(int x) {
    string s = "kept"; int z = 0;
    if (x) {
        int y = 1 / z;
    }
    return s;
}
*string sub b_t_loop() {
    string r = ""; int z = 0;
    foreach int i in (1, 2, 3) {
        if (i == 2) {
            int y = 1 / z;
        }
        r += i;
    }
    return r;
}
*string sub b_t_rec(int n) {
    string s = "r" + n; int z = 0;
    if (n > 0) {
        s += b_t_rec(n - 1);
    } else {
        int y = 1 / z;
    }
    return s;
}
*string sub b_t_nested(int x) {
    string s = "k";
    try {
        try {
            throw "N";
        } catch (hash<ExceptionInfo> ex) {
            s += "1";
            rethrow;
        }
    } catch (hash<ExceptionInfo> ex) {
        s += "2";
    }
    return s;
}
*string sub b_t_cl(int x) {
    string s = "k";
    code c = sub () { int z = 0; s += "a"; int y = 1 / z; s += "b"; };
    c();
    return s;
}
*string sub b_t_sw(int x) {
    string s = "k"; int z = 0;
    switch (x) {
        case 1: s += "1"; int y = 1 / z; s += "x";
        case 2: s += "2"; break;
    }
    return s;
}
*string sub c_t_fe(int x) {
    string r = "";
    list<auto> l = (1, "x", 3);
    foreach int i in (l) {
        r += i;
    }
    return r + "|";
}
*string sub c_t_wh(int x) {
    string r = ""; int z = 0; int i = 0;
    while (i < 2 || (1 / z)) {
        r += ++i;
    }
    return r + "|";
}

#! the cases: function, exception code, the event dismissed, arguments
const Cases = (
    ("a_t_local", "A", 1, (1,)), ("a_t_local", "A", 2, (1,)),
    ("a_t_loop", "B", 1, ()), ("a_t_loop", "B", 2, ()), ("a_t_loop", "B", 3, ()),
    ("a_t_try", "C", 1, (1,)), ("a_t_try", "C", 2, (1,)),
    ("a_t_onerr", "D", 1, (1,)), ("a_t_onerr", "D", 2, (1,)), ("a_t_onerr", "D", 3, (1,)),
    ("a_t_while", "W", 1, ()), ("a_t_while", "W", 2, ()), ("a_t_while", "W", 3, ()),
    ("b_t_local", "DIVISION-BY-ZERO", 1, (1,)), ("b_t_local", "DIVISION-BY-ZERO", 2, (1,)),
    ("b_t_loop", "DIVISION-BY-ZERO", 1, ()), ("b_t_loop", "DIVISION-BY-ZERO", 2, ()),
    ("b_t_loop", "DIVISION-BY-ZERO", 3, ()),
    ("b_t_rec", "DIVISION-BY-ZERO", 1, (2,)), ("b_t_rec", "DIVISION-BY-ZERO", 2, (2,)),
    ("b_t_rec", "DIVISION-BY-ZERO", 3, (2,)), ("b_t_rec", "DIVISION-BY-ZERO", 4, (2,)),
    ("b_t_nested", "N", 1, (1,)), ("b_t_nested", "N", 2, (1,)), ("b_t_nested", "N", 3, (1,)),
    ("b_t_nested", "N", 4, (1,)),
    ("b_t_cl", "DIVISION-BY-ZERO", 1, (1,)), ("b_t_cl", "DIVISION-BY-ZERO", 2, (1,)),
    ("b_t_cl", "DIVISION-BY-ZERO", 3, (1,)),
    ("b_t_sw", "DIVISION-BY-ZERO", 1, (1,)), ("b_t_sw", "DIVISION-BY-ZERO", 2, (1,)),
    ("b_t_sw", "DIVISION-BY-ZERO", 3, (1,)),
    ("c_t_fe", "RUNTIME-TYPE-ERROR", 1, (1,)), ("c_t_wh", "DIVISION-BY-ZERO", 1, (1,)),
);

Dismisser r();
ProgramControl pc = ProgramControl::getProgram();
Counter cnt(1);
background sub () {
    on_exit cnt.dec();
    r.addProgram(pc);
    on_exit r.removeProgram(pc);
    foreach list<auto> d in (Cases) {
        r.seen = ();
        r.derr = d[1];
        r.nth = d[2];
        r.cnt = 0;
        try {
            auto rv = call_function_args(d[0], d[3]);
            printf("%s/%d: %y rv=%y\n", d[0], d[2], r.seen, rv);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s/%d: %y EXC %s\n", d[0], d[2], r.seen, ex.err);
        }
    }
}();
cnt.waitForZero();
