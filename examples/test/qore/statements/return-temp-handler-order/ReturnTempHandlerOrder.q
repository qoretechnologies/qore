# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The values that the expression of a return statement reads are released before the handlers of the blocks that it
# leaves run (#6003); see return-temp-handler-order.qtest

%modern
%enable-all-warnings

our list<string> log = ();

class T {
    public {
        string n;
        *T c;
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

our *T g;

# on_success replaces the local that the return statement reads, and the value that the function assigned it last;
# the return runs no on_error handler
string sub s1(int m) {
    T a = mk("s1-a" + m);
    on_exit {
        log += "x:" + a.n;
    }
    on_success {
        log += "s:" + a.n;
        a = mk("s1-as");
        log += "s-end";
    }
    on_error {
        a = mk("s1-ae");
    }
    if (m == 1) {
        return a.n;
    }
    a = mk("s1-b" + m);
    return a.n;
}

# on_exit replaces a global
string sub s2() {
    g = mk("s2-g");
    on_exit {
        g = mk("s2-g2");
        log += "x";
    }
    return g.n;
}

# on_exit replaces a member
string sub s3() {
    T a = mk("s3-a");
    a.c = mk("s3-c");
    on_exit {
        a.c = mk("s3-c2");
        log += "x";
    }
    return a.c.n;
}

# a return in a try block
string sub s4() {
    T a = mk("s4-a");
    try {
        on_exit {
            a = mk("s4-a2");
            log += "x";
        }
        return a.n;
    } catch () {
    }
}

# a return in an inner block
string sub s5() {
    T a = mk("s5-a");
    if (True) {
        on_success {
            a = mk("s5-a2");
            log += "s";
        }
        return a.n;
    }
}

# a value of a hash
string sub s6() {
    hash<auto> h = {"o": mk("s6-a")};
    on_exit {
        h.o = mk("s6-a2");
        log += "x";
    }
    return h.o.n;
}

# an element of a list
int sub s7() {
    list<auto> l = (mk("s7-a"),);
    on_exit {
        l = ();
        log += "x";
    }
    return l[0].n.size();
}

# only an on_error handler: the return runs none
string sub s8() {
    T a = mk("s8-a");
    on_error {
        a = mk("s8-a2");
    }
    return a.n;
}

# the functions are called directly, so that they run as native code in the jit and tiered modes
string sub call(string fn) {
    switch (fn) {
        case "s1-0": return s1(0);
        case "s1-1": return s1(1);
        case "s2": return s2();
        case "s3": return s3();
        case "s4": return s4();
        case "s5": return s5();
        case "s6": return s6();
        case "s7": return s7().toString();
        case "s8": return s8();
    }
    throw "UNKNOWN", fn;
}

foreach string fn in (("s1-0", "s1-1", "s2", "s3", "s4", "s5", "s6", "s7", "s8")) {
    log = ();
    try {
        log += "r:" + call(fn);
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    printf("%s: %y\n", fn, log);
}
# the global's value is released before the program's globals are
remove g;
