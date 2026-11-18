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
string sub fail() { throw "E2"; }

sub localInit() {
    Item a = make("a");
    on_exit { a = make("b"); events += "exit"; }
    string s = a.n + fail();
}
string sub returnThrow() {
    Item a = make("a");
    on_exit { a = make("b"); events += "exit"; }
    return a.n + fail();
}
string sub errorHandler() {
    Item a = make("a");
    on_error { a = make("b"); events += "error"; }
    return a.n + fail();
}
int sub rejectedReturnExit() {
    hash<auto> h = {"o": make("a"), "s": "bad"};
    on_exit { h = {}; events += "exit"; }
    return h.o ? h.s : 0;
}
int sub rejectedReturnError() {
    hash<auto> h = {"o": make("a"), "s": "bad"};
    on_error { h = {}; events += "error"; }
    return h.o ? h.s : 0;
}
int sub tryRejectedReturn() {
    hash<auto> h = {"o": make("a"), "s": "bad"};
    try {
        on_exit { h = {}; events += "exit"; }
        return h.s;
    } catch (hash<ExceptionInfo> ex) {
        events += "inner:" + ex.err;
    }
    return 7;
}
sub typedLocal() {
    hash<auto> h = {"o": make("a"), "s": "bad"};
    on_exit { h = {}; events += "exit"; }
    int value = h.s;
}
sub listRead() {
    list<Item> items = (make("a"),);
    on_exit { items = (); events += "exit"; }
    string s = items[0].n + fail();
}
sub hashRead() {
    hash<string, Item> items = {"key": make("a")};
    on_exit { items = {}; events += "exit"; }
    string s = items.key.n + fail();
}
sub nestedScopes() {
    Item outer = make("outer");
    on_exit { outer = make("outer2"); events += "outer-exit"; }
    {
        Item inner = make("inner");
        on_error { inner = make("inner2"); events += "inner-error"; }
        string s = inner.n + fail();
    }
}
sub throwingDestructor() {
    Item a("a", True);
    on_error {
        try { a = make("b"); events += "unreachable"; }
        catch (hash<ExceptionInfo> ex) { events += "handler:" + ex.err; }
        events += "handler-end";
    }
    string s = a.n + fail();
}
sub temporaryRead() {
    on_exit { events += "exit"; }
    string s = make("temp").n + fail();
}
sub show(string name) { printf("%s: %y\n", name, events); events = (); }
for (int i = 0; i < 2; ++i) {
    try { localInit(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("local-init");
    try { returnThrow(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("return-throw");
    try { errorHandler(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("error-handler");
    try { rejectedReturnExit(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("rejected-return-exit");
    try { rejectedReturnError(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("rejected-return-error");
    try { events += "result:" + tryRejectedReturn(); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("try-rejected-return");
    try { typedLocal(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("typed-local");
    try { listRead(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("list-read");
    try { hashRead(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("hash-read");
    try { nestedScopes(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("nested-scopes");
    try { throwingDestructor(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("throwing-destructor");
    try { temporaryRead(); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    show("temporary-read");
}
