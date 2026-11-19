# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a handler that runs in an outlined helper and assigns a local of the function releases the old value when it
# assigns it: the coordinator keeps no reference to it across the call (qore#6652); see
# AOTOutlineExceptionScopes.qtest

%modern
our list<string> events = ();
class Item {
    public { string name; }
    constructor(string name) { self.name = name; }
    destructor() { events += "~" + name; }
}
Item sub make(string name) { return new Item(name); }
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
}
Shape shape();
foreach string h in (("catch", "on-error")) {
try { shape.method(make("a"), h); } catch (hash<ExceptionInfo> ex) { events += "caught:" + ex.err; }
printf("%y\n", events); events = ();
}
