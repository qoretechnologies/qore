# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# a region whose closure captures locals of the region and of the coordinator (qore#6654) in a function whose return
# takes a value that a handler past the inline cap hands over (#6137): the ScopeExit that hands the value over and
# the instruction that takes it stay in one function (qore#6650); see AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
our int cnt;
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
T sub mk(string n) { return new T(n); }
string sub show(auto v) { return v.typeCode() == NT_OBJECT ? "T:" + v.n : sprintf("%y", v); }

# closures created in a loop capture a local of the loop and a local of the function, and a return without a value
# takes the value that a handler past the inline cap hands over
auto sub f(int x) {
    T z("f-z");
    int m = x * 10;
    list<code> l = ();
    for (int round = 0; round < 2; ++round) {
        int d = round;
        for (int i = 0; i < 2; ++i) {
            l += int sub () { return m + d; };
        }
    }
    log += sprintf("%y", (map $1(), l));
    {
        on_exit { log += "f-o"; }
        int y = x + 1;
        if (y > 100) {
            log += "big";
        }
        {
            T w("f-w");
            on_exit { if (x >= 0) return mk("f-h" + x + ":" + (foldl $1 + $2, (map $1(), l))); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return;
        }
    }
}

foreach int x in (0, 1) {
    log = ();
    try {
        auto v = f(x);
        log += "->" + show(v);
        remove v;
    } catch (hash<ExceptionInfo> ex) {
        log += "ex " + ex.err;
    }
    printf("f%d: %s\n", x, log.join(","));
}
printf("cnt: %d\n", cnt);
