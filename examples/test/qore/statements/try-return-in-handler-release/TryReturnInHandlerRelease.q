# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A return in a try statement of a handler replaces a value whose destructor throws (#6128); see
# try-return-in-handler-release.qtest

%modern
%enable-all-warnings
our list<string> lg = ();
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { lg += "~" + n; } }
# a value whose destructor throws
class TT { public { string n; } constructor(string n) { self.n = n; } destructor() { lg += "~" + n; throw "DTOR-" + n; } }

auto sub u1() {
    on_exit { try { return new T("u1-h"); } catch (hash<ExceptionInfo> ex) { lg += "c:" + ex.err; } }
    return new TT("u1-body");
}
# with an on_error handler of the function's block, which runs for the exception
auto sub u2() {
    on_exit { try { return new T("u2-h"); } catch (hash<ExceptionInfo> ex) { lg += "c:" + ex.err; } }
    on_error { lg += "e"; }
    return new TT("u2-body");
}
# the statements of the handler after the try statement do not run
auto sub u3() {
    on_exit {
        try {
            return new T("u3-h");
        } catch (hash<ExceptionInfo> ex) {
            lg += "c:" + ex.err;
        }
        lg += "after";
    }
    return new TT("u3-body");
}
# a return in a nested try statement
auto sub u4() {
    on_exit {
        try {
            try { return new T("u4-h"); } catch (hash<ExceptionInfo> ex) { lg += "ci:" + ex.err; }
        } catch (hash<ExceptionInfo> ex) {
            lg += "co:" + ex.err;
        }
    }
    return new TT("u4-body");
}
# a return in the catch block of a try statement
auto sub u5() {
    on_exit {
        try {
            throw "U5";
        } catch (hash<ExceptionInfo> ex) {
            lg += "c:" + ex.err;
            return new T("u5-h");
        }
    }
    return new TT("u5-body");
}

# runs the function twice: in jit and tiered mode, the second call runs as native code
sub show(string label, code c) {
    for (int i = 0; i < 2; ++i) {
        lg = ();
        try {
            auto v = c();
            lg += "rv:" + (v instanceof T ? v.n : (exists v ? sprintf("%y", v) : "nothing"));
        } catch (hash<ExceptionInfo> ex) {
            lg += "ex:" + ex.err;
        }
        printf("%s[%d]: %y\n", label, i, lg);
    }
}
show("u1", \u1());
show("u2", \u2());
show("u3", \u3());
show("u4", \u4());
show("u5", \u5());
