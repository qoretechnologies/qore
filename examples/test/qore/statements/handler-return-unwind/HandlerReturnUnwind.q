# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A handler that returns a value while a thread_exit or an exception leaves its block (#6073, #6074); see
# handler-return-unwind.qtest

%modern
%enable-all-warnings
our list<string> lg = ();
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { lg += "~" + n; } }
# a value whose member is returned
class TB { public { T m; string n; } constructor(string n) { self.n = n; m = new T(n + "-m"); } destructor() { lg += "~" + n; } }
# a value whose destructor throws
class TT { public { string n; } constructor(string n) { self.n = n; } destructor() { lg += "~" + n; throw "DTOR-" + n; } }
our bool never = False;

# a later handler ends in a thread_exit after a handler's return replaced the value (#6073)
auto sub te1() {
    on_exit { lg += "x1"; thread_exit; }
    on_exit { lg += "x2"; return new T("te1-h"); }
    return new T("te1-body");
}
# a handler returns after a later handler's thread_exit: its value replaces the interrupted return (#6074)
auto sub te2() {
    on_exit { lg += "x"; return new T("te2-h"); }
    on_success { lg += "s"; thread_exit; }
    return new T("te2-body");
}
auto sub te3() {
    on_exit { lg += "x"; return new T("te3-h"); }
    on_exit { lg += "te"; thread_exit; }
    return new T("te3-body");
}
# with a local: the replaced value is released before the local, the handler's value after it
auto sub te4() {
    T l("te4-l");
    on_exit { lg += "x"; return new T("te4-h"); }
    on_exit { lg += "te"; thread_exit; }
    return new T("te4-body");
}
auto sub te5() {
    T l("te5-l");
    on_exit { lg += "x"; return new T("te5-h"); }
    on_success { lg += "s"; thread_exit; }
    return new T("te5-body");
}
# a thread_exit in the body: the handler's value is kept until the function ends
auto sub te6() {
    T l("te6-l");
    on_exit { lg += "x"; return new T("te6-h"); }
    thread_exit;
}
# handlers of an inner block and of the function's block
auto sub te7() {
    {
        on_exit { lg += "xi"; return new T("te7-h"); }
        on_exit { lg += "te"; thread_exit; }
        return new T("te7-body");
    }
}
auto sub te8() {
    on_exit { lg += "xo"; return new T("te8-h"); }
    {
        on_exit { lg += "te"; thread_exit; }
        return new T("te8-body");
    }
}
# three handlers, the last one to run ends in a thread_exit
auto sub te9() {
    on_exit { lg += "x1"; thread_exit; }
    on_exit { lg += "x2"; return new T("te9-h2"); }
    on_exit { lg += "x3"; return new T("te9-h3"); }
    return new T("te9-body");
}
# a handler returns while an exception is pending, with a local (#6074)
auto sub ex1() {
    T l("ex1-l");
    on_exit { lg += "x"; return new T("ex1-h"); }
    on_exit { lg += "thr"; throw "EX1"; }
    return new T("ex1-body");
}
auto sub ex2() {
    T a("ex2-a");
    on_exit { lg += "x"; return new T("ex2-h"); }
    throw "EX2";
}
auto sub ex3() {
    T a("ex3-a");
    on_exit { lg += "x"; return new T("ex3-h"); }
    on_exit { lg += "te"; thread_exit; }
    throw "EX3";
}

