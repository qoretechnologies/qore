# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
our list<string> log = ();
our int live = 0;
class T {
    public { string n; }
    constructor(string n) { self.n = n; ++live; }
    destructor() { log += "~" + n; --live; }
}
class TT inherits T {
    constructor(string n) : T(n) {}
    destructor() { log += "dthrow:" + n; throw "DT-" + n; }
}
class A { }
class B { }
auto sub tag() { return 1; }
auto sub bad_tag() { return new T("v"); }
string sub chain(hash<ExceptionInfo> ex) {
    string s = ex.err;
    while (ex.next) { ex = ex.next; s += "/" + ex.err; }
    return s;
}
final class C {
    auto first(int x, auto a) { return a; }
    auto first(string x, auto a) { return a; }
    auto typed(A x, auto a) { return a; }
    auto typed(B x, auto a) { return a; }
    auto object_count() {
        T z("z"); { T q("q"); return first(tag(), new T("v"), new TT("tt")); }
    }
    auto list_count() {
        T z("z"); { T q("q"); return first(tag(), (new T("v"),), (new TT("tt"),)); }
    }
    auto hash_count() {
        T z("z"); { T q("q"); return first(tag(), {"v": new T("v")}, {"t": new TT("tt")}); }
    }
    auto no_throw_count() {
        T z("z"); { T q("q"); return first(tag(), new T("v"), new T("t")); }
    }
    auto both_throw_count() {
        T z("z"); { T q("q"); return first(tag(), new TT("v"), new TT("tt")); }
    }
    auto type_reject() {
        T z("z"); { T q("q"); return typed(bad_tag(), new TT("tt")); }
    }
    auto invoked() {
        T z("z");
        try { T q("q"); return first(tag(), new T("v"), new TT("tt")); }
        catch (hash<ExceptionInfo> ex) { log += "inner:" + chain(ex); }
        log += "continued";
        return 7;
    }
    auto invoked_type() {
        T z("z");
        try { T q("q"); return typed(bad_tag(), new TT("tt")); }
        catch (hash<ExceptionInfo> ex) { log += "inner:" + chain(ex); }
        log += "continued";
        return 7;
    }
    auto handlers() {
        T z("z");
        on_error { log += "error"; }
        on_exit { log += "exit"; }
        { T q("q"); return first(tag(), new T("v"), new TT("tt")); }
    }
    auto zero(*int x) { throw "ZERO"; }
    auto zero(*string x) { throw "ZERO"; }
    auto zero_args() {
        T z("z"); { T q("q"); return zero(); }
    }
    auto successful() {
        T z("z"); { T q("q"); return first(tag(), new T("v")); }
    }
}
C c();
for (int repeat = 0; repeat < 3; ++repeat) {
    foreach string name in (("object_count", "list_count", "hash_count", "no_throw_count", "both_throw_count",
            "type_reject", "invoked", "invoked_type", "handlers", "successful", "zero_args")) {
        log = ();
        try { auto r = call_object_method(c, name); log += "got"; }
        catch (hash<ExceptionInfo> ex) { log += "outer:" + chain(ex); }
        printf("%s: %y live=%d\n", name, log, live);
        if (live) { throw "LEAK", sprintf("%s left %d live objects", name, live); }
    }
}
