# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The value of a return that a handler's exception interrupts is kept until after the locals when the exception
# branches to a handler of the same function (#6031); see handler-return-in-frame-exception.qtest

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

string sub tag(string s) {
    return s;
}

# nested on_exit handlers that raise at a return, with string locals in the handlers
auto sub j1() {
    T z("j1-z");
    if (True) {
        on_exit {
            string s = tag("ox");
            log += s;
            throw "J1-OUTER";
        }
        T a("j1-a");
        if (True) {
            on_exit {
                string s = tag("ix");
                log += s;
                throw "J1-INNER";
            }
            T t("j1-t");
            return t;
        }
    }
}

# the handler's exception caught by a try statement of the function
auto sub j2() {
    T z("j2-z");
    try {
        T a("j2-a");
        if (True) {
            on_exit {
                log += "ix";
                throw "J2";
            }
            return mk("j2-t");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    log += "after";
    return 1;
}

# an inner block with a local between the handler and the return
auto sub j3() {
    T z("j3-z");
    if (True) {
        on_exit {
            log += "ox";
        }
        if (True) {
            T b("j3-b");
            if (True) {
                on_exit {
                    log += "ix";
                    throw "J3";
                }
                return mk("j3-t");
            }
        }
    }
}

# a thread_exit that interrupts a return in a try statement: the try statement releases the value
auto sub d1() {
    T z("d1-z");
    try {
        T a("d1-a");
        if (True) {
            on_exit {
                log += "ix";
                thread_exit;
            }
            return mk("d1-t");
        }
    } catch () {
        log += "catch";
    }
    log += "after";
}

# an exception of the inner handler, then a thread_exit of the outer handler in the try statement
auto sub d3() {
    T z("d3-z");
    try {
        if (True) {
            on_exit {
                log += "ox";
                thread_exit;
            }
            if (True) {
                on_exit {
                    log += "ix";
                    throw "D3";
                }
                return mk("d3-t");
            }
        }
    } catch () {
        log += "catch";
    }
}

# each function runs twice, through call_function()
foreach string fn in (("j1", "j2", "j3")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        try {
            call_function(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y live=%d\n", fn, log, live);
    }
}

# the thread_exit cases in threads of their own
foreach string fn in (("d1", "d3")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        Counter c(1);
        background sub () {
            on_exit c.dec();
            call_function(fn);
        }();
        c.waitForZero();
        printf("%s: %y live=%d\n", fn, log, live);
    }
}
