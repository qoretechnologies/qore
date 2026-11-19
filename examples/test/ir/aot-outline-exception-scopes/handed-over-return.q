# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# handlers run out of line past the inline cap in the block cleanups of a return hand the values of their returns over
# to the return (#6137) in functions outlined at low thresholds: the ScopeExit that hands a value over and the
# instructions that take or release it stay in one function, whose cell holds the value, and the value that a helper
# holds when an exception leaves it is released after the coordinator's locals (qore#6650); see
# AOTOutlineExceptionScopes.qtest

%modern
our list<string> log = ();
our int cnt;
class T { public { string n; } constructor(string n) { self.n = n; } destructor() { log += "~" + n; } }
T sub mk(string n) { return new T(n); }
string sub show(auto v) { return v.typeCode() == NT_OBJECT ? "T:" + v.n : sprintf("%y", v); }

# a handler past the cap replaces the value of the return
auto sub replaced(int x) {
    T z("replaced-z");
    int y = x + 1;
    if (y > 100) {
        log += "big";
    }
    {
        on_exit { log += "replaced-o"; }
        {
            T w("replaced-w");
            on_exit { return mk("replaced-h" + x); }
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
            return mk("replaced-ret");
        }
    }
}

# a try statement around the scope: a handler past the cap throws after another one handed a value over
auto sub caught(int x) {
    T z("caught-z");
    try {
        int y = x + 1;
        if (y > 100) {
            log += "big";
        }
        {
            on_exit { return mk("caught-h1"); }
            on_exit { if (x == 1) throw "CAUGHT"; return mk("caught-h2"); }
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
            return mk("caught-ret");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "caught " + ex.err;
    }
    return "caught-after";
}

# nested handlers: the handler past the cap has a handler that returns
auto sub nested(int x) {
    T z("nested-z");
    int y = x + 1;
    if (y > 100) {
        log += "big";
    }
    {
        on_exit { on_exit { return mk("nested-inner"); } if (x == 1) return mk("nested-outer"); }
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

# a return without a value takes the value that a handler past the cap hands over
auto sub novalue(int x) {
    T z("novalue-z");
    {
        on_exit { log += "novalue-o"; }
        int y = x + 1;
        if (y > 100) {
            log += "big";
        }
        {
            T w("novalue-w");
            on_exit { if (x >= 0) return mk("novalue-h" + x); }
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

sub run(string name, code c) {
    log = ();
    try {
        auto v = c();
        log += "->" + show(v);
        remove v;
    } catch (hash<ExceptionInfo> ex) {
        log += "ex " + ex.err;
    }
    printf("%s: %s\n", name, log.join(","));
}

foreach int x in (0, 1) {
    run("replaced" + x, auto sub () { return replaced(x); });
    run("caught" + x, auto sub () { return caught(x); });
    run("nested" + x, auto sub () { return nested(x); });
    run("novalue" + x, auto sub () { return novalue(x); });
}
printf("cnt: %d\n", cnt);
