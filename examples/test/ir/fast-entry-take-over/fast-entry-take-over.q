%modern
# the release order of the temporary arguments that a call gives to a callee whose parameter takes them over, in and
# outside try blocks (issue #6105); the AST interpreter's output is fast-entry-take-over.expected

class T {
    private { string n; }
    constructor(string n) { self.n = n; }
    destructor() { printf("dtor %s\n", n); }
}

sub log(string s) { printf("%s\n", s); }

# the parameter is written: the temporary is destroyed at the write
int sub w(list<auto> l) { int n = l.size(); l = (); log("w written"); return n; }
# the parameter is written, then an exception is raised
int sub wx(list<auto> l) { l = (); log("wx written"); throw "WX"; }
# an exception is raised before the parameter is written
int sub xw(list<auto> l, bool t) { if (t) { throw "XW"; } l = (); return 0; }
# two parameters: the first is written, the second only read
int sub w2(list<auto> a, list<auto> b) { a = (); log("w2 written a"); return b.size(); }
# two parameters: the second is written
int sub w2b(list<auto> a, list<auto> b) { b = (); log("w2b written b"); return a.size(); }
# the value is kept in a local before the parameter is written
list<auto> sub keep(list<auto> l) { list<auto> r = l; l = (); log("keep written"); return r; }
# recursion
int sub rec(list<auto> l, int d) { if (d) { rec((0, new T("rec" + d)), d - 1); } l = (); log("rec written " + d); return d; }
# a typed list parameter
int sub wl(list<T> l) { l = (); log("wl written"); return 0; }
# a parameter with a type that a run-time check binds
int sub wt(list<auto> l, int i) { l = (); log("wt written"); return i; }

# the static methods of a final class have fast entries
final class F {
    static int s(list<auto> l) { int n = l.size(); l = (); log("F::s written"); return n; }
    static int sx(list<auto> l) { l = (); log("F::sx written"); throw "FSX"; }
}

# the static methods of a class that is not final are called through the runtime
class C {
    static int s(list<auto> l) { int n = l.size(); l = (); log("s written"); return n; }
    static int sx(list<auto> l) { l = (); log("sx written"); throw "SX"; }
    int m(list<auto> l) { int n = l.size(); l = (); log("m written"); return n; }
}

sub in_try() {
    try {
        w((0, new T("w")));
        log("after w");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        wx((0, new T("wx")));
        log("after wx");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        xw((0, new T("xw")), True);
        log("after xw");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    list<auto> kept = (0, new T("kept"));
    try {
        w2((0, new T("w2")), kept);
        log("after w2");
        w2b(kept, (0, new T("w2b")));
        log("after w2b");
        w2((0, new T("w2-a")), (0, new T("w2-b")));
        log("after w2 both");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        list<auto> r = keep((0, new T("keep")));
        log("after keep");
        remove r;
        log("removed keep");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        wl((new T("wl"),));
        log("after wl");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        rec((0, new T("rec")), 2);
        log("after rec");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        # the argument is converted when the parameter is bound, which raises an exception for the second call
        auto v = 1;
        wt((0, new T("wt")), v);
        log("after wt");
        v = "x";
        wt((0, new T("wt-bind")), v);
        log("after wt-bind");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        C::s((0, new T("s")));
        log("after s");
        C::sx((0, new T("sx")));
        log("after sx");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        F::s((0, new T("fs")));
        log("after F::s");
        F::sx((0, new T("fsx")));
        log("after F::sx");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        C c();
        c.m((0, new T("m")));
        log("after m");
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    try {
        for (int i = 0; i < 2; ++i) {
            w((0, new T("loop" + i)));
            log("after loop " + i);
        }
    } catch (hash<ExceptionInfo> ex) {
        log("catch " + ex.err);
    }
    log("kept " + kept.size());
}

sub outside_try() {
    w((0, new T("ow")));
    log("after ow");
    C::s((0, new T("os")));
    log("after os");
    F::s((0, new T("ofs")));
    log("after ofs");
    list<auto> kept = (0, new T("okept"));
    w2((0, new T("ow2")), kept);
    log("after ow2");
}

in_try();
log("in try done");
outside_try();
log("outside try done");
try {
    w((0, new T("top")));
    log("after top");
} catch (hash<ExceptionInfo> ex) {
    log("catch " + ex.err);
}
