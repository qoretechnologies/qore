#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the conversions of bool values to int, float and number in functions, which JIT, tiered and AOT execution compile to
# native code; prints one line per conversion: "<name>: <value as %y>"

%modern

float sub float_of_softbool(softbool b) {
    return float(b);
}

float sub float_of_bool(bool b) {
    return float(b);
}

int sub int_of_softbool(softbool b) {
    return int(b);
}

number sub number_of_softbool(softbool b) {
    return number(b);
}

softfloat sub softfloat_assign(softbool b) {
    softfloat f = b;
    return f;
}

softint sub softint_assign(softbool b) {
    softint i = b;
    return i;
}

softnumber sub softnumber_assign(softbool b) {
    softnumber n = b;
    return n;
}

# a function returning its bool parameter as softfloat: see issue #5724 (AOT returns the bool)

softint sub softint_return(bool b) {
    return b;
}

float sub float_add(softbool b) {
    return b + 0.5;
}

int sub int_mul(softbool b) {
    return b * 3;
}

number sub number_add(softbool b) {
    return b + 1n;
}

float sub float_param(softfloat f) {
    return f;
}

float sub pass_softfloat(bool b) {
    return float_param(b);
}

# a bool or an int reaching one variable at a join
softint sub softint_join(bool b, bool which) {
    softint i;
    if (which) {
        i = b;
    } else {
        i = 7;
    }
    return i;
}

softfloat sub softfloat_join(bool b, bool which) {
    softfloat f;
    if (which) {
        f = b;
    } else {
        f = 2.5;
    }
    return f;
}

softint sub softint_ternary(bool b, bool which) {
    softint i = which ? b : 9;
    return i;
}

softfloat sub softfloat_ternary(bool b, bool which) {
    softfloat f = which ? b : 9.5;
    return f;
}

# a loop whose variable is a bool converted to int and float in each iteration
list<auto> sub loop_sum(bool b) {
    int is = 0;
    float fs = 0.0;
    for (int n = 0; n < 4; ++n) {
        softint i = b;
        softfloat f = b;
        is += i;
        fs += f;
    }
    return (is, fs);
}

# a softfloat that a loop assigns a comparison
softfloat sub loop_comparison_float(int n) {
    softfloat f = 5.0;
    for (int k = 0; k < n; ++k) {
        f = (k < 100);
    }
    return f;
}

float sub float_of_comparison(int a, int c) {
    return float(a < c);
}

int sub int_of_comparison(int a, int c) {
    return int(a < c);
}

float sub float_of_not(bool b) {
    return float(!b);
}

hash<string, auto> sub conversions(bool b) {
    return {
        "float(softbool)": float_of_softbool(b),
        "float(bool)": float_of_bool(b),
        "int(softbool)": int_of_softbool(b),
        "number(softbool)": number_of_softbool(b),
        "softfloat = softbool": softfloat_assign(b),
        "softint = softbool": softint_assign(b),
        "softnumber = softbool": softnumber_assign(b),
        "return bool as softint": softint_return(b),
        "softbool + 0.5": float_add(b),
        "softbool * 3": int_mul(b),
        "softbool + 1n": number_add(b),
        "bool as softfloat argument": pass_softfloat(b),
        "softint join (bool)": softint_join(b, True),
        "softint join (int)": softint_join(b, False),
        "softfloat join (bool)": softfloat_join(b, True),
        "softfloat join (float)": softfloat_join(b, False),
        "softint ternary (bool)": softint_ternary(b, True),
        "softint ternary (int)": softint_ternary(b, False),
        "softfloat ternary (bool)": softfloat_ternary(b, True),
        "softfloat ternary (float)": softfloat_ternary(b, False),
        "loop sum": loop_sum(b),
        "float(!bool)": float_of_not(b),
    };
}

# each function is called several times, so that tiered execution compiles it before the calls checked
for (int n = 0; n < 3; ++n) {
    conversions(True);
    conversions(False);
}
foreach bool b in ((True, False)) {
    foreach hash<auto> i in (conversions(b).pairIterator()) {
        printf("%y %s: %y\n", b, i.key, i.value);
    }
}
printf("float(1 < 2): %y\n", float_of_comparison(1, 2));
printf("float(2 < 1): %y\n", float_of_comparison(2, 1));
printf("int(1 < 2): %y\n", int_of_comparison(1, 2));
for (int n = 0; n < 3; ++n) {
    loop_comparison_float(3);
}
printf("loop softfloat = comparison: %y\n", loop_comparison_float(3));