# the cases of the review of #6074
# a handler's return replaces the value when it executes, before the block's remaining handlers run
auto sub k1() {
    T l("k1-l");
    on_exit { lg += "y"; }
    on_exit { lg += "x"; return new T("k1-h"); }
    on_exit { lg += "te"; thread_exit; }
    return new T("k1-body");
}
auto sub k2() {
    T l("k2-l");
    on_exit { lg += "y"; }
    on_exit { lg += "x"; return new T("k2-h"); }
    on_exit { lg += "thr"; throw "K2"; }
    return new T("k2-body");
}
# a handler's return replaces the value when it executes, before the handler's own handlers run
auto sub k3() {
    T l("k3-l");
    on_exit {
        on_exit { lg += "in"; return new T("k3-hin"); }
        lg += "out";
        return new T("k3-hout");
    }
    on_exit { lg += "te"; thread_exit; }
    return new T("k3-body");
}
auto sub k4() {
    T l("k4-l");
    on_exit {
        on_exit { lg += "in"; return new T("k4-hin"); }
        lg += "out";
        return new T("k4-hout");
    }
    on_exit { lg += "thr"; throw "K4"; }
    return new T("k4-body");
}
# the return of a handler of a handler releases the function's interrupted return
auto sub k5() {
    T l("k5-l");
    on_exit {
        T q("k5-q");
        on_exit { lg += "in"; return new T("k5-hin"); }
        lg += "out";
        thread_exit;
    }
    return new T("k5-body");
}
auto sub k6() {
    T l("k6-l");
    on_exit {
        T q("k6-q");
        on_exit { lg += "in"; return new T("k6-hin"); }
        lg += "out";
        throw "K6";
    }
    return new T("k6-body");
}
# with a try statement of the function catching the exception, the handler's return replaces the value of the try
# block's return when it executes
auto sub k7() {
    T l("k7-l");
    try {
        T m("k7-m");
        on_exit { lg += "x"; return new T("k7-h"); }
        on_exit { throw "K7"; }
        return new T("k7-bodyin");
    } catch (hash<ExceptionInfo> ex) {
        lg += "caught";
    }
    return new T("k7-body");
}


# a return without a value in a handler replaces the value with no value (#6074)
auto sub rn1() {
    T l("rn1-l");
    on_exit { lg += "x"; return; }
    on_exit { lg += "te"; thread_exit; }
    return new T("rn1-body");
}
auto sub rn2() {
    T l("rn2-l");
    on_exit { lg += "x"; return; }
    on_exit { lg += "thr"; throw "RN2"; }
    return new T("rn2-body");
}
auto sub rn3() {
    T l("rn3-l");
    on_exit { lg += "x"; return; }
    on_exit { lg += "y"; return new T("rn3-h"); }
    on_exit { lg += "thr"; throw "RN3"; }
    return new T("rn3-body");
}
# a return in a try statement of a handler replaces the value when the try statement ends (#6074)
auto sub tr1() {
    T l("tr1-l");
    on_exit {
        try {
            T t("tr1-t");
            lg += "x";
            return new T("tr1-h");
        } catch (hash<ExceptionInfo> ex) {
        }
    }
    on_exit { lg += "te"; thread_exit; }
    return new T("tr1-body");
}
auto sub tr2() {
    T l("tr2-l");
    on_exit {
        try {
            lg += "x";
            return new T("tr2-h");
        } catch (hash<ExceptionInfo> ex) {
        }
    }
    on_exit { lg += "thr"; throw "TR2"; }
    return new T("tr2-body");
}


