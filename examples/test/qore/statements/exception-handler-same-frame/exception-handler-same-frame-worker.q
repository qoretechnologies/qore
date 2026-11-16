#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of exception-handler-same-frame.qtest: each case raises an exception in a try block whose handler is in
# the same function, from an instruction that must branch to that handler; the output must be the same in every
# execution mode as with the AST interpreter.  Each round calls every case again, so that the tiered and JIT modes run
# them as native code too.

%modern

#! an object whose destructor raises an exception
class DtorThrow {
    destructor() {
        throw "DTOR-ERR";
    }
}

class C {
    static s() {
    }
}

auto sub any(auto v) {
    return v;
}

sub bg_f(int i) {
}

#! a thread that cannot be started: a method call on a local
string sub bg_dot(Queue q) {
    try {
        on_exit printf("  on_exit\n");
        background q.push(1);
        return "started";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a thread that cannot be started: an argument evaluated by the background helper
string sub bg_helper(Queue q) {
    try {
        on_exit printf("  on_exit\n");
        background bg_f(q.size() + 1);
        return "started";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a thread that cannot be started: a function call
string sub bg_func() {
    try {
        on_exit printf("  on_exit\n");
        background bg_f(1);
        return "started";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a thread that cannot be started: a static method call
string sub bg_static() {
    try {
        on_exit printf("  on_exit\n");
        background C::s();
        return "started";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a regex case whose match exceeds the match limit
string sub switch_regex(string s) {
    try {
        on_exit printf("  on_exit\n");
        switch (s) {
            case /^(a|a)+$/:
                return "matched";
        }
        return "no match";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a case whose comparison cannot convert the encoding of the value
string sub switch_case(string s) {
    try {
        on_exit printf("  on_exit\n");
        switch (s) {
            case c'a':
                return "matched";
        }
        return "no match";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a compound assignment to an element of a fixed list, which the optimizer replaces with a scalar
int sub div_int(int z) {
    list<int> l = (10, 2);
    try {
        l[0] /= z;
    } catch (hash<ExceptionInfo> ex) {
        printf("  caught %s\n", ex.err);
        return -1;
    }
    return l[0] + l[1];
}

#! as div_int() with floats
float sub div_float(float z) {
    list<float> l = (1.0, 2.0);
    try {
        l[0] /= z;
    } catch (hash<ExceptionInfo> ex) {
        printf("  caught %s\n", ex.err);
        return -1;
    }
    return l[0] + l[1];
}

#! a hash literal with constant keys and a value of the wrong type for its declared type
string sub hash_const_keys(auto v) {
    hash<string, int> h;
    try {
        on_exit printf("  on_exit\n");
        h = {"a": v};
        return sprintf("%y", h);
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a hash literal with keys evaluated at runtime and a value of the wrong type for its declared type
string sub hash_keys(string k, auto v) {
    hash<string, int> h;
    try {
        on_exit printf("  on_exit\n");
        h = {k: v};
        return sprintf("%y", h);
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a list literal with a value of the wrong type for its declared type
string sub typed_list(auto v) {
    list<int> l;
    try {
        on_exit printf("  on_exit\n");
        l = (1, v);
        return sprintf("%y", l);
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! the destructor of a temporary raises when the statement ends
string sub discard_temps() {
    try {
        on_exit printf("  on_exit\n");
        new DtorThrow();
        return "not raised";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! the destructor of a temporary of an if condition raises when the condition has been evaluated
string sub discard_temps_if() {
    try {
        on_exit printf("  on_exit\n");
        if (any(new DtorThrow())) {
            printf("  if body\n");
        }
        return "not raised";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! as discard_temps() without an on_exit handler
string sub discard_temps_plain() {
    try {
        new DtorThrow();
        return "not raised";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! as discard_temps_if() without an on_exit handler
string sub discard_temps_if_plain() {
    string rv = "not raised";
    try {
        if (any(new DtorThrow())) {
        }
    } catch (hash<ExceptionInfo> ex) {
        rv = "caught " + ex.err;
    }
    return rv;
}

#! the handler is in a loop over a temporary list, which must survive the exception
string sub discard_temps_loop() {
    list<string> rv = ();
    foreach int i in (any((1, 2))) {
        try {
            new DtorThrow();
        } catch (hash<ExceptionInfo> ex) {
            rv += sprintf("%d:%s", i, ex.err);
        }
    }
    return rv.join(" ");
}

#! no handler in the function: the exception leaves it, and the statements after it do not run
string sub discard_temps_out() {
    on_exit printf("  on_exit\n");
    new DtorThrow();
    printf("  not reached\n");
    return "not raised";
}

#! runs a case; an exception that leaves the case is reported as escaped
sub run(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: escaped %s\n", label, ex.err);
    }
}

int size = get_default_thread_stack_size();
string invalid = binary_to_string(<ff>, "ISO-2022-JP");
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    if (HAVE_MANAGE_STACK) {
        Queue q();
        # no stack this large can be allocated, so no thread can be created
        set_default_thread_stack_size(1 << 50);
        on_exit set_default_thread_stack_size(size);
        run("background method", auto sub () { return bg_dot(q); });
        run("background helper", auto sub () { return bg_helper(q); });
        run("background function", auto sub () { return bg_func(); });
        run("background static method", auto sub () { return bg_static(); });
    }
    run("switch regex", auto sub () { return switch_regex(strmul("a", 40) + "b"); });
    run("switch case", auto sub () { return switch_case(invalid); });
    run("div int", auto sub () { return div_int(0); });
    run("div int ok", auto sub () { return div_int(2); });
    run("div float", auto sub () { return div_float(0.0); });
    run("div float ok", auto sub () { return div_float(2.0); });
    run("hash const keys", auto sub () { return hash_const_keys("x"); });
    run("hash const keys ok", auto sub () { return hash_const_keys(1); });
    run("hash keys", auto sub () { return hash_keys("a", "x"); });
    run("hash keys ok", auto sub () { return hash_keys("a", 1); });
    run("typed list", auto sub () { return typed_list("x"); });
    run("discard temps", \discard_temps());
    run("discard temps if", \discard_temps_if());
    run("discard temps plain", \discard_temps_plain());
    run("discard temps if plain", \discard_temps_if_plain());
    run("discard temps loop", \discard_temps_loop());
    run("discard temps out", \discard_temps_out());
}
