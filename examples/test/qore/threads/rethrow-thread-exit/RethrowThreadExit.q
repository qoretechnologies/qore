# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A rethrow in an on_error handler keeps a thread exit that another handler raised (#5904); see
# rethrow-thread-exit.qtest

%modern

# the issue's case: the on_exit handler, which runs first, raises the thread exit, and the on_error handler rethrows
sub rethrow_after_exit() {
    try {
        if (True) {
            on_error { printf("on_error %s\n", $1.err); rethrow; }
            on_exit { printf("on_exit\n"); thread_exit; }
            throw "ERR";
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# the same without the rethrow: the reference for the case above
sub no_rethrow() {
    try {
        if (True) {
            on_error { printf("on_error %s\n", $1.err); }
            on_exit { printf("on_exit\n"); thread_exit; }
            throw "ERR";
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# the on_error handler runs first and rethrows, then the on_exit handler raises the thread exit
sub exit_after_rethrow() {
    try {
        if (True) {
            on_exit { printf("on_exit\n"); thread_exit; }
            on_error { printf("on_error %s\n", $1.err); rethrow; }
            throw "ERR";
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# the thread exit is raised in an inner block, and an on_error handler of an outer block rethrows the exception
sub outer_rethrow() {
    try {
        if (True) {
            on_error { printf("outer on_error %s\n", $1.err); rethrow; }
            if (True) {
                on_exit { printf("inner on_exit\n"); thread_exit; }
                throw "ERR";
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# a rethrow with arguments
sub rethrow_args() {
    try {
        if (True) {
            on_error { printf("on_error %s\n", $1.err); rethrow "NEW", "desc"; }
            on_exit { printf("on_exit\n"); thread_exit; }
            throw "ERR";
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# a function called in a try statement: the caller does not catch the exception either, as the thread exits
sub caller() {
    try {
        rethrow_after_exit();
    } catch (hash<ExceptionInfo> ex) {
        printf("caller caught %s\n", ex.err);
    }
    printf("caller continued\n");
}

foreach string fn in (("rethrow_after_exit", "no_rethrow", "exit_after_rethrow", "outer_rethrow", "rethrow_args",
        "caller")) {
    printf("-- %s\n", fn);
    Counter c(1);
    background sub () {
        on_exit c.dec();
        call_function(fn);
        printf("thread continued\n");
    }();
    c.waitForZero();
}
