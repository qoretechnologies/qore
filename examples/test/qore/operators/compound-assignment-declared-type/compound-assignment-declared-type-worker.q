#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Applies compound assignments whose result is not of the lvalue's declared type, and -= with a date operand to int
# lvalues, and prints each result next to the result of the plain assignment of the same value (x = x <op> y).  The
# cases run three times, so that tiered execution compiles them, and print in the last run.

%modern

auto sub id(auto v) {
    return v;
}

class FloatMember {
    public {
        float v;
    }
}

class IntMember {
    public {
        int v = 10;
    }
}

#! true in the last run, which prints the results
our bool print_results;

our int gi = 10;

#! runs one operation and prints its outcome
sub check(string label, code op) {
    string outcome;
    try {
        auto v = op();
        outcome = sprintf("%y %s", v, v.type());
    } catch (hash<ExceptionInfo> ex) {
        outcome = ex.err;
    }
    if (print_results) {
        printf("%s: %s\n", label, outcome);
    }
}

sub run_cases() {
    # a result is stored through the declared type, as an assignment of the result stores it
    check("number *= float", auto sub () { number n; n *= id(1.5); return n; });
    check("number = number * float", auto sub () { number n; n = n * id(1.5); return n; });
    check("number /= int", auto sub () { number n; n /= id(1); return n; });
    check("number 2n *= float", auto sub () { number n = 2n; n *= id(1.5); return n; });
    check("float member /= int", auto sub () { FloatMember m(); m.v /= id(1); return m.v; });
    check("float member ++", auto sub () { FloatMember m(); m.v++; return m.v; });
    check("hash<string, float> ++", auto sub () { hash<string, float> h = {}; h.v++; return h.v; });
    check("hash<string, float> /= int", auto sub () { hash<string, float> h = {}; h.v /= id(2); return h.v; });
    check("narrowed float hash member >>= int", auto sub () {
        hash<auto> h = id({"v": 8.5});
        h.v >>= id(1);
        return h.v;
    });
    check("narrowed float hash member <<= int", auto sub () {
        hash<auto> h = id({"v": 2.5});
        h.v <<= id(1);
        return h.v;
    });
    check("narrowed float hash member %= int", auto sub () {
        hash<auto> h = id({"v": 7.5});
        h.v %= id(4);
        return h.v;
    });
    check("narrowed float hash member |= int", auto sub () {
        hash<auto> h = id({"v": 4.5});
        h.v |= id(1);
        return h.v;
    });
    check("timeout *= float", auto sub () { timeout t; t *= id(1.5); return t; });
    check("timeout 1000 *= float", auto sub () { timeout t = 1000; t *= id(1.5); return t; });
    check("timeout = float", auto sub () { timeout t = id(1.5); return t; });
    check("timeout /= float", auto sub () { timeout t = 1000; t /= id(2.5); return t; });

    # -= with a date assigns the result of the binary operator, as x = x - d does
    check("int -= absolute date", auto sub () { int x = 10; x -= id(2024-01-01T00:00:00Z); return x; });
    check("int = int - absolute date", auto sub () { int x = 10; x = x - id(2024-01-01T00:00:00Z); return x; });
    check("int -= relative date", auto sub () { int x = 10; x -= id(1h); return x; });
    check("int -= literal date", auto sub () { int x = 10; x -= 1h; return x; });
    check("*int -= relative date", auto sub () { *int x = 10; x -= id(1h); return x; });
    check("global int -= relative date", auto sub () { gi = 10; gi -= id(1h); return gi; });
    check("int member -= relative date", auto sub () { IntMember m(); m.v -= id(1h); return m.v; });
    check("narrowed hash member -= relative date", auto sub () {
        hash<auto> h = {"v": 10};
        h.v -= id(1h);
        return h.v;
    });
    check("narrowed hash member -= absolute date", auto sub () {
        hash<auto> h = {"v": 10};
        h.v -= id(2024-01-01T00:00:00Z);
        return h.v;
    });
    check("softint -= relative date", auto sub () { softint x = 10; x -= id(1h); return x; });
    check("softint = softint - relative date", auto sub () { softint x = 10; x = x - id(1h); return x; });
    check("timeout -= relative date", auto sub () { timeout t = 5000; t -= id(1s); return t; });
    check("int -= NOTHING", auto sub () { int x; x -= id(NOTHING); return x; });
    check("*int -= NOTHING", auto sub () { *int x; x -= id(NOTHING); return x; });
    check("int -= int", auto sub () { int x = 10; x -= id(3); return x; });
}

for (int run = 0; run < 3; ++run) {
    print_results = run == 2;
    run_cases();
}
