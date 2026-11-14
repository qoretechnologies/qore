# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

our list<string> events = ();
class Item {
    public { string name; }
    constructor(string name) { self.name = name; }
    destructor() { events += "~" + name; }
}
Item sub make(string name) { return new Item(name); }

sub handlerObject(Item value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = make("b");
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = make("b");
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = make("b");
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerAnyObject(auto value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = make("b");
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = make("b");
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = make("b");
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerTypedHash(hash<string, Item> value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = {"key": make("b")};
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = {"key": make("b")};
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = {"key": make("b")};
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerHash(hash value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = {"key": make("b")};
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = {"key": make("b")};
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = {"key": make("b")};
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerAnyHash(auto value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = {"key": make("b")};
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = {"key": make("b")};
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = {"key": make("b")};
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerTypedList(list<Item> value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = (make("b"),);
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = (make("b"),);
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = (make("b"),);
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerList(list value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = (make("b"),);
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = (make("b"),);
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = (make("b"),);
                events += "assigned";
            }
        }
        throw "E";
    }
}

sub handlerAnyList(auto value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) {
            value = (make("b"),);
            events += "assigned";
            rethrow;
        }
    } else if (handler == "on-error") {
        on_error {
            value = (make("b"),);
            events += "assigned";
        }
        throw "E";
    } else {
        on_error {
            try { throw "NESTED"; }
            catch (hash<ExceptionInfo> ex) {
                value = (make("b"),);
                events += "assigned";
            }
        }
        throw "E";
    }
}

# Controls distinguish argument ownership from handler-local ownership.
sub plainParameter(Item value) {
    value = make("b");
    events += "assigned";
    events += "exit";
    throw "E";
}
sub handlerLocal() {
    Item value = make("a");
    on_exit { events += "exit"; }
    on_error { value = make("b"); events += "assigned"; }
    throw "E";
}
# Other call shapes: a method, a static method, a closure, a call reference, and calls outside a try block.
class Shape {
    method(Item value, string handler) {
        on_exit { events += "exit"; }
        if (handler == "catch") {
            try { throw "E"; }
            catch (hash<ExceptionInfo> ex) { value = make("b"); events += "assigned"; rethrow; }
        } else {
            on_error { value = make("b"); events += "assigned"; }
            throw "E";
        }
    }
    static staticMethod(Item value, string handler) {
        on_exit { events += "exit"; }
        if (handler == "catch") {
            try { throw "E"; }
            catch (hash<ExceptionInfo> ex) { value = make("b"); events += "assigned"; rethrow; }
        } else {
            on_error { value = make("b"); events += "assigned"; }
            throw "E";
        }
    }
}
code shapeClosure = sub (Item value, string handler) {
    on_exit { events += "exit"; }
    if (handler == "catch") {
        try { throw "E"; }
        catch (hash<ExceptionInfo> ex) { value = make("b"); events += "assigned"; rethrow; }
    } else {
        on_error { value = make("b"); events += "assigned"; }
        throw "E";
    }
};
code shapeCallRef = \handlerObject();
Shape shape();
sub noTryFunction(string handler) {
    on_error { events += "caught:" + $1.err; }
    handlerObject(make("a"), handler);
}
sub noTryMethod(string handler) {
    on_error { events += "caught:" + $1.err; }
    shape.method(make("a"), handler);
}
sub noTryClosure(string handler) {
    on_error { events += "caught:" + $1.err; }
    shapeClosure(make("a"), handler);
}
sub report(string name) {
    printf("%s: %y\n", name, events);
    events = ();
}

# Run twice to exercise cached call sites as well as first calls.
for (int iteration = 0; iteration < 2; ++iteration) {
    try { handlerObject(make("a"), "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-Object");
    try { handlerAnyObject(make("a"), "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-AnyObject");
    try { handlerTypedHash({"key": make("a")}, "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-TypedHash");
    try { handlerHash({"key": make("a")}, "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-Hash");
    try { handlerAnyHash({"key": make("a")}, "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-AnyHash");
    try { handlerTypedList((make("a"),), "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-TypedList");
    try { handlerList((make("a"),), "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-List");
    try { handlerAnyList((make("a"),), "catch"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("catch-AnyList");
    try { handlerObject(make("a"), "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-Object");
    try { handlerAnyObject(make("a"), "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-AnyObject");
    try { handlerTypedHash({"key": make("a")}, "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-TypedHash");
    try { handlerHash({"key": make("a")}, "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-Hash");
    try { handlerAnyHash({"key": make("a")}, "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-AnyHash");
    try { handlerTypedList((make("a"),), "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-TypedList");
    try { handlerList((make("a"),), "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-List");
    try { handlerAnyList((make("a"),), "on-error"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("on-error-AnyList");
    try { handlerObject(make("a"), "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-Object");
    try { handlerAnyObject(make("a"), "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-AnyObject");
    try { handlerTypedHash({"key": make("a")}, "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-TypedHash");
    try { handlerHash({"key": make("a")}, "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-Hash");
    try { handlerAnyHash({"key": make("a")}, "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-AnyHash");
    try { handlerTypedList((make("a"),), "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-TypedList");
    try { handlerList((make("a"),), "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-List");
    try { handlerAnyList((make("a"),), "nested"); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("nested-AnyList");
    try { plainParameter(make("a")); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("plain-parameter");
    try { handlerLocal(); }
    catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
    report("handler-local");
    foreach string handler in (("catch", "on-error")) {
        try { shape.method(make("a"), handler); }
        catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
        report(handler + "-method");
        try { Shape::staticMethod(make("a"), handler); }
        catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
        report(handler + "-static-method");
        try { shapeClosure(make("a"), handler); }
        catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
        report(handler + "-closure");
        try { shapeCallRef(make("a"), handler); }
        catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
        report(handler + "-call-reference");
        try { noTryFunction(handler); }
        catch (hash<ExceptionInfo> ex) {}
        report(handler + "-no-try-function");
        try { noTryMethod(handler); }
        catch (hash<ExceptionInfo> ex) {}
        report(handler + "-no-try-method");
        try { noTryClosure(handler); }
        catch (hash<ExceptionInfo> ex) {}
        report(handler + "-no-try-closure");
    }
}
