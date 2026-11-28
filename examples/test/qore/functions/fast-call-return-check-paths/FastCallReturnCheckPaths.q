#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# calls whose callee returns a value that fails its declared return type, through each of the runtime's fast calls of
# compiled code (a closure, a self-recursive call, a method, and a direct call between the functions of a qcc
# executable): the exception is raised with the callee on the call stack, and before a parameter whose destructor
# raises is released, as the AST interpreter raises it in the callee's frame (qore#7762); prints each exception and
# the call stack of the user code

%modern

#! an object whose destructor raises an exception
class Tmp {
    private {
        string n;
        bool raise;
    }

    constructor(string n, bool raise) {
        self.n = n;
        self.raise = raise;
    }

    destructor() {
        if (raise) {
            throw "DESTRUCTOR-ERROR", n;
        }
    }

    string name() {
        return n;
    }
}

#! methods whose return values fail their declared types
class C {
    int checked(string s) {
        auto v = s == "ok" ? 1 : s;
        return v;
    }

    int given(Tmp t) {
        auto v = t.name() == "ok" ? 1 : t.name();
        return v;
    }

    static int staticChecked(string s) {
        auto v = s == "ok" ? 1 : s;
        return v;
    }

    static int staticGiven(Tmp t) {
        auto v = t.name() == "ok" ? 1 : t.name();
        return v;
    }
}

# a direct call between the functions of the program (qcc's direct AOT call)
int sub direct_checked(string s) {
    auto v = s == "ok" ? 1 : s;
    return v;
}

int sub call_direct(string s) {
    return direct_checked(s);
}

# a function without a return statement for the argument "bad", with a local whose destructor raises an exception:
# the missing return is checked after the body has ended and its locals have been released, as in the AST
# interpreter, so only the destructor's exception is raised (qore#7762)
int sub missing(string s, bool raise) {
    Tmp t(s, raise);
    if (s == "ok") {
        return 1;
    }
}

int sub call_missing(string s, bool raise) {
    return missing(s, raise);
}

int sub direct_given(Tmp t) {
    auto v = t.name() == "ok" ? 1 : t.name();
    return v;
}

# the argument is a temporary, whose reference the call gives to the callee: the return value is checked before the
# parameter, whose destructor raises, is released
int sub call_direct_given(string s, bool raise) {
    return direct_given(new Tmp(s, raise));
}

# a self-recursive function: the value of the innermost call fails its type
int sub recursive(string s, int n) {
    if (n) {
        return recursive(s, n - 1);
    }
    auto v = s == "ok" ? 1 : s;
    return v;
}

int sub recursive_given(Tmp t, int n, bool raise) {
    if (n) {
        return recursive_given(new Tmp(t.name(), raise), n - 1, raise);
    }
    auto v = t.name() == "ok" ? 1 : t.name();
    return v;
}

int sub call_recursive_given(string s, bool raise) {
    return recursive_given(new Tmp(s, False), 1, raise);
}

# a closure
code closure = int sub (string s) {
    auto v = s == "ok" ? 1 : s;
    return v;
};

code closure_given = int sub (Tmp t) {
    auto v = t.name() == "ok" ? 1 : t.name();
    return v;
};

int sub call_closure(string s) {
    return closure(s);
}

int sub call_closure_given(string s, bool raise) {
    return closure_given(new Tmp(s, raise));
}

# a method
C c();

int sub call_method(string s) {
    return c.checked(s);
}

int sub call_method_given(string s, bool raise) {
    return c.given(new Tmp(s, raise));
}

# a static method
int sub call_static(string s) {
    return C::staticChecked(s);
}

int sub call_static_given(string s, bool raise) {
    return C::staticGiven(new Tmp(s, raise));
}

string sub errs(hash<ExceptionInfo> ex) {
    list<string> l = ();
    *hash<ExceptionInfo> e = ex;
    while (e) {
        push l, e.err;
        e = e.next;
    }
    return l.join(",");
}

string sub stack_of(hash<ExceptionInfo> ex) {
    return (map sprintf("%s:%d", $1.function, $1.line), ex.callstack, $1.type == "user").join(" <- ");
}

int t = 0;
for (int i = 0; i < 300; ++i) {
    t += call_direct("ok") + call_direct_given("ok", False) + recursive("ok", 2) + call_recursive_given("ok", False)
        + call_closure("ok") + call_closure_given("ok", False) + call_method("ok") + call_method_given("ok", False)
        + call_static("ok") + call_static_given("ok", False) + call_missing("ok", False);
}
printf("warm-up %d\n", t);
foreach string name in (("direct", "direct given", "recursive", "recursive given", "closure", "closure given",
        "method", "method given", "static", "static given", "missing destructor", "missing")) {
    try {
        switch (name) {
            case "direct": call_direct("bad"); break;
            case "direct given": call_direct_given("bad", True); break;
            case "recursive": recursive("bad", 2); break;
            case "recursive given": call_recursive_given("bad", True); break;
            case "closure": call_closure("bad"); break;
            case "closure given": call_closure_given("bad", True); break;
            case "method": call_method("bad"); break;
            case "method given": call_method_given("bad", True); break;
            case "static": call_static("bad"); break;
            case "static given": call_static_given("bad", True); break;
            case "missing destructor": call_missing("bad", True); break;
            # without the destructor's exception, the missing return is raised at the function's declaration, as
            # in the AST interpreter (qore#7787)
            case "missing": call_missing("bad", False); break;
        }
        printf("%s: no exception\n", name);
    } catch (hash<ExceptionInfo> ex) {
        # the location of the exception is that of the return statement in the callee, as in the AST interpreter
        # (qore#7762)
        printf("%s: %s: %s; at: %s:%d; stack: %s; exceptions: %s\n", name, ex.err, ex.desc, basename(ex.file),
            ex.line, stack_of(ex), errs(ex));
    }
}
