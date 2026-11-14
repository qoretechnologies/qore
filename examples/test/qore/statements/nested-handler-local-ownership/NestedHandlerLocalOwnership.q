# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
our list<string> events = ();
class Item {
    public { string n; bool raises; }
    constructor(string n, bool raises = False) { self.n = n; self.raises = raises; }
    destructor() {
        events += "~" + n;
        if (raises) { throw "DTOR"; }
    }
}
Item sub make(string n) { return new Item(n); }
sub original() {
    Item a = make("a");
    on_exit { events += "exit"; }
    on_error { a = make("b"); events += "assigned"; }
    throw "E";
}
string sub nested() {
    Item a = make("a");
    on_exit {
        on_exit { a = make("b"); events += "assigned"; }
        on_success { return a.n; }
        events += a.n;
    }
    on_success { return a.n; }
    return "unused";
}
string sub nestedAfterAssignment() {
    Item a = make("a");
    on_exit {
        on_exit { a = make("c"); events += "second"; }
        on_success { return a.n; }
        a = make("b");
        events += "first";
    }
    on_success { return a.n; }
    return "unused";
}
string sub errorHandler() {
    Item a = make("a");
    on_error {
        on_exit { a = make("b"); events += "assigned"; }
        on_success { return a.n; }
        Item inner = make("inner");
        events += a.n;
    }
    throw "E";
}
string sub throwingDestructor() {
    Item a("a", True);
    on_exit {
        on_exit {
            try { a = make("b"); events += "unreachable"; }
            catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
            events += "after-catch";
        }
        on_success { return a.n; }
        events += a.n;
    }
    on_success { return a.n; }
    return "unused";
}
sub readAfter() {
    Item a = make("a");
    on_exit { events += "outer:" + a.n; }
    on_error {
        {
            on_exit { a = make("b"); events += "assigned"; }
            events += a.n;
            throw "INNER";
        }
    }
    throw "E";
}
sub report(string name) { printf("%s: %y\n", name, events); events = (); }
for (int i = 0; i < 2; ++i) {
    try { original(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("original");
    nested(); report("nested");
    # Different handler return values are intentionally unused here; #5992 covers their propagation.
    nestedAfterAssignment(); report("nested-after-assignment");
    try { errorHandler(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("error-handler");
    try { throwingDestructor(); } catch (hash<ExceptionInfo> ex) { events += "escaped:" + ex.err; }
    report("throwing-destructor");
    try { readAfter(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("read-after");
}
