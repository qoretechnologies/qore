# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A handler that returns a value while a thread_exit or an exception leaves its block (#6073, #6074); see
# handler-return-unwind.qtest

%modern
%enable-all-warnings
our list<string> lg = ();
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { lg += "~" + n; } }

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
        printf("%s: %y\n", label, lg);
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
