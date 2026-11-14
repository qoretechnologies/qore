# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A return in a handler replaces the value of the return that the handler runs after (#5992); see
# handler-return-replaces-value.qtest

%modern
%enable-all-warnings
our list<string> log = ();
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
# the return of a handler that runs after one that returns gives the value
string sub f() {
    on_exit { return "last"; }
    on_success { return "success"; }
    return "body";
}
# the replaced value is released when the handler's return replaces it
T sub g() {
    T a("a");
    on_exit { log += "exit"; }
    on_success { return a; }
    return new T("unused");
}
# a return without a value replaces it with no value
auto sub q2() {
    T z("rz");
    try {
        if (True) {
            on_exit { log += "rox"; }
            if (True) {
                on_exit { log += "rix"; return; }
                return new T("rrv");
            }
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "catch";
    }
    log += "never";
}
# three handlers that return
auto sub th() {
    on_exit { log += "h1"; return new T("th1"); }
    on_exit { log += "h2"; return new T("th2"); }
    on_exit { log += "h3"; return new T("th3"); }
    return new T("th-body");
}
# a return without a value in a handler, then a return of a value
auto sub nv() {
    on_exit { return new T("nv-last"); }
    on_success { return; }
    return new T("nv-body");
}
# handlers of nested blocks that return
auto sub n1() {
    on_exit { log += "o"; return new T("n1-outer"); }
    {
        on_exit { log += "i"; return new T("n1-inner"); }
        return new T("n1-body");
    }
}
auto sub n2() {
    on_exit { log += "o"; return new T("n2-outer"); }
    {
        on_exit { log += "i"; }
        {
            on_success { log += "ii"; return new T("n2-inner"); }
            return new T("n2-body");
        }
    }
}
# a block that ends without a return
auto sub ft() {
    on_exit { return "ft-last"; }
    on_success { return "ft-success"; }
    log += "body";
}
# a block with on_exit and on_error handlers that ends without a return
auto sub mx() {
    on_exit { return "mx-last"; }
    on_error { log += "err"; }
    on_exit { return new T("mx-x"); }
    log += "body";
}
# a try statement in the function catches an exception that a handler raises after one that returns
auto sub tc() {
    try {
        on_exit { log += "tcx"; throw "TC"; }
        on_success { return new T("tc-s"); }
        return new T("tc-body");
    } catch (hash<ExceptionInfo> ex) {
        log += "catch";
    }
    return "tc-after";
}
# a return in a handler of a block in the body of another handler
auto sub hh1() { on_exit { if (True) { on_exit { return 1; } } } return 2; }
auto sub hh2() { on_exit { if (True) { on_exit { return 1; } } } }
auto sub hh3() { on_exit { if (True) { on_exit { if (False) return 1; } } } return 2; }
auto sub hh4() { on_exit { while (True) { on_exit { return 2; } break; } } return 3; }
auto sub hh5() {
    on_exit { log += "o1"; }
    on_exit {
        log += "h";
        if (True) {
            on_exit { log += "i"; return new T("hh5-inner"); }
            log += "hb";
        }
        log += "never";
    }
    return new T("hh5-body");
}
# the replaced value is released when the handler's return raises an exception
auto sub thrower() { log += "thr"; throw "EV"; }
T sub tc1() { on_exit { log += "x"; } on_exit { auto v = 1; return v; } return new T("body"); }
T sub ev1() { on_exit { log += "x"; } on_exit { return thrower(); } return new T("body"); }
auto sub ev3() {
    try {
        on_exit { log += "x"; }
        on_exit { return thrower(); }
        return new T("body");
    } catch (hash<ExceptionInfo> ex) {
        log += "catch";
    }
    return "after";
}
# a return in a try statement in a handler replaces the value when the try statement ends
auto sub tr1() {
    on_exit { try { log += "h"; return new T("tr1-h"); } catch (hash<ExceptionInfo> ex) { log += "c"; } }
    return new T("tr1-body");
}
auto sub tr2() {
    on_exit { try { thrower(); } catch (hash<ExceptionInfo> ex) { log += "c"; return new T("tr2-c"); } }
    return new T("tr2-body");
}
auto sub tr3() {
    on_exit {
        try {
            try { return new T("tr3-in"); } catch (hash<ExceptionInfo> ex) {}
            log += "never";
        } catch (hash<ExceptionInfo> ex) {}
    }
    return new T("tr3-body");
}
auto sub tr4() {
    on_exit { try { log += "h"; return; } catch (hash<ExceptionInfo> ex) {} }
    return new T("tr4-body");
}
auto sub tr5() {
    on_exit { log += "o"; }
    on_exit {
        try {
            T l("tr5-l");
            on_exit { log += "ti"; }
            return new T("tr5-h");
        } catch (hash<ExceptionInfo> ex) {}
    }
    return new T("tr5-body");
}
auto sub tr6() {
    on_exit { return new T("tr6-last"); }
    on_exit { try { return new T("tr6-try"); } catch (hash<ExceptionInfo> ex) {} }
    return new T("tr6-body");
}
auto sub tr7() {
    on_exit { try { return "tr7-h"; } catch (hash<ExceptionInfo> ex) {} }
    log += "body";
}
# a handler run while an exception leaves its block returns: its value replaces the interrupted return's
class TT inherits T {
    constructor(string n) : T(n) {}
    destructor() { log += "dthrow"; throw "DT"; }
}
auto sub xr1() {
    T z("xr1-z");
    on_exit { return new T("xr1-h"); }
    return (new TT("xr1-tt"), new T("xr1-body"))[1];
}
auto sub xr2() {
    T x("xr2-x");
    on_error { log += "err"; return new T("xr2-err"); }
    on_exit { log += "exit"; }
    throw "E2";
}
auto sub xr3() {
    T z("xr3-z");
    try {
        on_exit { return new T("xr3-h"); }
        return (new TT("xr3-tt"), new T("xr3-body"))[1];
    } catch (hash<ExceptionInfo> ex) {
        log += "catch";
    }
    return "xr3-after";
}
# a try statement catches the exception after the handler's return
auto sub xr4() {
    for (int i = 0; i < 2; ++i) {
        try {
            { on_error { return new T("xr4-h" + i); } throw "E4"; }
        } catch (hash<ExceptionInfo> ex) {
            log += "caught" + i;
        }
    }
    return "xr4-after";
}
# a handler that returns while an exception is pending in a try statement in another handler: its value is the try
# statement's interrupted return, and the function's return is kept
auto sub xr5() {
    on_exit {
        try {
            { on_error { return new T("xr5-h"); } thrower(); }
        } catch (hash<ExceptionInfo> ex) {
            log += "c";
        }
    }
    return new T("xr5-b");
}
auto sub xr6() {
    on_success {
        try {
            { on_error { return new T("xr6-h"); } thrower(); }
        } catch (hash<ExceptionInfo> ex) {
            log += "c";
        }
    }
    return new T("xr6-b");
}
# the same in a handler run while an exception leaves its block (out of line)
auto sub xr7() {
    on_exit {
        try {
            { on_error { return new T("xr7-h"); } thrower(); }
        } catch (hash<ExceptionInfo> ex) {
            log += "c";
        }
    }
    T b("xr7-b");
    throw "E7";
}
# in a method and a closure
class C {
    auto m() {
        on_exit { log += "mexit"; }
        on_success { return new T("m-s"); }
        return new T("m-body");
    }
}
code cl = auto sub () {
    on_exit { log += "cx"; }
    on_success { return new T("cl-s"); }
    return new T("cl-body");
};

sub show(string label, code c) {
    for (int i = 0; i < 2; ++i) {
        log = ();
        try {
            auto v = c();
            log += "rv:" + (v instanceof T ? v.n : (exists v ? sprintf("%s", v) : "nothing"));
        } catch (hash<ExceptionInfo> ex) {
            log += "ex:" + ex.err;
        }
        printf("%s: %y\n", label, log);
    }
}
for (int i = 0; i < 2; ++i) {
    printf("f: %s\n", f());
}
show("g", \g());
show("q2", \q2());
show("th", \th());
show("nv", \nv());
show("n1", \n1());
show("n2", \n2());
show("ft", \ft());
show("mx", \mx());
show("tc", \tc());
show("hh1", \hh1());
show("hh2", \hh2());
show("hh3", \hh3());
show("hh4", \hh4());
show("hh5", \hh5());
show("tc1", \tc1());
show("ev1", \ev1());
show("ev3", \ev3());
show("tr1", \tr1());
show("tr2", \tr2());
show("tr3", \tr3());
show("tr4", \tr4());
show("tr5", \tr5());
show("tr6", \tr6());
show("tr7", \tr7());
show("xr1", \xr1());
show("xr2", \xr2());
show("xr3", \xr3());
show("xr4", \xr4());
show("xr5", \xr5());
show("xr6", \xr6());
show("xr7", \xr7());
show("m", auto sub () { C c(); return c.m(); });
show("cl", cl);
