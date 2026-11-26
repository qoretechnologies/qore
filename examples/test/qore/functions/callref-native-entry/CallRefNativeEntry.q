#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Calls through references to functions with call-reference entries (qore#5578): in a program compiled with qcc, a
# call through a reference whose target is not known at the call calls the call-reference entry of the function that
# the reference has (qore_rt_call_ref_<r>_<a>_v1(), qore_rt_call_ref_boxed_v1()) when the reference and the call allow
# it, and the generic call path otherwise; the result, exceptions and call stacks of every case must be those of the
# AST interpreter.  The cases use typed and untyped references with native int, float and bool arguments and results
# (also values that need a node: wide ints and floats that collide with a value tag), NaN-boxed arguments and results,
# results consumed by native locals and compound assignments, exceptions, wrong types and arities, named arguments,
# references made by the runtime (get_call_reference()), references in containers, recursion through a reference,
# and the first calls of many references made by many threads at once.
#
# Prints a line for each case.  With the argument "report" (debug builds), it prints after each case whether its calls
# took a call-reference entry ("native", dbg_function_reference_native_calls()), the runtime's fast call through a
# reference ("fast", dbg_function_reference_fast_calls()) or neither ("generic") instead.

%modern
%no-child-restrictions

const N = 200;

const UserFunctions = {
    "thrower": True, "deep": True, "loop_int": True, "catch_loop": True, "fib_ref": True, "call1": True,
};

bool report = ARGV[0] == "report";

int sub step(int value) {
    return (value * 33 + 17) & 65535;
}

int sub wide(int value) {
    return value + 1;
}

float sub fstep(float value) {
    return 65535.0 - value;
}

float sub tagged(float value) {
    return -9.0e307 - value;
}

bool sub negb(bool b) {
    return !b;
}

float sub i2f(int value) {
    return value / 2.0;
}

int sub f2i(float value) {
    return int(value) * 3;
}

int sub izero() {
    return 42;
}

float sub fzero() {
    return -3.0e307;
}

bool sub bzero() {
    return True;
}

string sub greet(string s) {
    return "hi " + s;
}

int sub mixed(int a, float b) {
    return a + int(b * 10.0);
}

int sub three(int a, int b, int c) {
    return a * 100 + b * 10 + c;
}

string sub boxed_mixed(string s, int n, bool b) {
    return sprintf("%s:%d:%y", s, n, b);
}

number sub num_value(int x) {
    return x;
}

*int sub opt_value(int x) {
    return x;
}

# stores of the value of a call through a reference into native locals, outside a try block
int sub store_int(code f, int x) {
    int i = f(x);
    return i;
}

float sub store_float(code f, int x) {
    float d = f(x);
    return d;
}

bool sub store_bool(code f, int x) {
    bool b = f(x);
    return b;
}

auto sub auto_value(int x) {
    switch (x) {
        case 0: return 3.7;
        case 1: return 1;
        case 2: return "7";
        case 3: return True;
        case 4: return NOTHING;
    }
    return 2.5;
}

auto sub auto_return(int x) {
    return x > 5 ? "big" : x;
}

int sub bad_return(int x) {
    auto v = x > 5 ? "big" : x;
    return v;
}

int sub thrower(int x) {
    throw "CALLREF-ERROR", sprintf("x=%d", x), x;
}

int sub deep(int x) {
    return thrower(x) + 1;
}

int sub with_default(int x = 5) {
    return x * 3;
}

int sub fib_ref(code<int(int)> self_ref, int n) {
    return n < 2 ? n : self_ref(n - 1) + fib_ref(self_ref, n - 2);
}

code<int(int)> fib_cb;

int sub fib(int n) {
    return n < 2 ? n : fib_cb(n - 1) + fib_cb(n - 2);
}

int sub loop_int(code<int(int)> f, int n, int seed) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        seed = f(seed);
        sum += seed;
    }
    return sum;
}

int sub loop_int_result(code<int(int)> f, int n, int seed) {
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += int(f(seed));
        seed = 65535 - seed;
    }
    return sum;
}

float sub loop_float(code<float(float)> f, int n, float seed) {
    float v = seed;
    float sum = 0.0;
    for (int i = 0; i < n; ++i) {
        v = f(v);
        sum += v;
    }
    return sum;
}

int sub loop_bool(code<bool(bool)> f, int n) {
    bool b = False;
    int trues = 0;
    for (int i = 0; i < n; ++i) {
        b = f(b);
        if (b) {
            ++trues;
        }
    }
    return trues;
}

auto sub call0(code f) {
    return f();
}

auto sub call1(code f, auto x) {
    return f(x);
}

auto sub call_int(code f, int x) {
    return f(x);
}

auto sub call_float(code f, float x) {
    return f(x);
}

auto sub call_bool(code f, bool b) {
    return f(b);
}

auto sub call2(code f, auto a, auto b) {
    return f(a, b);
}

auto sub call3(code f, auto a, auto b, auto c) {
    return f(a, b, c);
}

