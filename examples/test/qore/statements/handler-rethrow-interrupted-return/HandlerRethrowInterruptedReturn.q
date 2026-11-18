# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The value of a return that an exception raised in the catch block of a try statement in a handler interrupts is
# released as the AST interpreter releases it (#6033); see handler-rethrow-interrupted-return.qtest

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

# the issue: an on_exit handler whose try statement's catch block rethrows, a new object returned, the exception
# caught by a try statement of the function
auto sub r1() {
    T z("r1-z");
    try {
        if (True) {
            on_exit { log += "ox"; }
            if (True) {
                on_exit {
                    try {
                        throw "R1";
                    } catch (hash<ExceptionInfo> ex) {
                        log += "c";
                        rethrow;
                    }
                }
                return new T("r1-rv");
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 3;
}

# a new throw in the handler's catch block, with an on_error handler in the same block
auto sub r2() {
    T z("r2-z");
    try {
        if (True) {
            on_error { log += "oe"; }
            on_exit {
                try {
                    throw "X";
                } catch (hash<ExceptionInfo> ex) {
                    log += "c";
                    throw "R2";
                }
            }
            return new T("r2-rv");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 3;
}

# a local of the try block returned
auto sub r3() {
    T z("r3-z");
    try {
        T t("r3-t");
        if (True) {
            on_exit {
                try {
                    throw "R3";
                } catch (hash<ExceptionInfo> ex) {
                    log += "c";
                    rethrow;
                }
            }
            return t;
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 3;
}

# the value of a call returned
auto sub r4() {
    T z("r4-z");
    try {
        if (True) {
            on_exit {
                try {
                    throw "R4";
                } catch (hash<ExceptionInfo> ex) {
                    log += "c";
                    rethrow;
                }
            }
            return mk("r4-rv");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 3;
}

# nested try statements: the inner one catches the exception and rethrows it to the outer one
auto sub r5() {
    T z("r5-z");
    try {
        try {
            if (True) {
                on_exit {
                    try {
                        throw "R5";
                    } catch (hash<ExceptionInfo> ex) {
                        log += "c";
                        rethrow;
                    }
                }
                return mk("r5-rv");
            }
        } catch (hash<ExceptionInfo> ex) {
            log += "inner:" + ex.err;
            rethrow;
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "outer:" + ex.err;
    }
    log += "after";
    return 3;
}

# nested try statements: the inner one catches the exception
auto sub r6() {
    T z("r6-z");
    try {
        try {
            if (True) {
                on_exit {
                    try {
                        throw "R6";
                    } catch (hash<ExceptionInfo> ex) {
                        log += "c";
                        rethrow;
                    }
                }
                return mk("r6-rv");
            }
        } catch (hash<ExceptionInfo> ex) {
            log += "inner:" + ex.err;
        }
        log += "mid";
    } catch (hash<ExceptionInfo> ex) {
        log += "outer:" + ex.err;
    }
    log += "after";
    return 3;
}

# a handler in the function's top block, the exception caught by the caller
auto sub r7() {
    T z("r7-z");
    on_exit {
        try {
            throw "R7";
        } catch (hash<ExceptionInfo> ex) {
            log += "c";
            rethrow;
        }
    }
    return mk("r7-rv");
}

# an on_success handler
auto sub r8() {
    T z("r8-z");
    try {
        if (True) {
            on_success {
                try {
                    throw "R8";
                } catch (hash<ExceptionInfo> ex) {
                    log += "c";
                    rethrow;
                }
            }
            return mk("r8-rv");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 3;
}

# a thread_exit in the handler's catch block in place of the rethrow
auto sub r9() {
    T z("r9-z");
    try {
        if (True) {
            on_exit {
                try {
                    throw "X";
                } catch (hash<ExceptionInfo> ex) {
                    log += "c";
                    thread_exit;
                }
            }
            return mk("r9-rv");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 3;
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each twice
sub call(string fn) {
    switch (fn) {
        case "r1": r1(); break;
        case "r2": r2(); break;
        case "r3": r3(); break;
        case "r4": r4(); break;
        case "r5": r5(); break;
        case "r6": r6(); break;
        case "r7": r7(); break;
        case "r8": r8(); break;
    }
}

foreach string fn in (("r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        try {
            call(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caller:" + ex.err;
        }
        printf("%s: %y live=%d\n", fn, log, live);
    }
}
for (int i = 0; i < 2; ++i) {
    log = ();
    Counter c(1);
    background sub () {
        on_exit c.dec();
        r9();
    }();
    c.waitForZero();
    printf("r9: %y live=%d\n", log, live);
}
