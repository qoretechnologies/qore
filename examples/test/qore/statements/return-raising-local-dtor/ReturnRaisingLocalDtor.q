# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The value of a return is released when the destructor of a local raises an exception after it (#6029); see
# return-raising-local-dtor.qtest

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

# a local whose destructor raises an exception
class D {
    private {
        string n;
    }
    constructor(string n) {
        self.n = n;
    }
    destructor() {
        log += "~" + n;
        throw "DX";
    }
}

T sub mk(string n) {
    return new T(n);
}

# the value of a call, in a block with a handler
auto sub d1() {
    T z("d1-z");
    if (True) {
        on_exit {
            log += "x";
        }
        D dd("d1-d");
        return mk("d1-t");
    }
}

# a local of the block
auto sub d2() {
    T z("d2-z");
    if (True) {
        D dd("d2-d");
        T t("d2-t");
        return t;
    }
}

# a method
class C {
    auto get() {
        D dd("d3-d");
        return mk("d3-t");
    }
}

auto sub d3() {
    C c();
    return c.get();
}

# a function with typed parameters, called with arguments of those types
T sub d4(string n, int i) {
    D dd("d4-d");
    return mk(n + i);
}

# a generic function, which runs as the specialized variant for its type argument
Y sub d5<Y>(Y y) {
    D dd("d5-d");
    return y;
}

# the call of a function from code that the IR interpreter does not call directly (call_function()), whose frame returns its value through the function's variant (UserVariantBase::evalTiered())
auto sub call(string fn) {
    if (fn == "d3") {
        return d3();
    }
    if (fn == "d4") {
        return call_function(\d4(), "d4-t", 1);
    }
    if (fn == "d5") {
        return d5<T>(mk("d5-t"));
    }
    return call_function(fn);
}

# each function runs several times, so that it runs as native code in the jit and tiered modes and is specialized
foreach string fn in (("d1", "d2", "d3", "d4", "d5")) {
    for (int i = 0; i < 3; ++i) {
        log = ();
        try {
            call(fn);
        } catch (hash<ExceptionInfo> ex) {
            log += "caught:" + ex.err;
        }
        printf("%s: %y live=%d\n", fn, log, live);
    }
}


# the top level of a program returns while the destructor of one of its locals raises an exception
const TopSrc = "%modern
our list<string> log = ();
our int live = 0;
class T { private { string n; } constructor(string n) { self.n = n; ++live; } destructor() { log += \"~\" + n; --live; } }
class D { private { string n; } constructor(string n) { self.n = n; } destructor() { log += \"~\" + n; throw \"DX\"; } }
T sub mk(string n) { return new T(n); }
{ T z(\"top-z\"); D dd(\"top-d\"); return mk(\"top-t\"); }
";
for (int i = 0; i < 3; ++i) {
    Program p(PO_NEW_STYLE);
    p.parse(TopSrc, "top");
    list<string> top_log;
    try {
        p.run();
    } catch (hash<ExceptionInfo> ex) {
        top_log += "caught:" + ex.err;
    }
    printf("top: %y live=%d\n", p.getGlobalVariable("log") + top_log, p.getGlobalVariable("live"));
}