auto sub call_named(code f) {
    return f(x: 7);
}

int sub catch_loop(code f, int n) {
    int caught = 0;
    for (int i = 0; i < n; ++i) {
        try {
            f(i);
        } catch (hash<ExceptionInfo> ex) {
            if (ex.err == "CALLREF-ERROR" && ex.arg == i) {
                ++caught;
            }
        }
    }
    return caught;
}

string sub stack_of(hash<ExceptionInfo> ex) {
    list<string> l = ();
    foreach hash<auto> frame in (ex.callstack) {
        if (UserFunctions{frame.function}) {
            push l, sprintf("%s:%d", frame.function, frame.line);
        }
    }
    return l.join(" <- ");
}

string sub err(hash<ExceptionInfo> ex) {
    return sprintf("%s: %s (arg %y)", ex.err, ex.desc, ex.arg);
}

int native_calls_before = 0;
int fast_calls_before = 0;

sub case_start() {
    if (report) {
        native_calls_before = call_function("dbg_function_reference_native_calls");
        fast_calls_before = call_function("dbg_function_reference_fast_calls");
    }
}

sub case_end(string name, string result) {
    if (report) {
        int native = call_function("dbg_function_reference_native_calls") - native_calls_before;
        int fast = call_function("dbg_function_reference_fast_calls") - fast_calls_before;
        printf("%s: %s\n", name, native ? "native" : (fast ? "fast" : "generic"));
    } else {
        printf("%s: %s\n", name, result);
    }
}

sub run_case(string name, code c) {
    case_start();
    string result;
    try {
        result = c();
    } catch (hash<ExceptionInfo> ex) {
        result = "unexpected " + err(ex);
    }
    case_end(name, result);
}

