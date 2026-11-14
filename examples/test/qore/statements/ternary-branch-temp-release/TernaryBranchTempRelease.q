# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A temporary of a call in a branch of ?: (and of the other operators whose join block holds the end of the statement)
# that an exception interrupts is released before the locals of the block (#5997); see
# ternary-branch-temp-release.qtest

%modern
%enable-all-warnings

our list<string> log = ();
our int live = 0;

class T {
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
        ++live;
    }
    destructor() {
        log += "~" + n;
        --live;
    }
}

T sub mk(string n) {
    return new T(n);
}

auto sub thr() {
    throw "THR";
}

auto sub first2(auto a, auto b) {
    return a;
}

auto sub nothing() {
}

# the call without ?:
T sub plain() {
    try {
        T z("z");
        return first2(mk("a"), thr());
    } catch () {
        log += "c";
    }
    return new T("o");
}

# the issue's case: the call in a branch of ?: in a return
T sub ternary(bool b) {
    try {
        T z("z");
        return b ? first2(mk("a"), thr()) : mk("x");
    } catch () {
        log += "c";
    }
    return new T("o");
}

# the call in the other branch
T sub ternary_else(bool b) {
    try {
        T z("z");
        return b ? mk("x") : first2(mk("a"), thr());
    } catch () {
        log += "c";
    }
    return new T("o");
}

# a temporary in both branches
T sub both(bool b) {
    try {
        T z("z");
        return b ? first2(mk("a"), thr()) : first2(mk("b"), thr());
    } catch () {
        log += "c";
    }
    return new T("o");
}

# ?: in an argument list
T sub arg_list(bool b) {
    try {
        T z("z");
        return first2(b ? first2(mk("a"), thr()) : mk("x"), mk("y"));
    } catch () {
        log += "c";
    }
    return new T("o");
}

# nested ?:
T sub nested(bool b) {
    try {
        T z("z");
        return b ? first2(mk("w"), b ? first2(mk("a"), thr()) : mk("y")) : mk("x");
    } catch () {
        log += "c";
    }
    return new T("o");
}

# nested calls in a branch
T sub nested_call(bool b) {
    try {
        T z("z");
        return b ? first2(mk("a"), first2(mk("b"), thr())) : mk("x");
    } catch () {
        log += "c";
    }
    return new T("o");
}

# ??
T sub null_coalescing() {
    try {
        T z("z");
        return nothing() ?? first2(mk("a"), thr());
    } catch () {
        log += "c";
    }
    return new T("o");
}

# ?*
T sub value_coalescing() {
    try {
        T z("z");
        return nothing() ?* first2(mk("a"), thr());
    } catch () {
        log += "c";
    }
    return new T("o");
}

# ?: in an assignment
T sub assignment(bool b) {
    try {
        T z("z");
        T r = b ? first2(mk("a"), thr()) : mk("x");
        log += "s";
    } catch () {
        log += "c";
    }
    return new T("o");
}

# ?: in an argument of a call that is a statement
T sub call_arg(bool b) {
    try {
        T z("z");
        first2(b ? first2(mk("a"), thr()) : mk("x"), 1);
        log += "s";
    } catch () {
        log += "c";
    }
    return new T("o");
}

# &&
T sub logical_and(bool b) {
    try {
        T z("z");
        bool r = b && first2(mk("a"), thr());
        log += "s" + r;
    } catch () {
        log += "c";
    }
    return new T("o");
}

# in a loop body
sub loop(bool b) {
    for (int i = 0; i < 2; ++i) {
        try {
            T z("z" + i);
            T r = b ? first2(mk("a" + i), thr()) : mk("x");
        } catch () {
            log += "c" + i;
        }
    }
}

# the exception caught by the caller
T sub caller_return(bool b) {
    T z("z");
    return b ? first2(mk("a"), thr()) : mk("x");
}

T sub caller_assign(bool b) {
    T z("z");
    T r = b ? first2(mk("a"), thr()) : mk("x");
    return r;
}

sub caller(code c) {
    try {
        T r = c();
    } catch () {
        log += "c";
    }
}

sub run(string name, code c) {
    log = ();
    {
        auto r = c();
    }
    printf("%s: %y live=%d\n", name, log, live);
}

run("plain", \plain());
run("ternary", auto sub () { return ternary(True); });
run("ternary-else", auto sub () { return ternary_else(False); });
run("both-1", auto sub () { return both(True); });
run("both-2", auto sub () { return both(False); });
run("arg-list", auto sub () { return arg_list(True); });
run("nested", auto sub () { return nested(True); });
run("nested-call", auto sub () { return nested_call(True); });
run("null-coalescing", \null_coalescing());
run("value-coalescing", \value_coalescing());
run("assignment", auto sub () { return assignment(True); });
run("call-arg", auto sub () { return call_arg(True); });
run("logical-and", auto sub () { return logical_and(True); });
run("loop", sub () { loop(True); });
run("caller-return", sub () { caller(auto sub () { return caller_return(True); }); });
run("caller-assign", sub () { caller(auto sub () { return caller_assign(True); }); });
