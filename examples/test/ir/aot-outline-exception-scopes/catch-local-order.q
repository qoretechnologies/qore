# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# an outlined helper releases its locals in the reverse order of their declaration: a catch block's exception
# information before a local declared before it (qore#6650); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
class D { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "del:" + n; } }
sub thrower() { throw "B"; }
sub k3i() {
    D loc("local");
    {
        D inner("inner");
        try { throw "A", "", new D("arg"); } catch (hash<ExceptionInfo> ex) { log += "c:" + ex.err; thrower(); }
    }
}
try { k3i(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
printf("%y\n", log);
