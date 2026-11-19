#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The cases of handler-return-past-cap-exception.qtest (#6762): a return whose cleanups run handlers out of line past
# the inline cap of the lowering, and an exception that a handler of an enclosing block raises before the return ends.
# The value of the return is the value of the return that the exception interrupts: the AST interpreter keeps it until
# the function ends, after its locals, or releases it at the try statement that catches the exception.  Each case logs
# the destruction of its objects; the output is the same in every execution mode and compiled with qcc.

%modern

our list<string> log = ();
our int cnt;

class T {
    private {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        log += "~" + n;
    }
}

T sub mk(string n) {
    return new T(n);
}

# a handler's return, lowered inline, and 62 handlers that run before it in the block of a return; the enclosing
# block's handler raises
auto sub inl62(bool x) {
    T z("z");
    {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    }
}

# as inl62(), with a try statement of the function that catches the exception
auto sub tinl62(bool x) {
    T z("z");
    try {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "caught-in " + ex.err;
    }
    return mk("after");
}

# a handler's return, lowered inline, and 63 handlers that run before it in the block of a return; the enclosing
# block's handler raises
auto sub inl63(bool x) {
    T z("z");
    {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    }
}

# as inl63(), with a try statement of the function that catches the exception
auto sub tinl63(bool x) {
    T z("z");
    try {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "caught-in " + ex.err;
    }
    return mk("after");
}

# a handler's return, lowered inline, and 80 handlers that run before it in the block of a return; the enclosing
# block's handler raises
auto sub inl80(bool x) {
    T z("z");
    {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    }
}

# as inl80(), with a try statement of the function that catches the exception
auto sub tinl80(bool x) {
    T z("z");
    try {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    } catch (hash<ExceptionInfo> ex) {
        log += "caught-in " + ex.err;
    }
    return mk("after");
}

# the value of a return with handlers past the cap that return nothing; the enclosing block's handler raises
auto sub plain(bool x) {
    T z("z");
    {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    }
}

# a handler's return with 70 handlers that run before it and 10 that run after it in the block of a return; the
# enclosing block's handler raises
auto sub ool(bool x) {
    T z("z");
    {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            return mk("ret");
        }
    }
}

# the end of a block with a handler's return and 80 other handlers; the enclosing block's handler raises
auto sub blk(bool x) {
    T z("z");
    {
        on_exit { if (x) throw "E"; }
        {
            T w("w");
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit { return mk("handed"); }
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
            on_exit ++cnt;
        }
        log += "after-block";
    }
    return mk("end");
}

sub run(string name, code c) {
    foreach bool x in ((False, True)) {
        log = ();
        string res;
        try {
            auto v = c(x);
            res = v instanceof T ? "T" : sprintf("%y", v);
            remove v;
        } catch (hash<ExceptionInfo> ex) {
            res = "ex:" + ex.err;
        }
        printf("%s(%y): %s %y\n", name, x, res, log);
    }
}

run("inl62", \inl62());
run("tinl62", \tinl62());
run("inl63", \inl63());
run("tinl63", \tinl63());
run("inl80", \inl80());
run("tinl80", \tinl80());
run("plain", \plain());
run("ool", \ool());
run("blk", \blk());
