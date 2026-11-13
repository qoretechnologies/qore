# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The value of a return that a handler interrupts is released after the locals when the code after the handler cannot
# be reached (#6028); see return-after-unreachable-exit.qtest

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

# a throw in a nested block of an on_exit handler that runs at a return
auto sub a1() {
    T z("a1-z");
    if (True) {
        on_exit {
            log += "x";
            if (True) {
                throw "A1";
            }
        }
        return mk("a1-t");
    }
}

# the same in an on_success handler, with a local of the function returned
auto sub a2() {
    T z("a2-z");
    T t("a2-t");
    if (True) {
        on_success {
            log += "s";
            if (True) {
                throw "A2";
            }
        }
        return t;
    }
}

# the return in a loop
auto sub a3() {
    int n = 0;
    T z("a3-z");
    while (n < 5) {
        ++n;
        if (n == 3) {
            on_exit {
                log += "x";
                if (True) {
                    throw "A3";
                }
            }
            return mk("a3-t");
        }
    }
    return n;
}

# a value that a conditional gives
auto sub a5(bool b) {
    T z("a5-z");
    on_exit {
        log += "x";
        if (True) {
            throw "A5";
        }
    }
    return b ? mk("a5-t") : 3;
}

# a thread_exit in a nested block of the handler
auto sub a4() {
    T z("a4-z");
    if (True) {
        on_exit {
            log += "x";
            if (True) {
                thread_exit;
            }
        }
        return mk("a4-t");
    }
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each three times
sub call(string fn) {
    switch (fn) {
        case "a1": a1(); break;
        case "a2": a2(); break;
        case "a3": a3(); break;
        case "a5": a5(True); break;
    }
}

foreach string fn in (("a1", "a2", "a3", "a5")) {
    for (int i = 0; i < 3; ++i) {
        log = ();
        try {
            call(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y live=%d\n", fn, log, live);
    }
}
for (int i = 0; i < 3; ++i) {
    log = ();
    Counter c(1);
    background sub () {
        on_exit c.dec();
        a4();
    }();
    c.waitForZero();
    printf("a4: %y live=%d\n", log, live);
}
