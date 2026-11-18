#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# The order in which the temporaries of a branch of ?:, ??, ?*, && and || and of a call that qcc folds are released
# (#6080); branch-block-temp-release.qtest runs it in every execution mode and compiled with qcc and compares its
# output with the AST interpreter's.  Each case runs twice, so that the tiered execution mode runs it as native code
# the second time.

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

    string name() {
        return n;
    }
}

T sub mk(string n) {
    return new T(n);
}

auto sub thr() {
    throw "THR";
}

auto sub thrif(bool c, auto v) {
    if (c) {
        throw "THR";
    }
    return v;
}

#! returns its first argument: qcc replaces a call with it
auto sub first2(auto a, auto b) {
    return a;
}

#! returns its second argument: qcc replaces a call with it
bool sub tb(auto a, bool r) {
    return r;
}

auto sub nul(auto a) {
}

sub out(string label) {
    printf("%s: %y live=%d\n", label, log, live);
    log = ();
}

# the temporaries of a branch are released when the statement ends, not when the next one does
sub ternary(bool b) {
    try {
        auto x = b ? first2(mk("a"), mk("p")) : first2(mk("b"), mk("q"));
        log += "after1";
        auto y = first2(mk("y"), mk("r"));
        log += "after2";
        remove x;
        remove y;
    } catch () {
        log += "c";
    }
}

sub null_coalescing(bool b) {
    try {
        auto x = (b ? nul(mk("a")) : 1) ?? first2(mk("b"), mk("q"));
        log += "after1";
        auto y = first2(mk("y"), mk("r"));
        log += "after2";
        remove x;
        remove y;
    } catch () {
        log += "c";
    }
}

sub value_coalescing(int k) {
    try {
        auto x = first2(k, mk("a")) ?* first2(mk("b"), thrif(k == 2, mk("q")));
        log += "after1";
        auto y = first2(mk("y"), mk("r"));
        log += "after2";
        remove x;
        remove y;
    } catch () {
        log += "c";
    }
}

sub logical_or(int k) {
    try {
        bool x = tb(mk("a"), k == 0) || tb(mk("b"), k == 1) || tb(mk("c"), thrif(k == 3, True));
        log += "after1:" + x;
        auto y = first2(mk("y"), mk("r"));
        log += "after2";
        remove y;
    } catch () {
        log += "c";
    }
}

sub logical_and(int k) {
    try {
        bool x = tb(mk("a"), k != 0) && tb(mk("b"), k != 1) && tb(mk("c"), thrif(k == 3, True));
        log += "after1:" + x;
        auto y = first2(mk("y"), mk("r"));
        log += "after2";
        remove y;
    } catch () {
        log += "c";
    }
}

sub nested(int k) {
    try {
        T z("z");
        auto x = k > 0
            ? first2(mk("a"), k > 1 ? first2(mk("b"), thrif(k == 3, mk("p"))) : first2(mk("c"), mk("s")))
            : first2(mk("d"), k == 0 ? mk("e") : thr());
        log += "after1";
        auto y = first2(mk("y"), mk("r"));
        log += "after2";
        remove x;
        remove y;
    } catch () {
        log += "c";
    }
}

# a receiver, which the statement holds until it ends, in a branch
sub receiver(bool b) {
    try {
        auto x = b ? mk(mk("a").name() + "o").name() : "";
        log += "after1";
        auto y = mk("y").name();
        log += "after2:" + x + y;
    } catch () {
        log += "c";
    }
}

# an assignment to a hash member, and a receiver in the right operand of &&
sub member_assignment(int k) {
    try {
        hash<auto> h;
        h.a = k ? mk(mk("a").name() + "o").name() : "x";
        log += "after1";
        h.b = k > 1 && mk(mk("b").name() + "o").name() == thrif(k == 3, "bo");
        log += "after2";
    } catch () {
        log += "c";
    }
}

auto sub ret(int k) {
    T z("z");
    return k ? first2(mk("a"), thrif(k == 2, mk("p"))) : first2(mk("b"), mk("q"));
}

sub return_value(int k) {
    try {
        auto v = ret(k);
        log += "got";
        remove v;
        log += "removed";
    } catch () {
        log += "c";
    }
}

sub loop(int lim) {
    try {
        for (int i = 0; i < 4; ++i) {
            T w("w" + i);
            auto x = (i % 2)
                ? first2(mk("a" + i), thrif(i == lim, mk("p" + i)))
                : first2(mk("b" + i), thrif(i == lim, mk("q" + i)));
            log += "s" + i;
            bool v = tb(mk("x" + i), i > 1) && tb(mk("y" + i), True) || tb(mk("z" + i), True);
            log += "t" + i + ":" + v;
            remove x;
        }
    } catch () {
        log += "c";
    }
}

# the arguments of a call that qcc folds are released when the call returns
sub folded_and() {
    try {
        T z("z");
        bool v = tb(mk("a"), True) && tb(mk("b"), thr());
        log += "s:" + v;
    } catch () {
        log += "c";
    }
}

sub folded_sum() {
    try {
        T z("z");
        auto v = first2(1, mk("a")) + first2(2, mk("b"));
        log += "s:" + v;
    } catch () {
        log += "c";
    }
}

sub folded_raise() {
    try {
        T z("z");
        auto v = first2(1, mk("a")) + tb(mk("b"), thr());
        log += "s:" + v;
    } catch () {
        log += "c";
    }
}

for (int n = 0; n < 2; ++n) {
    ternary(True); out("ternary-then");
    ternary(False); out("ternary-else");
    null_coalescing(True); out("null-coalescing-rhs");
    null_coalescing(False); out("null-coalescing-lhs");
    for (int k = 0; k < 3; ++k) { value_coalescing(k); out("value-coalescing-" + k); }
    for (int k = 0; k < 4; ++k) { logical_or(k); out("logical-or-" + k); }
    for (int k = 0; k < 4; ++k) { logical_and(k); out("logical-and-" + k); }
    for (int k = -1; k < 4; ++k) { nested(k); out("nested-" + k); }
    receiver(True); out("receiver");
    for (int k = 0; k < 4; ++k) { member_assignment(k); out("member-assignment-" + k); }
    for (int k = 0; k < 3; ++k) { return_value(k); out("return-" + k); }
    for (int l = 0; l < 5; ++l) { loop(l); out("loop-" + l); }
    folded_and(); out("folded-and");
    folded_sum(); out("folded-sum");
    folded_raise(); out("folded-raise");
}