# the cases of the second review of #6074
# a return in a try statement of a handler lowered inline replaces a value whose destructor throws: the exception is
# raised after the try statement, not caught by its catch block
auto sub rv1() {
    on_exit { try { return new T("rv1-h"); } catch (hash<ExceptionInfo> ex) { lg += "c:" + ex.err; } }
    return new TT("rv1-body");
}
# the same in a handler run out of line: the hand-over at the end of the try statement raises past its catch block
auto sub rv2() {
    T l("rv2-l");
    on_exit { lg += "y"; }
    on_exit { lg += "x"; try { return new T("rv2-h"); } catch (hash<ExceptionInfo> ex) { lg += "c:" + ex.err; }
        lg += "after"; }
    on_exit { lg += "thr"; throw "RV2"; }
    return new TT("rv2-body");
}
# a handler of a handler run out of line: its return replaces the function's interrupted return
auto sub rv3() {
    T l("rv3-l");
    on_exit { on_exit { lg += "2"; return new T("rv3-2"); } lg += "1"; thread_exit; }
    on_exit { lg += "te"; thread_exit; }
    return new T("rv3-body");
}
# a return in a try statement of a handler whose cleanups lower a handler with a return that is not reached
auto sub rv4() {
    T l("rv4-l");
    on_exit {
        try {
            on_exit { lg += "in"; if (never) return new T("rv4-in"); }
            lg += "out";
            return new T("rv4-out");
        } catch (hash<ExceptionInfo> ex) { lg += "c"; }
    }
    on_exit { lg += "te"; thread_exit; }
    return new T("rv4-body");
}
# a handler of a handler whose exception a try statement of the handler catches: the return goes to the try statement
auto sub rv5() {
    on_exit {
        try {
            { on_error { lg += "e"; return new T("rv5-h"); } throw "RV5"; }
        } catch (hash<ExceptionInfo> ex) { lg += "c"; }
    }
    T b("rv5-b");
    throw "RV5b";
}


# the same handler reached from sites at different try depths of the function: a normal return (the handler lowered
# inline), an exception that a try statement of the function catches (the value replaces the value of its try
# block's return) and a thread_exit that leaves the function (the value replaces the function's return); the handler
# function is the same for every site, which gives what is specific to the site at run time
auto sub sh(int mode) {
    T l("sh-l");
    try {
        on_exit { lg += "x"; return new T("sh-h" + mode); }
        if (mode == 1) {
            throw "SH";
        }
        if (mode == 2) {
            thread_exit;
        }
        return new T("sh-body" + mode);
    } catch (hash<ExceptionInfo> ex) {
        lg += "c";
    }
    return new T("sh-after" + mode);
}


# with a try statement of the function catching the exception, a handler's return replaces the value of the try
# block's return, which the return holds a reference of its own to: it is released when the handler's return executes
auto sub tf1() {
    T l("tf1-l");
    try {
        on_exit { lg += "x"; return new T("tf1-h"); }
        on_exit { lg += "thr"; throw "TF1"; }
        return (new TB("tf1-base")).m;
    } catch (hash<ExceptionInfo> ex) {
        lg += "c";
    }
    return new T("tf1-body");
}

# runs the function twice in a thread of its own, which a thread_exit ends
sub show(string label, code c) {
    for (int i = 0; i < 2; ++i) {
        lg = ();
        Counter done = new Counter(1);
        background sub () {
            on_exit done.dec();
            try {
                auto v = c();
                lg += "rv:" + (v instanceof T ? v.n : (exists v ? sprintf("%s", v) : "nothing"));
            } catch (hash<ExceptionInfo> ex) {
                lg += "ex:" + ex.err;
            }
        }();
        done.waitForZero();
        printf("%s[%d]: %y\n", label, i, lg);
    }
}
show("te1", \te1());
show("te2", \te2());
show("te3", \te3());
show("te4", \te4());
show("te5", \te5());
show("te6", \te6());
show("te7", \te7());
show("te8", \te8());
show("te9", \te9());
show("ex1", \ex1());
show("ex2", \ex2());
show("ex3", \ex3());
show("k1", \k1());
show("k2", \k2());
show("k3", \k3());
show("k4", \k4());
show("k5", \k5());
show("k6", \k6());
show("k7", \k7());
show("rn1", \rn1());
show("rn2", \rn2());
show("rn3", \rn3());
show("tr1", \tr1());
show("tr2", \tr2());
show("rv1", \rv1());
show("rv2", \rv2());
show("rv3", \rv3());
show("rv4", \rv4());
show("rv5", \rv5());
show("sh0", auto sub () { return sh(0); });
show("sh1", auto sub () { return sh(1); });
show("sh2", auto sub () { return sh(2); });
show("tf1", \tf1());
