# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The locals of a block that an exception leaves are released after its handlers and before the debugger's exception
# events of the statements that contain the block (#5867); see debugger-block-locals-unwind.qtest

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
const LastLine = 157;
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
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
foreach string fn in (("a", "b", "c", "d", "e", "f", "g", "h", "i", "n2", "n3", "n4", "n5", "n6")) {
    log = ();
    try { call_function(fn); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
    printf("%s: %y\n", fn, log);
}
printf("closure: %y\n", (map $1(), cl));
