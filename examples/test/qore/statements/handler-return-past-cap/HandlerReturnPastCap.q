# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A return in a handler run past the inline cap of the lowering of handlers replaces the value of the return (#6137);
# see handler-return-past-cap.qtest.  The nested handlers are written without indentation: each "on_exit {" line
# opens a handler in the body of the one before; the bodies of the outermost two and the innermost two return after
# the handlers nested in them are registered (each conditional return lowers the handlers after it again, so that the
# code grows to the budget of the handlers lowered inline).

%modern
%enable-all-warnings
our list<string> lg = ();
class T {
    public { string n; }
    constructor(string n) { self.n = n; }
    destructor() { lg += "~" + n; }
}
string sub show(auto v) {
    return v.typeCode() == NT_OBJECT ? "T:" + v.n : sprintf("%y", v);
}

# handlers nested 15 deep; handler k (1, 2, 14, 15) returns when x is k or negative
auto sub n15(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 15) return "n15";
}
if (x < 0 || x == 14) return "n14";
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return "n2";
}
if (x < 0 || x == 1) return "n1";
}
    return "body";
}
# handlers nested 16 deep; handler k (1, 2, 15, 16) returns when x is k or negative
auto sub n16(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 16) return "n16";
}
if (x < 0 || x == 15) return "n15";
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return "n2";
}
if (x < 0 || x == 1) return "n1";
}
    return "body";
}
# handlers nested 17 deep; handler k (1, 2, 16, 17) returns when x is k or negative
auto sub n17(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 17) return "n17";
}
if (x < 0 || x == 16) return "n16";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return "n2";
}
if (x < 0 || x == 1) return "n1";
}
    return "body";
}
auto sub o17(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 17) return new T("o17-17");
}
if (x < 0 || x == 16) return new T("o17-16");
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return new T("o17-2");
}
if (x < 0 || x == 1) return new T("o17-1");
}
    return new T("o17-body");
}
auto sub r17(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 17) return;
}
if (x < 0 || x == 16) return "r16";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return "r2";
}
if (x < 0 || x == 1) return;
}
    return;
}
# a handler's return without a value replaces the value with no value
auto sub v17(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x == 17) return;
}
if (x == 16) return;
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x == 2) return;
}
if (x == 1) return;
}
    return "v-body";
}
# handlers nested 32 deep; handler k (1, 2, 31, 32) returns when x is k or negative
auto sub n32(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 32) return "n32";
}
if (x < 0 || x == 31) return "n31";
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return "n2";
}
if (x < 0 || x == 1) return "n1";
}
    return "body";
}
auto sub o32(int x) {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 32) return new T("o32-32");
}
if (x < 0 || x == 31) return new T("o32-31");
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 2) return new T("o32-2");
}
if (x < 0 || x == 1) return new T("o32-1");
}
    return new T("o32-body");
}
# handler k registers the next one in a block of its own, which ends without a return: past the cap, the handlers
# run at the end of that block
auto sub b17(int x) {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
on_exit { {
if (x < 0 || x == 17) return "b17";
} }
if (x < 0 || x == 16) return "b16";
} }
if (x < 0 || x == 15) return "b15";
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
x += 0;
} }
    return "b-body";
}
# the handler registered first runs last and replaces the value that the nested handlers past the cap gave, also
# with a return in a try statement
auto sub p(int x) {
    on_exit { if (x < -1) return "outer"; }
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 17) return new T("p17");
}
if (x < 0 || x == 16) return new T("p16");
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 1) return new T("p1");
}
    return new T("p-body");
}
auto sub q(int x) {
    on_exit { try { if (x < -1) return "outer-try"; } catch (hash<ExceptionInfo> ex) { lg += ex.err; } }
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
on_exit {
if (x < 0 || x == 17) return new T("q17");
}
if (x < 0 || x == 16) return new T("q16");
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
if (x < 0 || x == 1) return new T("q1");
}
    return new T("q-body");
}
# handlers of one scope that return conditionally, several of them for the same argument: past the budget of the
# handlers lowered inline
auto sub c7(int x) {
    on_exit { if (x & 2) return "c1"; }
    on_exit { if (x & 4) return "c2"; }
    on_exit { if (x & 1) return "c3"; }
    on_exit { if (x & 2) return "c4"; }
    on_exit { if (x & 4) return "c5"; }
    on_exit { if (x & 1) return "c6"; }
    on_exit { if (x & 2) return "c7"; }
    return "c-body";
}
# on_exit, on_success and on_error handlers
auto sub s(int x) {
    on_success { if (x & 2) return new T("s1"); }
    on_error { if (x & 4) return new T("s2"); }
    on_exit { if (x & 8) return new T("s3"); }
    on_success { if (x & 1) return new T("s4"); }
    on_error { if (x & 2) return new T("s5"); }
    on_exit { if (x & 4) return new T("s6"); }
    on_success { if (x & 8) return new T("s7"); }
    on_error { if (x & 1) return new T("s8"); }
    if (x & 16) throw "S";
    return new T("s-body");
}

sub run(string label, code f, list<int> args) {
    foreach int x in (args) {
        for (int i = 0; i < 2; ++i) {
            lg = ();
            string v;
            try {
                v = show(f(x));
            } catch (hash<ExceptionInfo> ex) {
                v = "ex:" + ex.err;
            }
            printf("%s(%d)[%d]: %s %y\n", label, x, i, v, lg);
        }
    }
}

run("n15", \n15(), (-1, 0, 1, 2, 14, 15));
run("n16", \n16(), (-1, 0, 1, 2, 14, 15, 16));
run("n17", \n17(), (-1, 0, 1, 2, 14, 16, 17));
run("o17", \o17(), (-1, 0, 1, 2, 14, 16, 17));
run("r17", \r17(), (-1, 0, 1, 2, 14, 16, 17));
run("v17", \v17(), (0, 1, 16, 17));
run("n32", \n32(), (-1, 0, 1, 2, 14, 31, 32));
run("o32", \o32(), (-1, 0, 1, 2, 14, 31, 32));
run("b17", \b17(), (-1, 0, 15, 16, 17));
run("p", \p(), (-2, -1, 0, 1, 16, 17));
run("q", \q(), (-2, -1, 0, 1, 16, 17));
run("c7", \c7(), (0, 1, 2, 3, 4, 5, 6, 7, 8, 15));
run("s", \s(), (0, 1, 2, 3, 5, 6, 7, 15, 16, 17, 18, 31));