# native int, float and bool arguments and results, consumed by native locals and compound assignments
run_case("int", string sub () { return sprintf("%d", loop_int(\step(), N, 7)); });
run_case("int result", string sub () { return sprintf("%d", loop_int_result(\step(), N, 7)); });
run_case("float", string sub () { return sprintf("%.3f", loop_float(\fstep(), N, 7.0)); });
run_case("bool", string sub () { return sprintf("%d", loop_bool(\negb(), N)); });
run_case("bool as auto", string sub () { return sprintf("%y %y", call_bool(\negb(), True), call_bool(\negb(), False)); });
run_case("int to float", string sub () { return sprintf("%y", call_int(\i2f(), 7)); });
run_case("float to int", string sub () { return sprintf("%y", call_float(\f2i(), 7.5)); });
run_case("no args", string sub () {
    return sprintf("%y %y %y", call0(\izero()), call0(\fzero()), call0(\bzero()));
});
# values that need a node: ints beyond 48 bits and floats that collide with a value tag
run_case("wide int", string sub () {
    int max = 9223372036854775807;
    return sprintf("%d %d %d", call_int(\wide(), 1125899906842624), call_int(\wide(), max - 1),
        call_int(\wide(), -9223372036854775807 - 1));
});
run_case("wide int loop", string sub () {
    int v = 1125899906842624;
    code<int(int)> f = \wide();
    for (int i = 0; i < N; ++i) {
        v = f(v);
    }
    return sprintf("%d", v);
});
run_case("tagged float", string sub () {
    return sprintf("%y %y", call_float(\tagged(), 1.0), loop_float(\tagged(), 3, -3.0e307));
});
# NaN-boxed arguments and results
run_case("string", string sub () { return call1(\greet(), "there"); });
run_case("mixed native kinds", string sub () { return sprintf("%d", call2(\mixed(), 3, 1.5)); });
run_case("3 args", string sub () { return sprintf("%d", call3(\three(), 1, 2, 3)); });
run_case("boxed and native", string sub () { return call3(\boxed_mixed(), "x", 4, True); });
run_case("int as auto", string sub () { return sprintf("%y", call1(\step(), 7)); });
run_case("float as auto", string sub () { return sprintf("%y", call1(\fstep(), 7.0)); });
# exceptions raised by the callee and their call stacks
run_case("exception", string sub () {
    try {
        call_int(\thrower(), 3);
        return "no exception";
    } catch (hash<ExceptionInfo> ex) {
        return err(ex) + " at " + stack_of(ex);
    }
});
run_case("nested exception", string sub () {
    try {
        call_int(\deep(), 4);
        return "no exception";
    } catch (hash<ExceptionInfo> ex) {
        return err(ex) + " at " + stack_of(ex);
    }
});
run_case("exceptions in a loop", string sub () { return sprintf("%d", catch_loop(\thrower(), N)); });
run_case("return type", string sub () {
    try {
        return sprintf("%y", call_int(\bad_return(), 9));
    } catch (hash<ExceptionInfo> ex) {
        return err(ex);
    }
});
run_case("auto return to int", string sub () {
    list<string> l = ();
    foreach int x in ((3, 9)) {
        try {
            push l, sprintf("%y", loop_int(\auto_return(), 1, x));
        } catch (hash<ExceptionInfo> ex) {
            push l, ex.err;
        }
    }
    return l.join(" ");
});
# values of another type stored into native locals: the generic call path's value is checked and converted as the
# store converts it
run_case("store conversions", string sub () {
    code f = \auto_value();
    list<string> l = ();
    for (int x = 0; x < 6; ++x) {
        try {
            int i = f(x);
            push l, sprintf("i%y", i);
        } catch (hash<ExceptionInfo> ex) {
            push l, "i:" + ex.err;
        }
        try {
            float d = f(x);
            push l, sprintf("f%y", d);
        } catch (hash<ExceptionInfo> ex) {
            push l, "f:" + ex.err;
        }
        try {
            bool b = f(x);
            push l, sprintf("b%y", b);
        } catch (hash<ExceptionInfo> ex) {
            push l, "b:" + ex.err;
        }
    }
    return l.join(" ");
});
run_case("entry's boxed value stored into native locals", string sub () {
    code f = \num_value();
    code g = \opt_value();
    list<string> l = ();
    for (int x = 4; x < 7; ++x) {
        try {
            int i = f(x);
            push l, sprintf("i%y", i);
        } catch (hash<ExceptionInfo> ex) {
            push l, "i:" + ex.err;
        }
        try {
            float d = f(x);
            push l, sprintf("f%y", d);
        } catch (hash<ExceptionInfo> ex) {
            push l, "f:" + ex.err;
        }
        try {
            bool b = g(x);
            push l, sprintf("b%y", b);
        } catch (hash<ExceptionInfo> ex) {
            push l, "b:" + ex.err;
        }
        int j = g(x);
        push l, sprintf("j%y", j);
        foreach code c in ((f, g)) {
            try {
                push l, sprintf("I%y", store_int(c, x));
            } catch (hash<ExceptionInfo> ex) {
                push l, "I:" + ex.err;
            }
            try {
                push l, sprintf("F%y", store_float(c, x));
            } catch (hash<ExceptionInfo> ex) {
                push l, "F:" + ex.err;
            }
            try {
                push l, sprintf("B%y", store_bool(c, x));
            } catch (hash<ExceptionInfo> ex) {
                push l, "B:" + ex.err;
            }
        }
    }
    return l.join(" ");
});
run_case("native bool and float stores", string sub () {
    code<bool(bool)> nb = \negb();
    code<float(int)> fi = \i2f();
    bool b = False;
    float f = 0.0;
    for (int i = 0; i < N; ++i) {
        b = nb(b);
        f = fi(i);
    }
    return sprintf("%y %y", b, f);
});
# calls that the generic call path makes
run_case("named", string sub () {
    try {
        return sprintf("%y", call_named(\step()));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("too few args", string sub () {
    try {
        return sprintf("%y", call0(\step()));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("too many args", string sub () {
    try {
        return sprintf("%y", call2(\step(), 1, 2));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("wrong type", string sub () {
    try {
        return sprintf("%y", call1(\step(), "x"));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("NOTHING arg", string sub () {
    try {
        return sprintf("%y", call1(\greet(), NOTHING));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("float arg to int", string sub () {
    try {
        return sprintf("%y", call_float(\step(), 2.0));
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
});
run_case("default argument", string sub () { return sprintf("%y", call_int(\with_default(), 2)); });
run_case("closure", string sub () {
    int k = 3;
    return sprintf("%y", call_int(int sub (int x) { return x * k; }, 5));
});
# references made by the runtime and stored in containers
run_case("runtime reference", string sub () {
    return sprintf("%d", loop_int(get_call_reference("step"), N, 7));
});
run_case("reference in a hash", string sub () {
    hash<string, code> h = {"f": \step()};
    int v = 7;
    for (int i = 0; i < N; ++i) {
        v = h.f(v);
    }
    return sprintf("%d", v);
});
run_case("recursion", string sub () {
    fib_cb = \fib();
    return sprintf("%d %d", fib(15), fib_ref(\fib(), 12));
});
# the first calls of references to many functions, made by many threads at once
run_case("concurrent first use", string sub () {
    list<code> fs = (\step(), \wide(), \fstep(), \negb(), \i2f(), \f2i(), \izero(), \greet());
    list<auto> args = (7, 7, 7.0, False, 7, 7.5, NOTHING, "x");
    Counter c(1);
    Queue q();
    for (int t = 0; t < 8; ++t) {
        background sub () {
            c.waitForZero();
            list<string> r = ();
            for (int i = 0; i < fs.size(); ++i) {
                push r, sprintf("%y", exists args[i] ? call1(fs[i], args[i]) : call0(fs[i]));
            }
            q.push(r.join(","));
        }();
    }
    c.dec();
    hash<string, int> seen = {};
    for (int t = 0; t < 8; ++t) {
        ++seen{q.get()};
    }
    return seen.keys().join("|") + sprintf(" x%d", seen.firstValue());
});
