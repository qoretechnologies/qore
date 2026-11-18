# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern
%disable-warning duplicate-local-vars

our list<string> events = ();
class Item {
    public { string n; bool raises; }
    constructor(string n, bool raises = False) {
        self.n = n;
        self.raises = raises;
    }
    destructor() {
        events += "~" + n;
        if (raises) { throw "DTOR"; }
    }
}
Item sub make(string n) { return new Item(n); }

string sub member() {
    Item a = make("a");
    on_exit { a = make("b"); events += "assigned"; }
    on_success { return a.n; }
    return "unused";
}
string sub listMember() {
    list<Item> a = (make("a"),);
    on_exit { a = (make("b"),); events += "assigned"; }
    on_success { return a[0].n; }
    return "unused";
}
string sub hashMember() {
    hash<string, Item> a = {"key": make("a")};
    on_exit { a = {"key": make("b")}; events += "assigned"; }
    on_success { return a.key.n; }
    return "unused";
}
string sub exitReturn() {
    Item a = make("a");
    on_exit { a = make("b"); events += "assigned"; }
    on_exit { return a.n; }
    return "unused";
}
string sub removeLocal() {
    Item a = make("a");
    on_exit { remove a; events += "removed"; }
    on_success { return a.n; }
    return "unused";
}
string sub throwingDestructor() {
    Item a("a", True);
    on_exit {
        try { a = make("b"); events += "unreachable"; }
        catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
        events += "after-catch";
    }
    on_success { return a.n; }
    return "unused";
}
string sub closureLocal() {
    Item a = make("a");
    code<nothing()> replace = nothing sub () { a = make("b"); events += "assigned"; };
    on_exit { replace(); }
    on_success { return a.n; }
    return "unused";
}
string sub referenceLocal() {
    Item a = make("a");
    reference<Item> r = \a;
    on_exit { r = make("b"); events += "assigned"; }
    on_success { return a.n; }
    return "unused";
}
Item sub returnedObject() {
    Item a = make("a");
    on_exit { a = make("b"); events += "assigned"; }
    on_success { return a; }
    return a;
}
string sub constantReturn() {
    Item a = make("a");
    on_exit { a = make("b"); events += "assigned"; }
    on_success { return "constant"; }
    return "unused";
}
string sub successiveHandlers() {
    Item a = make("a");
    on_exit { events += a.n; a = make("c"); events += "second"; }
    on_exit { a = make("b"); events += "first"; }
    on_success { return a.n; }
    return "unused";
}
string sub shadowedLocal() {
    Item a = make("a");
    on_exit {
        a = make("b");
        { Item a = make("shadow"); events += a.n; }
        events += "assigned";
    }
    on_success { return a.n; }
    return "unused";
}

string sub constantNestedBlock() {
    Item outer = make("outer");
    on_exit { events += "outer-exit"; }
    {
        Item inner = make("inner");
        on_success { return "constant"; }
        return "unused";
    }
}
string sub handlerLocal() {
    Item outer = make("outer");
    on_exit { events += "outer-exit"; }
    on_success {
        Item inner = make("inner");
        return inner.n;
    }
    return "unused";
}

sub report(string name, string value) {
    printf("%s: %y -> %s\n", name, events, value);
    events = ();
}
for (int iteration = 0; iteration < 2; ++iteration) {
    report("member", member());
    report("list", listMember());
    report("hash", hashMember());
    report("on-exit-return", exitReturn());
    report("remove", removeLocal());
    try { report("throwing-dtor", throwingDestructor()); }
    catch (hash<ExceptionInfo> ex) { report("throwing-dtor-escaped", ex.err); }
    report("closure", closureLocal());
    report("reference", referenceLocal());
    {
        Item result = returnedObject();
        events += "result:" + result.n;
    }
    report("returned-object", "done");
    report("constant", constantReturn());
    report("successive-handlers", successiveHandlers());
    report("shadowed-local", shadowedLocal());
    report("nested-block-constant", constantNestedBlock());
    report("handler-local", handlerLocal());
}
