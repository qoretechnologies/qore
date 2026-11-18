# -*- mode: qore; indent-tabs-mode: nil -*-
# the script of call-site-frames.qtest and call-site-frames-in-process.qtest: the line and column numbers that its
# calls are made at are the expected values in CallSiteFrames.qc
%modern
our list<string> out = ();
sub rec(string what, hash<CallStackInfo> f) {
    out += sprintf("%s: %s %d-%d %d-%d", what, f.function, f.line, f.endline, f.start_column, f.end_column);
}
# records the frame of the code that calls it: 0 is get_thread_call_stack(), 1 is fr(), 2 is the caller's frame
sub fr(string what) { rec(what, get_thread_call_stack()[2]); }
int sub g(int i) { fr("g"); return i; }
int sub mi() { fr("member initializer"); return 1; }
int sub ba() { fr("base constructor argument"); return 1; }
int sub da() { fr("default argument"); return 1; }
int sub withDefault(int a = da()) { return a; }
class Base {
    constructor(int i) { fr("base constructor"); }
}
class C inherits Base {
    private { int v = mi(); }
    constructor() : Base(ba()) { fr("constructor"); }
    int m(int i) { fr("method"); return i; }
    static int s(int i) { fr("static method"); return i; }
    int selfCall() { return m(1) +
        m(2); }
}
int sub thr() { throw "TEST", "test"; }
int x = 0;
C c();
code cl = int sub (int i) { fr("closure"); return i; };
code fref = \g();
# the ternary operator does not evaluate g(1)
int r = (x ? g(1) : 0) +
    g(2);
# two calls in a statement of two lines
r = c.m(3) +
   cl(4);
r = C::s(5) + fref(
    6);
r = c.selfCall();
C c2 = new
    C();
bool b = x && g(7) || g(8);
r = withDefault();
try {
    r = 1 +
        thr();
} catch (hash<ExceptionInfo> ex) {
    rec("exception", ex.callstack[0]);
}
# a temporary object whose destructor makes a call runs between the calls of a statement
int sub quiet() { return 1; }
class D { destructor() { quiet(); } }
int sub takes(object o, int i) { fr("takes"); return i; }
int sub mk() { D d(); return 1; }
r = takes(new D(), 1) +
    g(9);
r = g(mk()) + g(takes(new D(), 2));
%ifndef NO_OUTPUT
printf("%s\n", out.join("\n"));
%endif
