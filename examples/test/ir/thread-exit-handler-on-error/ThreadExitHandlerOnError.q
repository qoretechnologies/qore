# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The handlers of a block after a thread_exit raised by one of them (#5893); see thread-exit-handler-on-error.qtest
%modern
class Rec {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { printf("dtor %s\n", n); }
}
# t1: on_exit raises thread_exit, the block has on_error
sub t1() {
    try {
        if (True) {
            on_error { printf("t1 on_error\n"); }
            on_exit { printf("t1 on_exit\n"); thread_exit; }
            Rec t("t1");
        }
    } catch () {
        printf("t1 caught\n");
    }
    printf("t1 not reached\n");
}
# t2: on_exit raises thread_exit, the block has on_success: it runs, as a thread_exit is not an error
sub t2() {
    if (True) {
        on_success { printf("t2 on_success\n"); }
        on_exit { printf("t2 on_exit\n"); thread_exit; }
        Rec t("t2");
    }
    printf("t2 not reached\n");
}
# t3: the block raises an exception, then on_exit raises thread_exit: on_error runs for the exception
sub t3() {
    if (True) {
        on_error { printf("t3 on_error %s\n", $1.err); }
        on_exit { printf("t3 on_exit\n"); thread_exit; }
        Rec t("t3");
        throw "T3";
    }
}
# t4: on_success raises thread_exit, the block has on_error
sub t4() {
    if (True) {
        on_error { printf("t4 on_error\n"); }
        on_success { printf("t4 on_success\n"); thread_exit; }
        Rec t("t4");
    }
    printf("t4 not reached\n");
}
# t5: nested blocks: the inner block's on_exit raises thread_exit; neither block's on_error runs
sub t5() {
    if (True) {
        on_error { printf("t5 outer on_error\n"); }
        on_success { printf("t5 outer on_success\n"); }
        on_exit { printf("t5 outer on_exit\n"); }
        if (True) {
            on_error { printf("t5 inner on_error\n"); }
            on_exit { printf("t5 inner on_exit\n"); thread_exit; }
        }
        printf("t5 after inner\n");
    }
}
# t6: a thread_exit, then an exception from another handler: on_error runs for the exception
sub t6() {
    if (True) {
        on_error { printf("t6 on_error %s\n", $1.err); }
        on_success { printf("t6 on_success\n"); }
        on_exit { printf("t6 on_exit2\n"); throw "T6"; }
        on_exit { printf("t6 on_exit1\n"); thread_exit; }
    }
}
foreach code f in ((\t1(), \t2(), \t3(), \t4(), \t5(), \t6())) {
    Counter c(1);
    background sub () {
        on_exit c.dec();
        try {
            f();
        } catch (hash<ExceptionInfo> ex) {
            printf("thread caught %s\n", ex.err);
        }
    }();
    c.waitForZero();
}
printf("done\n");
