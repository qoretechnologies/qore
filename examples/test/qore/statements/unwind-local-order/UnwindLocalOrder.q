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

# a try statement in the function that catches the exception that interrupted a return releases its value before
# the catch block runs (#5941): from a nested block, from the try block itself, through two blocks with locals, and
# from a return in a catch block, which the enclosing try statement catches
T sub u1() {
    try {
        if (True) {
            T t("u1");
            if (True) {
                on_exit { log += "x"; throw "U1"; }
                return t;
            }
        }
    } catch () {
        log += "c";
    }
    return new T("u1-other");
}
T sub u2() {
    try {
        T t("u2");
        on_exit { log += "x"; throw "U2"; }
        return t;
    } catch () {
        log += "c";
    }
    return new T("u2-other");
}
T sub u3() {
    T z("u3-z");
    try {
        T a("u3-a");
        if (True) {
            T b("u3-b");
            if (True) {
                on_exit { log += "x"; throw "U3"; }
                return b;
            }
        }
    } catch () {
        log += "c";
    }
    return new T("u3-other");
}
T sub u4() {
    try {
        try {
            throw "FIRST";
        } catch () {
            T t("u4");
            on_exit { log += "x"; throw "U4"; }
            return t;
        }
    } catch () {
        log += "c";
    }
    return new T("u4-other");
}

# an outer try statement catches the exception that a handler between it and an inner try statement raises
T sub u5() {
    try {
        if (True) {
            on_exit { log += "x"; throw "U5"; }
            try {
                T t("u5");
                return t;
            } catch () {
                log += "ic";
            }
        }
    } catch () {
        log += "c";
    }
    return new T("u5-other");
}
# the value of the return is a temporary
T sub u6() {
    try {
        if (True) {
            on_exit { log += "x"; throw "U6"; }
            return new T("u6");
        }
    } catch () {
        log += "c";
    }
    return new T("u6-other");
}
# the locals of the try block are released before the value of the return
T sub u7() {
    try {
        T q("u7-q");
        on_exit { log += "x"; throw "U7"; }
        T t("u7");
        return t;
    } catch () {
        log += "c";
    }
    return new T("u7-other");
}

# the functions are called directly, so that the jit mode runs their native code
sub run_returns() {
    hash<string, code> fns = {
        "u1": sub () { T r = u1(); },
        "u2": sub () { T r = u2(); },
        "u3": sub () { T r = u3(); },
        "u4": sub () { T r = u4(); },
        "u5": sub () { T r = u5(); },
        "u6": sub () { T r = u6(); },
        "u7": sub () { T r = u7(); },
    };
    foreach hash<auto> i in (fns.pairIterator()) {
        log = ();
        i.value();
        printf("%s: %y\n", i.key, log);
    }
}

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
    list<string> fns = ("a", "b", "c", "d", "e", "r1", "r2", "r3", "r4", "r5", "r6", "f1");
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
run_returns();
run_thread("t2");
run_thread("t1");
