#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

# a temporary whose destruction is logged, to show when each statement releases its temporaries
class T {
    private {
        string name;
    }
    constructor(string name) {
        self.name = name;
    }
    destructor() {
        printf("  release %s\n", name);
    }
    string get() {
        return name;
    }
}

string sub show_stack(*list<auto> cs) {
    list<string> l = ();
    foreach hash<auto> h in (cs) {
        if (h.type != "user") {
            continue;
        }
        push l, sprintf("%s:%d", h.function, h.line);
    }
    return l.join(" ");
}

sub show(string label, hash<ExceptionInfo> ex) {
    printf("  %s: %s/%s/%y %s:%d\n    [%s]\n", label, ex.err, ex.desc, ex.arg, basename(ex.file ?? ""), ex.line,
        show_stack(ex.callstack));
    if (ex.next) {
        show(label + ".next", ex.next);
    }
}

sub thrower(string err, auto arg) {
    throw err, "thrown", arg;
}

string sub raise_in_temp(T t) {
    thrower("TEMP", t.get());
    return t.get();
}

sub simple() {
    try {
        printf("  %s\n", raise_in_temp(new T("a")));
    } catch (hash<ExceptionInfo> ex) {
        show("caught", ex);
    }
}

sub rethrow_plain() {
    try {
        try {
            thrower("INNER", 1);
        } catch (hash<ExceptionInfo> ex) {
            show("inner", ex);
            rethrow;
        }
    } catch (hash<ExceptionInfo> ex) {
        show("outer", ex);
    }
}

sub rethrow_args() {
    try {
        try {
            thrower("INNER", 2);
        } catch (hash<ExceptionInfo> ex) {
            rethrow "REPLACED", "new desc", 3;
        }
    } catch (hash<ExceptionInfo> ex) {
        show("outer", ex);
    }
}

sub nested_new_exception() {
    try {
        try {
            thrower("FIRST", 4);
        } catch (hash<ExceptionInfo> ex) {
            show("inner", ex);
            thrower("SECOND", ex.err);
        }
    } catch (hash<ExceptionInfo> ex) {
        show("outer", ex);
    }
}

sub nested_fallthrough() {
    try {
        try {
            thrower("INNER", 5);
        } catch (hash<ExceptionInfo> ex) {
            show("inner", ex);
        }
        printf("  after inner\n");
        thrower("OUTER", 6);
    } catch (hash<ExceptionInfo> ex) {
        show("outer", ex);
    }
}

sub handlers_in_try() {
    try {
        on_exit printf("  on_exit (try block)\n");
        on_error printf("  on_error (try block)\n");
        on_success printf("  on_success (try block)\n");
        {
            on_exit printf("  on_exit (nested block)\n");
            on_error printf("  on_error (nested block)\n");
            printf("  %s\n", raise_in_temp(new T("b")));
        }
    } catch (hash<ExceptionInfo> ex) {
        on_exit printf("  on_exit (catch block)\n");
        show("caught", ex);
    }
}

sub handler_rethrow_in_try() {
    try {
        try {
            on_error printf("  on_error before rethrow\n");
            thrower("H", 7);
        } catch (hash<ExceptionInfo> ex) {
            on_exit printf("  on_exit in catch before rethrow\n");
            rethrow;
        }
    } catch (hash<ExceptionInfo> ex) {
        show("outer", ex);
    }
}

sub handler_raises_in_try() {
    try {
        on_exit thrower("FROM_HANDLER", 8);
        printf("  try body\n");
    } catch (hash<ExceptionInfo> ex) {
        show("caught", ex);
    }
}

int sub return_from_try() {
    try {
        on_exit printf("  on_exit (return from try)\n");
        return 9;
    } catch (hash<ExceptionInfo> ex) {
        show("caught", ex);
    }
    return -1;
}

int sub return_from_catch() {
    try {
        thrower("R", 10);
    } catch (hash<ExceptionInfo> ex) {
        on_exit printf("  on_exit (return from catch)\n");
        return ex.arg + 1;
    }
    return -1;
}

sub loop_jumps() {
    for (int i = 0; i < 4; ++i) {
        try {
            if (i == 1) {
                continue;
            }
            if (i == 3) {
                break;
            }
            thrower("LOOP", i);
        } catch (hash<ExceptionInfo> ex) {
            printf("  loop %d: %s %y\n", i, ex.err, ex.arg);
            if (i == 2) {
                continue;
            }
        }
        printf("  loop %d end\n", i);
    }
}

sub catch_raises() {
    try {
        try {
            thrower("ONE", 11);
        } catch (hash<ExceptionInfo> ex) {
            T t("c");
            printf("  catch with %s\n", t.get());
            thrower("TWO", ex.arg);
        }
    } catch (hash<ExceptionInfo> ex) {
        show("outer", ex);
    }
}

sub uncaught_through_frames() {
    try {
        rethrow_from_function();
    } catch (hash<ExceptionInfo> ex) {
        show("caught", ex);
    }
}

sub rethrow_from_function() {
    try {
        thrower("DEEP", 12);
    } catch (hash<ExceptionInfo> ex) {
        rethrow;
    }
}

sub runtime_error() {
    try {
        hash<auto> h = {};
        int x = h.a.b + 1;
        printf("  %d\n", x);
        list<int> l = (1,);
        int z = l.size() - 1;
        printf("  %y\n", l[0] / z);
    } catch (hash<ExceptionInfo> ex) {
        show("caught", ex);
    }
}

string sub join_all(string a, string b, string c, string d, string e, string f) {
    return a + b + c + d + e + f;
}

# the temporaries of a statement with many of them are released in the same order when it ends normally and when it
# raises
sub many_temps(bool raise) {
    try {
        printf("  %s\n", join_all(new T("t1").get(), new T("t2").get(), new T("t3").get(), new T("t4").get(),
            raise ? raise_in_temp(new T("t5")) : new T("t5").get(), new T("t6").get()));
        printf("  %s\n", join_all(new T("u1").get(), new T("u2").get(), new T("u3").get(), new T("u4").get(),
            new T("u5").get(), new T("u6").get()));
    } catch (hash<ExceptionInfo> ex) {
        printf("  %s\n", join_all(new T("c1").get(), new T("c2").get(), new T("c3").get(), new T("c4").get(),
            new T("c5").get(), ex.err));
        show("caught", ex);
    }
}

sub run(string name, code c) {
    printf("%s:\n", name);
    auto v = c();
    if (exists v) {
        printf("  returned %y\n", v);
    }
}

run("simple", \simple());
run("rethrow", \rethrow_plain());
run("rethrow with arguments", \rethrow_args());
run("new exception in catch", \nested_new_exception());
run("nested fallthrough", \nested_fallthrough());
run("handlers in try", \handlers_in_try());
run("handler and rethrow", \handler_rethrow_in_try());
run("handler raises", \handler_raises_in_try());
run("return from try", \return_from_try());
run("return from catch", \return_from_catch());
run("loop jumps", \loop_jumps());
run("catch raises", \catch_raises());
run("rethrow through frames", \uncaught_through_frames());
run("runtime error", \runtime_error());
run("many temporaries", sub () { many_temps(False); });
run("many temporaries raising", sub () { many_temps(True); });
