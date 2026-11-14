# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The locals that a handler declares are released at the end of their blocks when the handler raises an exception
# (#6002); see handler-local-release.qtest

%modern
%enable-all-warnings

our list<string> log = ();

class T {
    public {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        log += "~" + n;
    }
}

T sub mk(string n) {
    return new T(n);
}

# the local of an on_error handler that raises, before an outer handler
sub h1() {
    on_exit {
        log += "x";
    }
    on_error {
        T z = mk("h1-z");
        throw "H1";
    }
    throw "E1";
}

# the local of an inner block of the handler
sub h2() {
    on_exit {
        log += "x";
    }
    on_error {
        if (True) {
            T z = mk("h2-z");
            throw "H2";
        }
    }
    throw "E2";
}

# the local of an on_exit handler that raises, with a handler of its own
sub h3() {
    T a = mk("h3-a");
    on_exit {
        log += "x";
    }
    on_exit {
        T z = mk("h3-z");
        on_exit {
            log += "n";
        }
        throw "H3";
    }
}

# the local of an inner block of an on_exit handler lowered inline that declares none in its own block
sub h6() {
    on_exit {
        log += "x";
    }
    on_exit {
        if (True) {
            T z = mk("h6-z");
            throw "H6";
        }
    }
}

# the locals of a loop of an on_exit handler lowered inline, without a local in the handler's block
sub h7() {
    on_exit {
        log += "x";
    }
    on_exit {
        for (int i = 0; i < 2; ++i) {
            T z = mk("h7-z" + i);
            if (i == 1) {
                throw "H7";
            }
        }
    }
}

# a container local of a handler run out of line that the handler changes in place keeps its value
sub h8() {
    on_error {
        list<auto> l;
        l += 1;
        hash<auto> h;
        h.a = 1;
        log += sprintf("l: %y h: %y", l, h);
    }
    throw "E8";
}

# the shape of the reproducer of #6002: a nested handler of an on_error handler that raises; the release of the value
# that a handler run out of line replaces in native code is #6005
sub h5() {
    T a = mk("h5-a");
    on_exit {
        log += "h1:" + a.n;
    }
    on_error {
        T z = mk("h5-z");
        on_exit {
            log += "n:" + a.n;
        }
        log += "h2";
        throw "H5";
    }
    try {
        T b = mk("h5-b");
        throw "X";
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
        rethrow;
    }
}

# the functions are called directly, so that they run as native code in the jit and tiered modes, and each twice, so
# that the second call runs the code that the first one compiled in the tiered mode
sub call(string fn) {
    switch (fn) {
        case "h1": h1(); break;
        case "h2": h2(); break;
        case "h3": h3(); break;
        case "h5": h5(); break;
        case "h6": h6(); break;
        case "h7": h7(); break;
        case "h8": h8(); break;
        default: throw "UNKNOWN", fn;
    }
}

foreach string fn in (("h1", "h2", "h3", "h5", "h6", "h7", "h8")) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        try {
            call(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y\n", fn, log);
    }
}
