# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the value of a return that a handler's exception interrupts is released after the locals of the function when
# the exception leaves it, also when the return is in a region (qore#6650); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
T sub mk(string n) { return new T(n); }
auto sub a3() {
    int n = 0;
    T z("a3-z");
    while (n < 5) {
        ++n;
        if (n == 3) {
            on_exit { log += "x"; if (True) { throw "A3"; } }
            return mk("a3-t");
        }
    }
    return n;
}
try { a3(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
printf("%y\n", log);
