#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# Applies compound assignments whose result the lvalue's declared type accepts, or may accept, next to those that
# are parse errors (qore#6103): an lvalue type that accepts or converts the result, an operand or lvalue whose type
# is not known at parse time, and an operand that may have no value.  The cases run three times, so that tiered
# execution compiles them, and print in the last run.

%modern

auto sub id(auto v) {
    return v;
}

*date sub no_date() {
}

#! true in the last run, which prints the results
our bool print_results;

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
    check("int *= int", auto sub () { int i = 3; i *= 2; return i; });
    check("float *= int", auto sub () { float f = 1.5; f *= 2; return f; });
    check("date *= int", auto sub () { date d = 1h; d *= 2; return d; });
    check("auto *= date", auto sub () { auto a; a *= 2h; return a; });
    check("any *= date", auto sub () { any a = id(2); a *= 1h; return a; });
    check("softint *= date", auto sub () { softint i = 2; i *= 1h; return i; });
    check("softstring *= date", auto sub () { softstring s = "2"; s *= 1h; return s; });
    check("timeout *= date", auto sub () { timeout t = 2; t *= 1s; return t; });
    check("union<int, date> *= date", auto sub () { union<int, date> u = 2; u *= 1h; return u; });
    check("*union<int, date> *= date", auto sub () { *union<int, date> u; u *= 1h; return u; });
    check("int *= *date", auto sub () { int i = 3; i *= no_date(); return i; });
    check("int *= auto date", auto sub () { int i = 3; i *= id(1h); return i; });
    check("softint -= date", auto sub () { softint i = 10; i -= 1h; return i; });
    check("timeout -= date", auto sub () { timeout t = 5000; t -= 1s; return t; });
    check("int -= *date", auto sub () { int i = 3; i -= no_date(); return i; });
    check("int -= int", auto sub () { int i = 3; i -= 1; return i; });
    check("int /= date", auto sub () { int i = 7200; i /= 1h; return i; });
    check("any /= int", auto sub () { any a = id(2h); a /= 2; return a; });
    check("softdate /= int", auto sub () { softdate d = 1h; d /= 2; return d.type(); });
    check("auto date /= int", auto sub () { auto d = id(1h); d /= 2; return d; });
}

for (int i = 0; i < 3; ++i) {
    print_results = (i == 2);
    run_cases();
}
