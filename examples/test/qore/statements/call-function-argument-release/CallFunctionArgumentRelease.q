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
    destructor() { log += "dthrow"; throw "DT"; }
}
class ResultThrower inherits T {
    constructor(string n) : T(n) {}
    destructor() { log += "dthrow"; throw "RESULT"; }
}
auto sub first(auto a, auto b) { return a; }
auto sub failing(auto a, auto b) { throw "CALL"; }

T sub ref_return() {
    T z("z");
    { T q("q"); return call_function(\first(), new T("v"), new TT("tt")); }
}
auto sub named_return() {
    T z("z");
    { T q("q"); return call_function("first", new T("v"), new TT("tt")); }
}
auto sub list_return() {
    T z("z");
    { T q("q"); return call_function(\first(), (new T("v"),), new TT("tt")); }
}
auto sub hash_return() {
    T z("z");
    { T q("q"); return call_function(\first(), {"v": new T("v")}, new TT("tt")); }
}
auto sub caught_return() {
    T z("z");
    try {
        T q("q");
        return call_function(\first(), new T("v"), new TT("tt"));
    } catch (hash<ExceptionInfo> ex) { log += "inner:" + ex.err; }
    log += "continued";
    return 7;
}
auto sub handlers() {
    T z("z");
    on_error { log += "error"; }
    on_exit { log += "exit"; }
    { T q("q"); return call_function(\first(), new T("v"), new TT("tt")); }
}
auto sub assigned() {
    T z("z");
    { T q("q"); auto value = call_function(\first(), new T("v"), new TT("tt"));
      log += "unreachable"; }
}
auto sub discarded() {
    T z("z");
    { T q("q"); call_function(\first(), new T("v"), new TT("tt"));
      log += "unreachable"; }
}
auto sub result_raises() {
    T z("z");
    { T q("q"); return call_function(\first(), new ResultThrower("v"), new TT("tt")); }
}
auto sub call_raises() {
    T z("z");
    { T q("q"); return call_function(\failing(), new T("v"), new T("t")); }
}
auto sub direct_control() {
    T z("z");
    { T q("q"); return first(new T("v"), new TT("tt")); }
}
# A statement temporary raises after the return has taken its value (#6085).
auto sub statement_control() {
    T z("z");
    { T q("q"); return (new TT("tt"), new T("v"))[1]; }
}
auto sub success_control() {
    T z("z");
    { T q("q"); return call_function(\first(), new T("v"), new T("t")); }
}

for (int repeat = 0; repeat < 3; ++repeat) {
    foreach string name in (("ref_return", "named_return", "list_return", "hash_return", "caught_return",
            "handlers", "assigned", "discarded", "result_raises", "call_raises", "direct_control", "statement_control",
            "success_control")) {
        log = ();
        try {
            auto result = call_function(name);
            log += "got";
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
            if (name == "result_raises") { log += "next:" + ex.next.err; }
        }
        printf("%s: %y live=%d\n", name, log, live);
        if (live) { throw "LEAK", sprintf("%s left %d live objects", name, live); }
    }
}
