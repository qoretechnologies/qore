#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# the probe of RejectedReturnValueRelease.qtest: the value of a return that the declared return type rejects is
# released after the locals of the blocks that the exception leaves, as the AST interpreter releases it (qore#5996)

%modern

our list<string> log = ();
# the number of T and U objects alive: a value that is not released leaves it above zero
our int live = 0;

class T {
    public { string n; }
    constructor(string n) { self.n = n; ++live; }
    destructor() { log += "~" + n; --live; }
}

class U {
    public { string n; }
    constructor(string n) { self.n = n; ++live; }
    destructor() { log += "~" + n; --live; }
}

auto sub mku(string n) { return new U(n); }

# the exception leaves the frame
T sub k2() { T z("k2-z"); return mku("k2-u"); }

# a try statement of the frame catches the exception
T sub k3() {
    try {
        T z("k3-z");
        if (True) { T q("k3-q"); return mku("k3-u"); }
    } catch () { log += "c"; }
    return new T("k3-other");
}

# the rejected value is the value of a local
T sub k4() { T z("k4-z"); auto u = new U("k4-u"); return u; }

# the rejected value is a container holding an object
T sub k5() { T z("k5-z"); auto l = (new U("k5-u"),); return l; }

# on_exit handlers of the blocks that the exception leaves
T sub k6() {
    on_exit { log += "x1"; }
    T z("k6-z");
    {
        on_exit { log += "x2"; }
        T q("k6-q");
        return mku("k6-u");
    }
}

# a nullable return type rejects the value; a try statement two blocks out catches the exception
*T sub k7() {
    try {
        T a("k7-a");
        {
            T b("k7-b");
            {
                T c("k7-c");
                return mku("k7-u");
            }
        }
    } catch (hash<ExceptionInfo> e) {
        log += "c:" + e.err;
    }
}

# a method and a static method
class M {
    T m() { T z("m-z"); return mku("m-u"); }
    static T s() { T z("s-z"); return mku("s-u"); }
}

# a closure
code k8 = T sub () { T z("k8-z"); return mku("k8-u"); };

# the return type accepts the value: no exception, the value is returned
T sub k9() { U z("k9-z"); return new T("k9-t"); }


