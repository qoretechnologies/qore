#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# qore#6252: switch cases with operators in other statement forms, compared with the if statement: a switch in a try
# block, plain and operator cases on an int (the IR uses a jump table for equality with int cases), switch values
# that are not local variables, nested switches, and number, softint, date, enum and NOTHING values

%modern

enum Color {
    Red = 1,
    Blue = 2,
}

int mismatches = 0;

sub check(string label, string sw, string cond) {
    printf("%s: %s %s\n", label, sw, cond);
    if (sw != cond) {
        ++mismatches;
    }
}

string sub mixed(int v) {
    switch (v) {
        case 1: return "one";
        case == 2: return "eq2";
        case > 3: return "gt3";
        default: return "other";
    }
}
string sub mixed_if(int v) {
    if (v === 1) { return "one"; }
    if (v == 2) { return "eq2"; }
    if (v > 3) { return "gt3"; }
    return "other";
}

string sub in_try(auto v) {
    try {
        switch (v) {
            case > 1: return "gt1";
            case == 0: return "eq0";
        }
        return "none";
    } catch (hash<ExceptionInfo> ex) {
        return "ex:" + ex.err;
    }
}
string sub in_try_if(auto v) {
    try {
        if (v > 1) { return "gt1"; }
        if (v == 0) { return "eq0"; }
        return "none";
    } catch (hash<ExceptionInfo> ex) {
        return "ex:" + ex.err;
    }
}

auto sub val(auto v) {
    return v;
}

string sub not_local(hash<auto> h, auto v) {
    switch (h.k) {
        case >= 10: return "h>=10";
    }
    switch (val(v)) {
        case < 0: return "call<0";
    }
    return "none";
}
string sub not_local_if(hash<auto> h, auto v) {
    if (h.k >= 10) { return "h>=10"; }
    if (val(v) < 0) { return "call<0"; }
    return "none";
}

string sub nested(auto a, auto b) {
    switch (a) {
        case > 0:
            switch (b) {
                case <= 5: return "a>0,b<=5";
                case > 5: return "a>0,b>5";
            }
            return "a>0";
        case == 0: return "a==0";
    }
    return "none";
}
string sub nested_if(auto a, auto b) {
    if (a > 0) {
        if (b <= 5) { return "a>0,b<=5"; }
        if (b > 5) { return "a>0,b>5"; }
        return "a>0";
    }
    if (a == 0) { return "a==0"; }
    return "none";
}

string sub typed_number(number n) { switch (n) { case > 1.5: return "1"; } return "0"; }
string sub typed_number_if(number n) { if (n > 1.5) { return "1"; } return "0"; }
string sub typed_softint(softint n) { switch (n) { case == "3": return "1"; } return "0"; }
string sub typed_softint_if(softint n) { if (n == "3") { return "1"; } return "0"; }
string sub typed_date(date d) { switch (d) { case < 2026-01-01: return "1"; } return "0"; }
string sub typed_date_if(date d) { if (d < 2026-01-01) { return "1"; } return "0"; }
string sub typed_enum(Color c) { switch (c) { case == 2: return "1"; } return "0"; }
string sub typed_enum_if(Color c) { if (c == 2) { return "1"; } return "0"; }
string sub maybe_int(*int i) { switch (i) { case < 1: return "1"; } return "0"; }
string sub maybe_int_if(*int i) { if (i < 1) { return "1"; } return "0"; }

foreach int v in ((0, 1, 2, 3, 4, -1)) {
    check(sprintf("mixed %d", v), mixed(v), mixed_if(v));
}
foreach auto v in ((0, 2, "x", new buffer<int64>((1, 2)), new buffer<int64>((0, 0)), new buffer<int64>(), NOTHING)) {
    check(sprintf("try %d", $#), in_try(v), in_try_if(v));
}
foreach auto v in ((-1, 1, "-2", NOTHING)) {
    check(sprintf("not local %d", $#), not_local({"k": $# * 5}, v), not_local_if({"k": $# * 5}, v));
}
foreach list<auto> ab in (((1, 3), (1, 7), (0, 1), (-1, 1), (new buffer<int64>((1,)), 9))) {
    check(sprintf("nested %d", $#), nested(ab[0], ab[1]), nested_if(ab[0], ab[1]));
}
foreach number n in ((1n, 2n, 1.5n)) {
    check(sprintf("number %d", $#), typed_number(n), typed_number_if(n));
}
foreach softint n in ((3, "3", 4)) {
    check(sprintf("softint %d", $#), typed_softint(n), typed_softint_if(n));
}
foreach date d in ((2025-06-01, 2026-06-01)) {
    check(sprintf("date %d", $#), typed_date(d), typed_date_if(d));
}
foreach Color c in ((Color::Red, Color::Blue)) {
    check(sprintf("enum %d", $#), typed_enum(c), typed_enum_if(c));
}
foreach *int i in ((NOTHING, 0, 1)) {
    check(sprintf("maybe int %d", $#), maybe_int(i), maybe_int_if(i));
}
printf("mismatches: %d\n", mismatches);
