# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a region does not end in a scope with handlers: the helper would run them when it returns, before the code of
# the scope after the region (qore#6652); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
string sub tag(string s) { return s; }
int sub g1() {
    T z("g1-z");
    if (True) {
        on_exit { string s = tag("ox"); log += s; throw "G1-OUTER"; }
        T a("g1-a");
        if (True) {
            on_exit { log += tag("ix"); throw "G1-INNER"; }
            return 1;
        }
    }
}
try { g1(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
printf("%y\n", log);
