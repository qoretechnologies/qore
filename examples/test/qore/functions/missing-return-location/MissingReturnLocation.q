#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

# Fixture for issue #7787: the RUNTIME-TYPE-ERROR of code that declares a return type that rejects NOTHING and ends
# without a return statement is located at the declaration of the code, as the AST interpreter locates it, in every
# execution mode.  Each case calls the code through a function, so that native code calls it directly (through its
# fast entry), first with a value that returns, then with one that ends without a return statement, and prints every
# location field of the exception; the lines of this file are part of the expected output
# (MissingReturnLocation.expected), in which the path of this file is <fixture>.  The cases whose error was raised by
# the fast entry of native code (qore_rt_coerce_nothing_return*(), counted by the debug library) are printed on the
# "fast entry errors:" line, which depends on the execution mode and is not part of the expected output.

# a call through a call reference, which the completion analysis cannot follow: without it, the end of the code below
# would be known to be reachable, and the program would not parse (MISSING-RETURN)
our code<nothing(string)> hook = sub (string s) {};

#! the labels of the cases whose error was raised by the fast entry of native code
our list<string> fast_entry_errors = ();

#! the number of missing return values that the fast entries of native code have rejected, or -1 if not counted
/** only the debug library counts them (dbg_get_missing_return_coercion_error_count())
*/
int sub fast_entry_error_count() {
    return Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_missing_return_coercion_error_count") : -1;
}

#! records the case if a fast entry raised its error since the count was \a before
sub record_fast_entry_error(string label, int before) {
    if (before >= 0 && fast_entry_error_count() > before) {
        push fast_entry_errors, label;
    }
}

class Tmp {
    constructor(string n) {}
}

int sub missing_tmp(string s) {
    Tmp t(s);
    if (s == "ok") {
        return 1;
    }
}
int sub call_missing_tmp(string s) {
    return missing_tmp(s);
}

int sub missing_plain(string s) {
    hook(s);
    if (s == "ok") {
        return 1;
    }
}
int sub call_missing_plain(string s) {
    return missing_plain(s);
}

int sub missing_on_exit(string s) {
    int n = 0;
    on_exit {
        ++n;
    }
    hook(s);
    if (s == "ok") {
        return 1;
    }
}
int sub call_missing_on_exit(string s) {
    return missing_on_exit(s);
}

string sub missing_string(int i) {
    hook("x");
    if (i > 0) {
        return "ok";
    }
}
string sub call_missing_string(string s) {
    return missing_string(s == "ok" ? 1 : 0);
}

class C {
    int m(string s) {
        hook(s);
        if (s == "ok") {
            return 1;
        }
    }

    int callSelf(string s) {
        return m(s);
    }

    int callPrivate(string s) {
        return p(s);
    }

    private int p(string s) {
        hook(s);
        if (s == "ok") {
            return 1;
        }
    }

    static int sm(string s) {
        hook(s);
        if (s == "ok") {
            return 1;
        }
    }
}

final class F {
    int m(string s) {
        hook(s);
        if (s == "ok") {
            return 1;
        }
    }
}

int sub call_method(C c, string s) {
    return c.m(s);
}
int sub call_final_method(F f, string s) {
    return f.m(s);
}
int sub call_static(string s) {
    return C::sm(s);
}

int sub call_local_closure(string s) {
    code c = int sub (string s0) {
        hook(s0);
        if (s0 == "ok") {
            return 1;
        }
    };
    return c(s);
}

code<int(string)> sub make_closure() {
    return int sub (string s0) {
        hook(s0);
        if (s0 == "ok") {
            return 1;
        }
    };
}
int sub call_closure(code<int(string)> c, string s) {
    return c(s);
}

# a return type that accepts NOTHING: ending without a return statement is no error
*int sub optional_return(string s) {
    hook(s);
    if (s == "ok") {
        return 1;
    }
}
auto sub call_optional_return(string s) {
    return optional_return(s);
}

# a return type that converts NOTHING: ending without a return statement returns an empty list
softlist<int> sub softlist_return(string s) {
    hook(s);
    if (s == "ok") {
        return 1;
    }
}
auto sub call_softlist_return(string s) {
    return softlist_return(s);
}

sub print_ex(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s file=%y line=%d endline=%d source=%y offset=%d lang=%y desc=%y\n", label, ex.err, ex.file, ex.line,
        ex.endline, ex.source, ex.offset, ex.lang, ex.desc);
}

sub show(string label, code c) {
    auto t;
    for (int i = 0; i < 3; ++i) {
        t = c("ok");
    }
    int before = fast_entry_error_count();
    try {
        printf("%s: ok %y, other %y\n", label, t, c("other"));
    } catch (hash<ExceptionInfo> ex) {
        print_ex(label, ex);
    }
    record_fast_entry_error(label, before);
}

# the shape of the issue's reproduction, called by top-level code
{
    int t = 0;
    for (int i = 0; i < 300; ++i) {
        t += call_missing_tmp("ok");
    }
    int before = fast_entry_error_count();
    try {
        call_missing_tmp("bad");
        printf("repro: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        print_ex("repro", ex);
    }
    record_fast_entry_error("repro", before);
}

C c();
F f();
code<int(string)> closure = make_closure();
show("local object", \call_missing_tmp());
show("plain", \call_missing_plain());
show("on_exit", \call_missing_on_exit());
show("string", \call_missing_string());
show("method", auto sub (string s) { return call_method(c, s); });
show("self method", auto sub (string s) { return c.callSelf(s); });
show("private method", auto sub (string s) { return c.callPrivate(s); });
show("final class method", auto sub (string s) { return call_final_method(f, s); });
show("static method", \call_static());
show("local closure", \call_local_closure());
show("closure", auto sub (string s) { return call_closure(closure, s); });
show("optional return", \call_optional_return());
show("softlist return", \call_softlist_return());
# a call of the function itself through a call reference runs its standard entry, whose caller raises the error
show("direct", \missing_plain());
printf("fast entry errors: %s\n", Option::HAVE_LIBRARY_DEBUGGING ? fast_entry_errors.join(",") : "not counted");
printf("done\n");
