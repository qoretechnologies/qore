# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The locals of a block that an exception leaves are released after its handlers and before the debugger's exception
# events of the statements that contain the block (#5867, #5921); see debugger-block-locals-unwind.qtest

%modern
%allow-debugger
class D inherits DebugProgram {
    public { list<string> seen = (); }
    onAttach(ProgramControl pgm, reference rs, reference rts) { rs = DebugRun; }
    onDetach(ProgramControl pgm, reference rs, reference rts) {}
    onStep(ProgramControl pgm, int b, *int s, *int bp, reference flow, reference rs, reference rts) { rs = DebugRun; }
    onFunctionEnter(ProgramControl pgm, int s, reference rs, reference rts) { rs = DebugRun; }
    onFunctionExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) { rs = DebugRun; }
    onException(ProgramControl pgm, int s, hash<auto> ex, reference dismiss, reference rs, reference rts) {
        rs = DebugRun;
        hash<auto> i = pgm.getStatementIdInfo(s);
        int l = i.start_line + i.offset;
        # the events of the functions below
        if (l >= FirstLine && l <= LastLine) {
            log += sprintf("%s@%d", ex.err, l);
        }
    }
    onExit(ProgramControl pgm, int s, reference result, reference rs, reference rts) {}
}
const FirstLine = 35;
const LastLine = 388;
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
# a destructor that raises an exception
class TX { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; throw "DX"; } }
our *D R;
# the debugger is attached before the top-level code runs; without DBG in the environment, the program runs without it
bool sub attach() { if (ENV.DBG) { R = new D(); R.addProgram(ProgramControl::getProgram()); } return True; }
const Attached = attach();

