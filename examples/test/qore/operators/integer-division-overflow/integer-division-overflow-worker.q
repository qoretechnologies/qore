# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the results of dividing the smallest integer by -1 in every form and the error of the modulo-equals operator
# with a divisor of 0, three times, so that tiered execution runs native code
%modern

int sub typed_div(int a, int b) {
    return a / b;
}

int sub typed_mod(int a, int b) {
    return a % b;
}

auto sub untyped_div(auto a, auto b) {
    return a / b;
}

auto sub untyped_mod(auto a, auto b) {
    return a % b;
}

int sub div_assign(int a, int b) {
    a /= b;
    return a;
}

int sub mod_assign(int a, int b) {
    a %= b;
    return a;
}

hash<auto> sub member_assign(int a, int b) {
    hash<auto> h = {"d": a, "m": a};
    h.d /= b;
    h.m %= b;
    return h;
}

auto sub untyped_mod_assign(auto a, auto b) {
    a %= b;
    return a;
}

int sub constant_div() {
    return (-9223372036854775807 - 1) / -1;
}

int sub constant_mod() {
    return (-9223372036854775807 - 1) % -1;
}

#! the error that the modulo-equals operator raises for a divisor of 0 (qore#1034), and the value it leaves
string sub mod_zero(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

int min = -9223372036854775807 - 1;
for (int i = 0; i < 3; ++i) {
    hash<auto> h = member_assign(min, -1);
    hash<auto> z = member_assign(7, 4);
    printf("%d %d %d %d %d %d %d %d %d %d %s %s %s %d\n", typed_div(min, -1), typed_mod(min, -1),
        untyped_div(min, -1), untyped_mod(min, -1), div_assign(min, -1), mod_assign(min, -1), h.d, h.m, constant_div(),
        constant_mod(), mod_zero(auto sub () { return mod_assign(7, 0); }),
        mod_zero(auto sub () { return untyped_mod_assign(7, 0); }), mod_zero(auto sub () { return z.m %= 0; }), z.m);
}
