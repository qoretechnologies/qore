# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The value of a return is released at once when the release of the return statement's temporaries raises an
# exception (#5995); see return-temp-raises-value-release.qtest

%modern
%enable-all-warnings
%no-child-restrictions

our list<string> log = ();
our int live = 0;

class T {
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
        ++live;
    }
    destructor() {
        log += "~" + n;
        --live;
    }
}

# an object whose destructor raises an exception
class TT inherits T {
    constructor(string n) : T(n) {
    }
    destructor() {
        log += "dthrow";
        throw "DT";
    }
}

auto sub first(auto a, auto b) {
    return a;
}

auto sub second(auto a, auto b) {
    return b;
}

# the raising temporary is the last argument
T sub r1() {
    T z("r1-z");
    if (True) {
        T q("r1-q");
        return first(new T("r1-v"), new TT("r1-tt"));
    }
}

# the raising temporary is the first argument
T sub r2() {
    T z("r2-z");
    if (True) {
        T q("r2-q");
        return second(new TT("r2-tt"), new T("r2-v"));
    }
}

# a local is returned
T sub r3() {
    T z("r3-z");
    T q("r3-q");
    return first(q, new TT("r3-tt"));
}

# a new object is returned, whose argument raises before it is created
T sub r4() {
    T z("r4-z");
    {
        T q("r4-q");
        return new T("r4-v" + (first("", new TT("r4-tt")) ?? ""));
    }
}

# in a try block whose catch block continues
T sub r5() {
    T z("r5-z");
    try {
        T q("r5-q");
        return first(new T("r5-v"), new TT("r5-tt"));
    } catch (hash<ExceptionInfo> ex) {
        log += "catch:" + ex.err;
    }
    return new T("r5-after");
}

# in nested blocks
T sub r6() {
    T z("r6-z");
    {
        T a("r6-a");
        {
            T b("r6-b");
            if (True) {
                T q("r6-q");
                return first(new T("r6-v"), new TT("r6-tt"));
            }
        }
    }
}

# with handlers on the block
T sub r7() {
    T z("r7-z");
    on_exit log += "r7-exit";
    on_error log += "r7-error";
    {
        T q("r7-q");
        return first(new T("r7-v"), new TT("r7-tt"));
    }
}

class C {
    T m() {
        T z("m-z");
        {
            T q("m-q");
            return first(new T("m-v"), new TT("m-tt"));
        }
    }
}

# typed parameters
T sub r9(string s) {
    T z("r9-z" + s);
    {
        T q("r9-q");
        return first(new T("r9-v"), new TT("r9-tt"));
    }
}

# no locals
auto sub r10() {
    return first(new T("r10-v"), new TT("r10-tt"));
}

# the destructor of a local raises after the return: the value is released after the locals
T sub r11() {
    T z("r11-z");
    TT d("r11-d");
    return first(new T("r11-v"), new T("r11-t"));
}

sub run(string name, code c) {
    log = ();
    try {
        auto r = c();
        log += "got";
    } catch (hash<ExceptionInfo> ex) {
        log += "caught:" + ex.err;
    }
    printf("%s: %y live=%d\n", name, log, live);
}

code closure = T sub () {
    T z("cl-z");
    {
        T q("cl-q");
        return first(new T("cl-v"), new TT("cl-tt"));
    }
};

# several times, so that tiered code runs each function as native code
for (int i = 0; i < 3; ++i) {
    run("r1", \r1());
    run("r2", \r2());
    run("r3", \r3());
    run("r4", \r4());
    run("r5", \r5());
    run("r6", \r6());
    run("r7", \r7());
    run("method", auto sub () { C c(); return c.m(); });
    run("r9", auto sub () { return r9("x"); });
    run("r10", \r10());
    run("r11", \r11());
    run("closure", closure);
}