sub a() {
    if (True) {
        on_exit log += "x";
        T t("a");
        throw "A";
    }
}
list<code> cl;
sub b() {
    if (True) {
        on_exit log += "x";
        T t("b");
        int z = 5;
        cl += int sub () { return z; };
        throw "B";
    }
}
sub c() {
    for (int i = 0; i < 2; ++i) {
        on_exit log += "x" + i;
        T t("c" + i);
        if (i == 1) {
            throw "C";
        }
    }
}
sub d() {
    if (True) {
        on_error log += "e";
        T t("d1");
        if (True) {
            on_exit log += "x";
            T u("d2");
            throw "D";
        }
    }
}
sub e() {
    if (True) {
        on_exit log += "x";
        T t("e");
        auto r = \t;
        throw "E";
    }
}
# a return of the block's local that a handler's exception interrupts: the value of the return is kept until the
# function ends, as the AST interpreter keeps it, so the local is released then
T sub f() {
    if (True) {
        on_exit { log += "x"; throw "F"; }
        T t("f");
        return t;
    }
}
# an exception raised before the declaration of the block's local
sub g() {
    if (True) {
        on_exit log += "x";
        if (True) {
            throw "G";
        }
        T t("g");
    }
}
# a return in an inner block that a handler's exception interrupts: the returned local of the outer block is kept
T sub h() {
    if (True) {
        on_exit log += "x";
        T t("h");
        if (True) {
            on_exit { log += "y"; throw "H"; }
            return t;
        }
    }
}
# an interrupted return keeps only the returned local: the other local of the block is released with the block
T sub i() {
    if (True) {
        on_exit { log += "x"; throw "I"; }
        T t("i");
        T u("iu");
        return t;
    }
}
# a return value that a local holds through a closure, a reference, a container, or in a new container: kept until
# the function ends, however the locals reach it
auto sub n2() {
    if (True) {
        on_exit { log += "x"; throw "N2"; }
        T t("n2");
        code c = auto sub () { return t; };
        return t;
    }
}
auto sub n3() {
    if (True) {
        on_exit { log += "x"; throw "N3"; }
        T t("n3");
        reference r = \t;
        return r;
    }
}
auto sub n4() {
    if (True) {
        on_exit { log += "x"; throw "N4"; }
        list l = (new T("n4"),);
        return l[0];
    }
}
auto sub n5() {
    if (True) {
        on_exit { log += "x"; throw "N5"; }
        hash h = {"a": new T("n5")};
        return h.a;
    }
}
auto sub n6() {
    if (True) {
        on_exit { log += "x"; throw "N6"; }
        T t("n6");
        return (t,);
    }
}
# a block without handlers that an exception leaves through an inner block: its locals are released before the event
# of the statement that contains it (#5921)
sub k1() {
    T o("k1o");
    if (True) {
        T a("k1a");
        if (True) {
            on_exit log += "x";
            T b("k1b");
            throw "K1";
        }
    }
}
# two nested blocks without handlers
sub k2() {
    T o("k2o");
    if (True) {
        T a("k2a");
        if (True) {
            T b("k2b");
            if (True) {
                on_exit log += "x";
                throw "K2";
            }
        }
    }
}
# a block without handlers whose own statement raises the exception, in a try block
sub k3() {
    try {
        if (True) {
            T a("k3a");
            throw "K3";
        }
    } catch () {
        log += "c";
    }
    T z("k3z");
}
# a block without handlers that continues after an exception is caught in it, and that an exception leaves later
sub k4() {
    if (True) {
        T a("k4a");
        try {
            if (True) {
                on_exit log += "x";
                throw "K4A";
            }
        } catch () {
            log += "c";
        }
        T b("k4b");
        throw "K4B";
    }
}
# a block without handlers in a loop: each iteration's locals
sub k5() {
    foreach int i in (1, 2) {
        T a("k5a" + i);
        if (i == 2) {
            if (True) {
                on_exit log += "x";
                throw "K5";
            }
        }
    }
}
# a return of a local of a block without handlers that a handler's exception interrupts: the value is kept until the
# function ends, however the local reaches it
T sub m1() {
    if (True) {
        T t("m1");
        T u("m1u");
        if (True) {
            on_exit { log += "x"; throw "M1"; }
            return t;
        }
    }
}
auto sub m2() {
    if (True) {
        T t("m2");
        code c = auto sub () { return t; };
        if (True) {
            on_exit { log += "x"; throw "M2"; }
            return c();
        }
    }
}
auto sub m3() {
    if (True) {
        list l = (new T("m3"),);
        if (True) {
            on_exit { log += "x"; throw "M3"; }
            return l[0];
        }
    }
}
auto sub m4() {
    if (True) {
        T t("m4");
        reference r = \t;
        if (True) {
            on_exit { log += "x"; throw "M4"; }
            return r;
        }
    }
}
# two blocks without handlers around the interrupted return
T sub m5() {
    if (True) {
        T t("m5");
        if (True) {
            T u("m5u");
            if (True) {
                on_exit { log += "x"; throw "M5"; }
                return t;
            }
        }
    }
}
# a block with an exception path of its own (in a try block, or in a block with handlers) releases its locals after
# the exception events of the statements in it that the exception leaves (#5954)
sub v1() {
    try {
        if (True) {
            T a("v1");
            if (True) {
                throw "V1";
            }
        }
    } catch () {
        log += "c";
    }
}
sub v2() {
    if (True) {
        T a("v2a");
        if (True) {
            on_exit log += "x";
            if (True) {
                T b("v2b");
                if (True) {
                    throw "V2";
                }
            }
        }
    }
}
sub v3() {
    if (True) {
        on_error log += "oe";
        if (True) {
            T a("v3");
            if (True) {
                throw "V3";
            }
        }
    }
}
sub v4() {
    T o("v4o");
    try {
        T a("v4a");
        if (True) {
            T b("v4b");
            throw "V4A";
        }
    } catch () {
        if (True) {
            throw "V4";
        }
    }
}
sub v5() {
    foreach int i in (1, 2) {
        try {
            T a("v5_" + i);
            if (i == 2) {
                throw "V5";
            }
        } catch () {
            log += "c";
            rethrow;
        }
    }
}

# an exception that a destructor raises at the end of a block or of a statement, in a block with an exception path of
# its own (#5954)
sub w1() {
    try {
        if (True) {
            T a("w1");
            if (True) {
                T b("w1b");
                if (True) {
                    TX x("w1x");
                }
            }
        }
    } catch () {
        log += "c";
    }
}
sub w2() {
    try {
        if (True) {
            T a("w2");
            if (True) {
                on_exit log += "x";
                TX x("w2x");
            }
        }
    } catch () {
        log += "c";
    }
}
sub w3() {
    try {
        if (True) {
            T a("w3");
            new TX("w3t");
            log += "n";
        }
    } catch () {
        log += "c";
    }
}

foreach string fn in (("a", "b", "c", "d", "e", "f", "g", "h", "i", "n2", "n3", "n4", "n5", "n6", "k1", "k2", "k3",
        "k4", "k5", "m1", "m2", "m3", "m4", "m5", "v1", "v2", "v3", "v4", "v5", "w1", "w2", "w3")) {
    log = ();
    try { call_function(fn); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    printf("%s: %y\n", fn, log);
}
printf("closure: %y\n", (map $1(), cl));
