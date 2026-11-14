# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A try statement that a thread_exit leaves discards an exception that a handler raises while the thread exits
# (#5912); see thread-exit-handler-exception.qtest

%modern

# the issue's case: the on_exit handler of a block in the try block raises an exception
sub block_handler() {
    try {
        if (True) {
            on_exit { printf("on_exit\n"); throw "ONEXIT"; }
            thread_exit;
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# the handler of the try block itself
sub try_block_handler() {
    try {
        on_exit { printf("on_exit\n"); throw "ONEXIT"; }
        thread_exit;
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# an on_error and an on_success handler beside the on_exit handler
sub error_success_handlers() {
    try {
        if (True) {
            on_error printf("on_error %y\n", $1.err);
            on_success printf("on_success\n");
            on_exit { printf("on_exit\n"); throw "ONEXIT"; }
            thread_exit;
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# two handlers that raise exceptions; the second one runs while the first exception is pending
sub two_handlers() {
    try {
        if (True) {
            on_exit { printf("on_exit 1\n"); throw "ONEXIT1"; }
            on_exit { printf("on_exit 2\n"); throw "ONEXIT2"; }
            thread_exit;
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# nested try statements: the inner one discards the exception; the handler of the outer try block raises another one,
# which the outer try statement discards
sub nested_try() {
    try {
        on_exit { printf("outer on_exit\n"); throw "OUTER"; }
        try {
            if (True) {
                on_exit { printf("inner on_exit\n"); throw "INNER"; }
                thread_exit;
            }
        } catch (hash<ExceptionInfo> ex) {
            printf("inner caught %s\n", ex.err);
        }
        printf("inner continued\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("outer caught %s\n", ex.err);
    }
    printf("continued\n");
}

# a thread_exit in a catch block of a try statement in a try block
sub in_catch_block() {
    try {
        try {
            throw "FIRST";
        } catch (hash<ExceptionInfo> ex) {
            on_exit { printf("on_exit in catch %s\n", ex.err); throw "ONEXIT"; }
            thread_exit;
        }
        printf("inner continued\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# a handler of a catch block that a thread_exit leaves runs with the catch block's exception: a rethrow in it rethrows
# that exception, also in a catch block of an outer try statement
sub rethrow_in_catch_handler() {
    try {
        try {
            throw "A";
        } catch (hash<ExceptionInfo> ex) {
            on_exit {
                printf("on_exit %s\n", ex.err);
                try {
                    rethrow;
                } catch (hash<ExceptionInfo> e2) {
                    printf("rethrown %s\n", e2.err);
                }
                throw "H";
            }
            thread_exit;
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}
sub rethrow_in_nested_catch_handler() {
    try {
        throw "OUTER";
    } catch (hash<ExceptionInfo> o) {
        try {
            try {
                throw "A";
            } catch (hash<ExceptionInfo> ex) {
                on_exit {
                    try {
                        rethrow;
                    } catch (hash<ExceptionInfo> e2) {
                        printf("rethrown %s\n", e2.err);
                    }
                }
                thread_exit;
            }
        } catch () {
            printf("caught\n");
        }
        printf("outer catch continued %s\n", o.err);
    }
    printf("continued\n");
}

# a thread_exit in a loop in a try block, with a handler in the loop body
sub in_loop() {
    try {
        for (int i = 0; i < 3; ++i) {
            on_exit {
                printf("on_exit %d\n", i);
                if (i == 1) {
                    throw "ONEXIT";
                }
            }
            if (i == 1) {
                thread_exit;
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# a thread_exit in a try block without handlers: the thread exits
sub no_handler() {
    try {
        thread_exit;
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("continued\n");
}

# a thread_exit outside any try statement whose handler raises an exception, in a function called in a try block
sub callee() {
    on_exit { printf("callee on_exit\n"); throw "CALLEE"; }
    thread_exit;
}
sub caller_try() {
    try {
        callee();
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s\n", ex.err);
    }
    printf("caller continued\n");
}

foreach string fn in (("block_handler", "try_block_handler", "error_success_handlers", "two_handlers", "nested_try",
        "in_catch_block", "rethrow_in_catch_handler", "rethrow_in_nested_catch_handler", "in_loop", "no_handler",
        "caller_try")) {
    printf("-- %s\n", fn);
    Counter c(1);
    background sub () {
        on_exit c.dec();
        call_function(fn);
        printf("thread continued\n");
    }();
    c.waitForZero();
}