# more returns whose value the return type rejects, and returns that it accepts, from the review (qore#5996)
our auto g;
# try with no locals: check emitted, no cleanup_before_return
T sub a1(auto u) { try { return u; } catch () { log += "c"; } return new T("a1-o"); }
T sub a2(auto u) { try { return u; } catch (hash<ExceptionInfo> e) { log += "c"; rethrow; } }
T sub a3() { try { return mku("a3-u"); } catch () { log += "c"; } return new T("a3-o"); }
# global / member
T sub a4() { T z("a4-z"); return g; }
class MM { public { auto m; } T f() { T z("m-z"); return m; } T f2() { try { return m; } catch () { log += "mc"; } return new T("m-o"); } }
# on_error
T sub a5() { on_error { log += "err"; } auto u = new U("a5-u"); return u; }
# foreach / switch / context
T sub a6() { T z("a6-z"); foreach auto x in ((new U("a6-u"),)) { T q("a6-q"); return x; } }
T sub a7(int i) { T z("a7-z"); switch (i) { case 1: { auto u = new U("a7-u"); return u; } } }
# *T nothing ok
*T sub a8() { T z("a8-z"); auto n; return n; }
# nested frames: callee raise caught by caller frame
T sub a9in() { T z("a9in-z"); auto u = new U("a9in-u"); return u; }
T sub a9() { T z("a9-z"); try { return a9in(); } catch () { log += "c"; } return new T("a9-o"); }
# rejected inside try with locals rethrow from catch
T sub a10() { try { T z("a10-z"); auto u = new U("a10-u"); return u; } catch (hash<ExceptionInfo> e) { log += "c"; throw "X"; } }
# success path, local return and member, try without locals
T sub b1(T t) { try { return t; } catch () {} }
T sub b2() { T z("b2-z"); T t("b2-t"); return t; }
T sub b3() { try { T t("b3-t"); return t; } catch () {} }
string sub b4(string s) { try { return s; } catch () {} }
list<T> sub b5() { try { list<T> l = (new T("b5-t"),); return l; } catch () {} }
# closure capturing local
T sub a11() { auto u = new U("a11-u"); code c = auto sub () { return u; }; T z("a11-z"); return u; }
%allow-weak-references
T sub c1(bool b) { T z("c1-z"); auto u = new U("c1-u"); return b ? u : mku("c1-x"); }
T sub c2() { T z("c2-z"); auto u = new U("c2-u"); return u ?? mku("c2-x"); }
T sub c3() { T z("c3-z"); auto u = new U("c3-u"); auto w := u; return w; }
T sub c4() { T z("c4-z"); auto u = new U("c4-u"); return (mku("c4-x"), u)[1]; }
T sub c5() { T z("c5-z"); hash h = {"a": new U("c5-u")}; return h.a; }
T sub c6() { T z("c6-z"); auto u = new U("c6-u"); try { T q("c6-q"); return b() ? u : mku("c6-x"); } catch () { log += "c"; } return new T("c6-o"); }
bool sub b() { return True; }
T sub d1() { T z("d1-z"); on_exit { try { throw "IN"; } catch () { log += "ic"; } } T y("d1-y"); return mku("d1-u"); }
T sub d2() { T z("d2-z"); on_error { try { throw "IN"; } catch () { log += "ic"; } } auto u = new U("d2-u"); return u; }
T sub d3() { T z("d3-z"); on_exit { log += "x"; } return mku("d3-u"); }
# value returned also in a return of a prior interrupted return? on_exit returns
T sub d4() { T z("d4-z"); { on_exit { T q("d4-q"); } auto u = new U("d4-u"); return u; } }
# AST reference: try inside a nested function called from on_exit
sub inner() { try { throw "IN"; } catch () { log += "ic"; } }
T sub d5() { T z("d5-z"); on_exit { inner(); } T y("d5-y"); return mku("d5-u"); }

for (int n = 0; n < 2; ++n) {
    M m();
    foreach string fn in (("k2", "k3", "k4", "k5", "k6", "k7", "m", "s", "k8", "k9")) {
        log = ();
        try {
            auto r;
            switch (fn) {
                case "m": r = m.m(); break;
                case "s": r = M::s(); break;
                case "k8": r = k8(); break;
                default: r = call_function(fn); break;
            }
            log += sprintf("r:%s", r ? r.className() : "none");
        } catch (hash<ExceptionInfo> e) {
            log += "caught:" + e.err;
        }
        printf("%s: %y\n", fn, log);
    }
}

for (int n = 0; n < 2; ++n) {
    MM mm();
    mm.m = new U("mm-u");
    g = new U("g-u");
    foreach string fn in (("a1", "a2", "a3", "a4", "mf", "mf2", "a5", "a6", "a7", "a8", "a9", "a10", "b1", "b2", "b3",
            "b4", "b5", "a11", "c1", "c2", "c3", "c4", "c5", "c6", "d1", "d2", "d3", "d4", "d5")) {
        log = ();
        try {
            auto r;
            switch (fn) {
                case "a1": r = a1(new U("a1-u")); break;
                case "a2": r = a2(new U("a2-u")); break;
                case "mf": r = mm.f(); break;
                case "mf2": r = mm.f2(); break;
                case "a7": r = a7(1); break;
                case "b1": r = b1(new T("b1-t")); break;
                case "b4": r = b4("x"); break;
                case "c1": r = c1(True); break;
                default: r = call_function(fn); break;
            }
            log += sprintf("r:%y", r instanceof T ? r.n : r instanceof list ? r.size() : r);
        } catch (hash<ExceptionInfo> e) {
            log += "caught:" + e.err;
        }
        printf("%s: %y live=%d\n", fn, log, live - 2);
    }
    delete mm;
    g = NOTHING;
}
printf("final live=%d\n", live);
