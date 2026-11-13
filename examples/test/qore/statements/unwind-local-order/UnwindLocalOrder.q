# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The locals of a frame that an exception leaves through a block with handlers are released last declared first, as
# in the AST interpreter (#5920); see unwind-local-order.qtest

%modern

our list<string> log = ();

class T {
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        log += "~" + n;
    }
    m() {
        throw "M";
    }
}

# the issue's case: a local of the function and one of a block with an on_exit handler
sub a() {
    T z("a-outer");
    if (True) {
        on_exit log += "x";
        T t("a-inner");
        throw "A";
    }
}

# three levels, an on_error handler in the middle
sub b() {
    T z("b1");
    if (True) {
        on_error log += "e";
        T y("b2");
        if (True) {
            on_exit log += "x";
            T t("b3");
            throw "B";
        }
    }
}

# a loop: the locals of the iteration that the exception leaves, then those of the function
sub c() {
    T z("c-outer");
    for (int i = 0; i < 3; ++i) {
        on_exit log += "x" + i;
        T t("c" + i);
        if (i == 1) {
            throw "C";
        }
    }
}

# a block that ends normally: its handler and locals, then the function's locals at its end
sub d() {
    T z("d-outer");
    if (True) {
        on_exit log += "x";
        T t("d-inner");
    }
    log += "d-end";
}

# a return that a handler's exception interrupts: the function's local is released before the returned value
auto sub e() {
    T o("e-root");
    if (True) {
        on_exit { log += "x"; throw "E"; }
        T t("e");
        return t;
    }
}

# a return that a handler interrupts with an exception that is not raised by a throw statement: a division by zero, a
# closure, a method, a type error, a method call on no object
code cthr = sub () { throw "CTHR"; };
auto sub r1() { T z("r1-z"); if (True) { on_exit { log += "x"; int a = 0; int b = 1 / a; } T t("r1-t"); return t; } }
auto sub r2() { T z("r2-z"); if (True) { on_exit { log += "x"; cthr(); } T t("r2-t"); return t; } }
auto sub r3() { T z("r3-z"); if (True) { on_exit { log += "x"; z.m(); } T t("r3-t"); return t; } }
auto sub r4() { T z("r4-z"); if (True) { on_exit { log += "x"; hash<auto> h; string s = h.a.b; } T t("r4-t"); return t; } }
auto sub r5() { T z("r5-z"); if (True) { on_exit { log += "x"; auto v = "a"; int i = v; } T t("r5-t"); return t; } }
auto sub r6(*T a) { T z("r6-z"); if (True) { on_exit { log += "x"; a.m(); } T t("r6-t"); return t; } }

# a temporary of a statement that an exception leaves (the result of a fold) is released before the function's locals:
# it is not a value of a return
list<T> sub mkl(string p) { return (new T(p + "1"), new T(p + "2")); }
auto sub thr() { throw "THR"; }
auto sub two(auto a, auto b) { return a; }
sub f1() { T z("f1-z"); auto v = two((foldl $1, mkl("f1-l")), thr()); }

# a thread_exit in a handler that interrupts a return, and in a loop
auto sub t1() { T z("t1-z"); if (True) { on_exit { log += "x"; thread_exit; } T t("t1-t"); return t; } }
sub t2() { T z("t2-z"); for (int i = 0; i < 3; ++i) { on_exit log += "x" + i; T t("t2-" + i); if (i == 1) thread_exit; } }

sub run_thread(string fn) {
    log = ();
    Counter c(1);
    background sub () {
        on_exit c.dec();
        call_function(fn);
    }();
    c.waitForZero();
    printf("%s: %y\n", fn, log);
}

sub run() {
    # "native": compiled with qcc, whose native code releases the value of an interrupted return before the locals
    # of the function (temporary: https://github.com/qoretechnologies/qore/issues/5941), so e is left out
    list<string> fns = ARGV[0] == "native" ? ("a", "b", "c", "d")
        : ("a", "b", "c", "d", "e", "r1", "r2", "r3", "r4", "r5", "r6", "f1");
    foreach string fn in (fns) {
        log = ();
        try {
            call_function(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y\n", fn, log);
    }
}
run();
run_thread("t2");
if (ARGV[0] != "native") {
    run_thread("t1");
}
