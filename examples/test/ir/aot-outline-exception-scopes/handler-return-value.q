# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the value of a handler's return while an exception leaves an outlined helper with no return of a value of its
# own is released after the locals of the function, as the function releases it (qore#6650); see
# AOTOutlineExceptionScopes.qtest
# smaller regions end in the scope of the handler, which qore#6652 fixes
# outline-targets: 8 16 32

%modern
our list<string> log = ();
class T { private { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
T sub mk(string n) { return new T(n); }
auto sub g2() {
    T z("g2-z");
    int n = 0;
    while (n < 4) {
        ++n;
        if (n == 2) {
            on_exit { log += "h"; return mk("g2-h"); }
            throw "G2";
        }
    }
    return "end";
}
try { g2(); } catch (hash<ExceptionInfo> ex) { log += "caught:" + ex.err; }
printf("%y\n", log);
