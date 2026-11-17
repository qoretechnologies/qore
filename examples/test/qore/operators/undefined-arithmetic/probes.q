# -*- mode: qore; indent-tabs-mode: nil -*-
# the probes of undefined-arithmetic.qtest (qore#1034): division and modulo by zero and the float and number math
# functions outside their domain

%modern

string sub div_a(auto a, auto b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_i(int a, int b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_f(float a, float b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_n(number a, number b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_si(softint a, auto b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_sf(softfloat a, auto b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_sn(softnumber a, auto b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_if(int a, float b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_fi(float a, int b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_fn(float a, number b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_nf(number a, float b) {
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_i0(int a) {
    int b = 0;
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_f0(float a) {
    float b = 0.0;
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_fm0(float a) {
    float b = -0.0;
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_n0(number a) {
    number b = 0n;
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub div_a0(auto a) {
    auto b = 0;
    try { return sprintf("%y", a / b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_a(auto a, auto b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_i(int a, int b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_f(float a, float b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_n(number a, number b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_si(softint a, auto b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_sf(softfloat a, auto b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_sn(softnumber a, auto b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_fi(float a, int b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_nf(number a, float b) {
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_i0(int a) {
    int b = 0;
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_f0(float a) {
    float b = 0.0;
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_fm0(float a) {
    float b = -0.0;
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_n0(number a) {
    number b = 0n;
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub dive_a0(auto a) {
    auto b = 0;
    try { a /= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_a(auto a, auto b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_i(int a, int b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_f(float a, float b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_n(number a, number b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_si(softint a, auto b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_sf(softfloat a, auto b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_sn(softnumber a, auto b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_if(int a, float b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_fi(float a, int b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_fn(float a, number b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_nf(number a, float b) {
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_i0(int a) {
    int b = 0;
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_f0(float a) {
    float b = 0.0;
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_fm0(float a) {
    float b = -0.0;
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_n0(number a) {
    number b = 0n;
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mod_a0(auto a) {
    auto b = 0;
    try { return sprintf("%y", a % b); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mode_a(auto a, auto b) {
    try { a %= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mode_i(int a, int b) {
    try { a %= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mode_si(softint a, auto b) {
    try { a %= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mode_if(int a, float b) {
    try { a %= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mode_i0(int a) {
    int b = 0;
    try { a %= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub mode_a0(auto a) {
    auto b = 0;
    try { a %= b; return sprintf("%y", a); } catch (hash<ExceptionInfo> e) { return e.err; }
}

string sub op_probes_0() {
    list<string> l = ();
    l += sprintf("div_a(%s) %s", "1.0, 0.0", div_a(1.0, 0.0));
    l += sprintf("div_a(%s) %s", "-1.0, 0.0", div_a(-1.0, 0.0));
    l += sprintf("div_a(%s) %s", "0.0, 0.0", div_a(0.0, 0.0));
    l += sprintf("div_a(%s) %s", "@inf@, 0.0", div_a(@inf@, 0.0));
    l += sprintf("div_a(%s) %s", "-@inf@, 0.0", div_a(-@inf@, 0.0));
    l += sprintf("div_a(%s) %s", "@nan@, 0.0", div_a(@nan@, 0.0));
    l += sprintf("div_a(%s) %s", "1.0, -0.0", div_a(1.0, -0.0));
    l += sprintf("div_a(%s) %s", "0.0, -0.0", div_a(0.0, -0.0));
    l += sprintf("div_a(%s) %s", "1.0, @inf@", div_a(1.0, @inf@));
    l += sprintf("div_a(%s) %s", "1.0, @nan@", div_a(1.0, @nan@));
    l += sprintf("div_a(%s) %s", "@nan@, @nan@", div_a(@nan@, @nan@));
    l += sprintf("div_a(%s) %s", "@inf@, @inf@", div_a(@inf@, @inf@));
    l += sprintf("div_a(%s) %s", "0.0, @nan@", div_a(0.0, @nan@));
    l += sprintf("div_a(%s) %s", "1.0, 5e-324", div_a(1.0, 5e-324));
    l += sprintf("div_a(%s) %s", "6.0, 4.0", div_a(6.0, 4.0));
    l += sprintf("div_a(%s) %s", "1.0n, 0.0n", div_a(1.0n, 0.0n));
    l += sprintf("div_a(%s) %s", "-1.0n, 0.0n", div_a(-1.0n, 0.0n));
    l += sprintf("div_a(%s) %s", "0.0n, 0.0n", div_a(0.0n, 0.0n));
    l += sprintf("div_a(%s) %s", "@inf@n, 0.0n", div_a(@inf@n, 0.0n));
    l += sprintf("div_a(%s) %s", "-@inf@n, 0.0n", div_a(-@inf@n, 0.0n));
    l += sprintf("div_a(%s) %s", "@nan@n, 0.0n", div_a(@nan@n, 0.0n));
    l += sprintf("div_a(%s) %s", "1.0n, -0.0n", div_a(1.0n, -0.0n));
    l += sprintf("div_a(%s) %s", "0.0n, -0.0n", div_a(0.0n, -0.0n));
    l += sprintf("div_a(%s) %s", "1.0n, @inf@n", div_a(1.0n, @inf@n));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_1() {
    list<string> l = ();
    l += sprintf("div_a(%s) %s", "1.0n, @nan@n", div_a(1.0n, @nan@n));
    l += sprintf("div_a(%s) %s", "@nan@n, @nan@n", div_a(@nan@n, @nan@n));
    l += sprintf("div_a(%s) %s", "@inf@n, @inf@n", div_a(@inf@n, @inf@n));
    l += sprintf("div_a(%s) %s", "0.0n, @nan@n", div_a(0.0n, @nan@n));
    l += sprintf("div_a(%s) %s", "1.0n, 5e-324n", div_a(1.0n, 5e-324n));
    l += sprintf("div_a(%s) %s", "6.0n, 4.0n", div_a(6.0n, 4.0n));
    l += sprintf("div_a(%s) %s", "1, 0", div_a(1, 0));
    l += sprintf("div_a(%s) %s", "0, 0", div_a(0, 0));
    l += sprintf("div_a(%s) %s", "-1, 0", div_a(-1, 0));
    l += sprintf("div_a(%s) %s", "MININT, 0", div_a(MININT, 0));
    l += sprintf("div_a(%s) %s", "MININT, -1", div_a(MININT, -1));
    l += sprintf("div_a(%s) %s", "MAXINT, -1", div_a(MAXINT, -1));
    l += sprintf("div_a(%s) %s", "7, -1", div_a(7, -1));
    l += sprintf("div_a(%s) %s", "7, 2", div_a(7, 2));
    l += sprintf("div_a(%s) %s", "-7, 2", div_a(-7, 2));
    l += sprintf("div_a(%s) %s", "1, 0.0", div_a(1, 0.0));
    l += sprintf("div_a(%s) %s", "1.0, 0", div_a(1.0, 0));
    l += sprintf("div_a(%s) %s", "1n, 0", div_a(1n, 0));
    l += sprintf("div_a(%s) %s", "1, 0n", div_a(1, 0n));
    l += sprintf("div_a(%s) %s", "1n, 0.0", div_a(1n, 0.0));
    l += sprintf("div_a(%s) %s", "1.0, 0n", div_a(1.0, 0n));
    l += sprintf("div_a(%s) %s", "1, -0.0", div_a(1, -0.0));
    l += sprintf("div_a(%s) %s", "\"1\", \"0\"", div_a("1", "0"));
    l += sprintf("div_a(%s) %s", "1, \"0\"", div_a(1, "0"));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_2() {
    list<string> l = ();
    l += sprintf("div_a(%s) %s", "1, False", div_a(1, False));
    l += sprintf("div_a(%s) %s", "1, NULL", div_a(1, NULL));
    l += sprintf("div_a(%s) %s", "1, NOTHING", div_a(1, NOTHING));
    l += sprintf("div_a(%s) %s", "1, @nan@", div_a(1, @nan@));
    l += sprintf("div_a(%s) %s", "\"abc\", 0", div_a("abc", 0));
    l += sprintf("div_a(%s) %s", "1n, @nan@", div_a(1n, @nan@));
    l += sprintf("div_a(%s) %s", "@nan@, 0n", div_a(@nan@, 0n));
    l += sprintf("div_a(%s) %s", "7, 2.0", div_a(7, 2.0));
    l += sprintf("div_a(%s) %s", "7, 2n", div_a(7, 2n));
    l += sprintf("div_i(%s) %s", "1, 0", div_i(1, 0));
    l += sprintf("div_i(%s) %s", "0, 0", div_i(0, 0));
    l += sprintf("div_i(%s) %s", "-1, 0", div_i(-1, 0));
    l += sprintf("div_i(%s) %s", "MININT, 0", div_i(MININT, 0));
    l += sprintf("div_i(%s) %s", "MININT, -1", div_i(MININT, -1));
    l += sprintf("div_i(%s) %s", "MAXINT, -1", div_i(MAXINT, -1));
    l += sprintf("div_i(%s) %s", "7, -1", div_i(7, -1));
    l += sprintf("div_i(%s) %s", "7, 2", div_i(7, 2));
    l += sprintf("div_i(%s) %s", "-7, 2", div_i(-7, 2));
    l += sprintf("div_f(%s) %s", "1.0, 0.0", div_f(1.0, 0.0));
    l += sprintf("div_f(%s) %s", "-1.0, 0.0", div_f(-1.0, 0.0));
    l += sprintf("div_f(%s) %s", "0.0, 0.0", div_f(0.0, 0.0));
    l += sprintf("div_f(%s) %s", "@inf@, 0.0", div_f(@inf@, 0.0));
    l += sprintf("div_f(%s) %s", "-@inf@, 0.0", div_f(-@inf@, 0.0));
    l += sprintf("div_f(%s) %s", "@nan@, 0.0", div_f(@nan@, 0.0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_3() {
    list<string> l = ();
    l += sprintf("div_f(%s) %s", "1.0, -0.0", div_f(1.0, -0.0));
    l += sprintf("div_f(%s) %s", "0.0, -0.0", div_f(0.0, -0.0));
    l += sprintf("div_f(%s) %s", "1.0, @inf@", div_f(1.0, @inf@));
    l += sprintf("div_f(%s) %s", "1.0, @nan@", div_f(1.0, @nan@));
    l += sprintf("div_f(%s) %s", "@nan@, @nan@", div_f(@nan@, @nan@));
    l += sprintf("div_f(%s) %s", "@inf@, @inf@", div_f(@inf@, @inf@));
    l += sprintf("div_f(%s) %s", "0.0, @nan@", div_f(0.0, @nan@));
    l += sprintf("div_f(%s) %s", "1.0, 5e-324", div_f(1.0, 5e-324));
    l += sprintf("div_f(%s) %s", "6.0, 4.0", div_f(6.0, 4.0));
    l += sprintf("div_n(%s) %s", "1.0n, 0.0n", div_n(1.0n, 0.0n));
    l += sprintf("div_n(%s) %s", "-1.0n, 0.0n", div_n(-1.0n, 0.0n));
    l += sprintf("div_n(%s) %s", "0.0n, 0.0n", div_n(0.0n, 0.0n));
    l += sprintf("div_n(%s) %s", "@inf@n, 0.0n", div_n(@inf@n, 0.0n));
    l += sprintf("div_n(%s) %s", "-@inf@n, 0.0n", div_n(-@inf@n, 0.0n));
    l += sprintf("div_n(%s) %s", "@nan@n, 0.0n", div_n(@nan@n, 0.0n));
    l += sprintf("div_n(%s) %s", "1.0n, -0.0n", div_n(1.0n, -0.0n));
    l += sprintf("div_n(%s) %s", "0.0n, -0.0n", div_n(0.0n, -0.0n));
    l += sprintf("div_n(%s) %s", "1.0n, @inf@n", div_n(1.0n, @inf@n));
    l += sprintf("div_n(%s) %s", "1.0n, @nan@n", div_n(1.0n, @nan@n));
    l += sprintf("div_n(%s) %s", "@nan@n, @nan@n", div_n(@nan@n, @nan@n));
    l += sprintf("div_n(%s) %s", "@inf@n, @inf@n", div_n(@inf@n, @inf@n));
    l += sprintf("div_n(%s) %s", "0.0n, @nan@n", div_n(0.0n, @nan@n));
    l += sprintf("div_n(%s) %s", "1.0n, 5e-324n", div_n(1.0n, 5e-324n));
    l += sprintf("div_n(%s) %s", "6.0n, 4.0n", div_n(6.0n, 4.0n));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_4() {
    list<string> l = ();
    l += sprintf("div_si(%s) %s", "1, 0", div_si(1, 0));
    l += sprintf("div_si(%s) %s", "1, 0.5", div_si(1, 0.5));
    l += sprintf("div_si(%s) %s", "1, @nan@", div_si(1, @nan@));
    l += sprintf("div_si(%s) %s", "1, 0n", div_si(1, 0n));
    l += sprintf("div_sf(%s) %s", "1, 0", div_sf(1, 0));
    l += sprintf("div_sf(%s) %s", "1.0, 0.0", div_sf(1.0, 0.0));
    l += sprintf("div_sf(%s) %s", "1, @nan@", div_sf(1, @nan@));
    l += sprintf("div_sf(%s) %s", "1, 0n", div_sf(1, 0n));
    l += sprintf("div_sf(%s) %s", "1, \"0\"", div_sf(1, "0"));
    l += sprintf("div_sn(%s) %s", "1, 0", div_sn(1, 0));
    l += sprintf("div_sn(%s) %s", "1.0, 0.0", div_sn(1.0, 0.0));
    l += sprintf("div_sn(%s) %s", "0n, 0n", div_sn(0n, 0n));
    l += sprintf("div_sn(%s) %s", "1, @nan@", div_sn(1, @nan@));
    l += sprintf("div_sn(%s) %s", "1, \"0\"", div_sn(1, "0"));
    l += sprintf("div_if(%s) %s", "1, 0.0", div_if(1, 0.0));
    l += sprintf("div_if(%s) %s", "1, -0.0", div_if(1, -0.0));
    l += sprintf("div_if(%s) %s", "1, 0.5", div_if(1, 0.5));
    l += sprintf("div_if(%s) %s", "1, @nan@", div_if(1, @nan@));
    l += sprintf("div_fi(%s) %s", "1.0, 0", div_fi(1.0, 0));
    l += sprintf("div_fi(%s) %s", "0.0, 0", div_fi(0.0, 0));
    l += sprintf("div_fi(%s) %s", "@nan@, 0", div_fi(@nan@, 0));
    l += sprintf("div_fn(%s) %s", "1.0, 0n", div_fn(1.0, 0n));
    l += sprintf("div_fn(%s) %s", "0.0, 0n", div_fn(0.0, 0n));
    l += sprintf("div_fn(%s) %s", "@nan@, 0n", div_fn(@nan@, 0n));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_5() {
    list<string> l = ();
    l += sprintf("div_nf(%s) %s", "1n, 0.0", div_nf(1n, 0.0));
    l += sprintf("div_nf(%s) %s", "0n, 0.0", div_nf(0n, 0.0));
    l += sprintf("div_nf(%s) %s", "@nan@n, 0.0", div_nf(@nan@n, 0.0));
    l += sprintf("div_nf(%s) %s", "1n, -0.0", div_nf(1n, -0.0));
    l += sprintf("div_i0(1) %s", div_i0(1));
    l += sprintf("div_i0(0) %s", div_i0(0));
    l += sprintf("div_f0(1.0) %s", div_f0(1.0));
    l += sprintf("div_f0(0.0) %s", div_f0(0.0));
    l += sprintf("div_f0(@nan@) %s", div_f0(@nan@));
    l += sprintf("div_fm0(1.0) %s", div_fm0(1.0));
    l += sprintf("div_n0(1n) %s", div_n0(1n));
    l += sprintf("div_n0(0n) %s", div_n0(0n));
    l += sprintf("div_n0(@nan@n) %s", div_n0(@nan@n));
    l += sprintf("div_a0(1) %s", div_a0(1));
    l += sprintf("div_a0(1.0) %s", div_a0(1.0));
    l += sprintf("div_a0(1n) %s", div_a0(1n));
    l += sprintf("dive_a(%s) %s", "1.0, 0.0", dive_a(1.0, 0.0));
    l += sprintf("dive_a(%s) %s", "-1.0, 0.0", dive_a(-1.0, 0.0));
    l += sprintf("dive_a(%s) %s", "0.0, 0.0", dive_a(0.0, 0.0));
    l += sprintf("dive_a(%s) %s", "@inf@, 0.0", dive_a(@inf@, 0.0));
    l += sprintf("dive_a(%s) %s", "-@inf@, 0.0", dive_a(-@inf@, 0.0));
    l += sprintf("dive_a(%s) %s", "@nan@, 0.0", dive_a(@nan@, 0.0));
    l += sprintf("dive_a(%s) %s", "1.0, -0.0", dive_a(1.0, -0.0));
    l += sprintf("dive_a(%s) %s", "0.0, -0.0", dive_a(0.0, -0.0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_6() {
    list<string> l = ();
    l += sprintf("dive_a(%s) %s", "1.0, @inf@", dive_a(1.0, @inf@));
    l += sprintf("dive_a(%s) %s", "1.0, @nan@", dive_a(1.0, @nan@));
    l += sprintf("dive_a(%s) %s", "@nan@, @nan@", dive_a(@nan@, @nan@));
    l += sprintf("dive_a(%s) %s", "@inf@, @inf@", dive_a(@inf@, @inf@));
    l += sprintf("dive_a(%s) %s", "0.0, @nan@", dive_a(0.0, @nan@));
    l += sprintf("dive_a(%s) %s", "1.0, 5e-324", dive_a(1.0, 5e-324));
    l += sprintf("dive_a(%s) %s", "6.0, 4.0", dive_a(6.0, 4.0));
    l += sprintf("dive_a(%s) %s", "1.0n, 0.0n", dive_a(1.0n, 0.0n));
    l += sprintf("dive_a(%s) %s", "-1.0n, 0.0n", dive_a(-1.0n, 0.0n));
    l += sprintf("dive_a(%s) %s", "0.0n, 0.0n", dive_a(0.0n, 0.0n));
    l += sprintf("dive_a(%s) %s", "@inf@n, 0.0n", dive_a(@inf@n, 0.0n));
    l += sprintf("dive_a(%s) %s", "-@inf@n, 0.0n", dive_a(-@inf@n, 0.0n));
    l += sprintf("dive_a(%s) %s", "@nan@n, 0.0n", dive_a(@nan@n, 0.0n));
    l += sprintf("dive_a(%s) %s", "1.0n, -0.0n", dive_a(1.0n, -0.0n));
    l += sprintf("dive_a(%s) %s", "0.0n, -0.0n", dive_a(0.0n, -0.0n));
    l += sprintf("dive_a(%s) %s", "1.0n, @inf@n", dive_a(1.0n, @inf@n));
    l += sprintf("dive_a(%s) %s", "1.0n, @nan@n", dive_a(1.0n, @nan@n));
    l += sprintf("dive_a(%s) %s", "@nan@n, @nan@n", dive_a(@nan@n, @nan@n));
    l += sprintf("dive_a(%s) %s", "@inf@n, @inf@n", dive_a(@inf@n, @inf@n));
    l += sprintf("dive_a(%s) %s", "0.0n, @nan@n", dive_a(0.0n, @nan@n));
    l += sprintf("dive_a(%s) %s", "1.0n, 5e-324n", dive_a(1.0n, 5e-324n));
    l += sprintf("dive_a(%s) %s", "6.0n, 4.0n", dive_a(6.0n, 4.0n));
    l += sprintf("dive_a(%s) %s", "1, 0", dive_a(1, 0));
    l += sprintf("dive_a(%s) %s", "0, 0", dive_a(0, 0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_7() {
    list<string> l = ();
    l += sprintf("dive_a(%s) %s", "-1, 0", dive_a(-1, 0));
    l += sprintf("dive_a(%s) %s", "MININT, 0", dive_a(MININT, 0));
    l += sprintf("dive_a(%s) %s", "MININT, -1", dive_a(MININT, -1));
    l += sprintf("dive_a(%s) %s", "MAXINT, -1", dive_a(MAXINT, -1));
    l += sprintf("dive_a(%s) %s", "7, -1", dive_a(7, -1));
    l += sprintf("dive_a(%s) %s", "7, 2", dive_a(7, 2));
    l += sprintf("dive_a(%s) %s", "-7, 2", dive_a(-7, 2));
    l += sprintf("dive_a(%s) %s", "1, 0.0", dive_a(1, 0.0));
    l += sprintf("dive_a(%s) %s", "1.0, 0", dive_a(1.0, 0));
    l += sprintf("dive_a(%s) %s", "1n, 0", dive_a(1n, 0));
    l += sprintf("dive_a(%s) %s", "1, 0n", dive_a(1, 0n));
    l += sprintf("dive_a(%s) %s", "1n, 0.0", dive_a(1n, 0.0));
    l += sprintf("dive_a(%s) %s", "1.0, 0n", dive_a(1.0, 0n));
    l += sprintf("dive_a(%s) %s", "1, -0.0", dive_a(1, -0.0));
    l += sprintf("dive_a(%s) %s", "\"1\", \"0\"", dive_a("1", "0"));
    l += sprintf("dive_a(%s) %s", "1, \"0\"", dive_a(1, "0"));
    l += sprintf("dive_a(%s) %s", "1, False", dive_a(1, False));
    l += sprintf("dive_a(%s) %s", "1, NULL", dive_a(1, NULL));
    l += sprintf("dive_a(%s) %s", "1, NOTHING", dive_a(1, NOTHING));
    l += sprintf("dive_a(%s) %s", "1, @nan@", dive_a(1, @nan@));
    l += sprintf("dive_a(%s) %s", "\"abc\", 0", dive_a("abc", 0));
    l += sprintf("dive_a(%s) %s", "1n, @nan@", dive_a(1n, @nan@));
    l += sprintf("dive_a(%s) %s", "@nan@, 0n", dive_a(@nan@, 0n));
    l += sprintf("dive_a(%s) %s", "7, 2.0", dive_a(7, 2.0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_8() {
    list<string> l = ();
    l += sprintf("dive_a(%s) %s", "7, 2n", dive_a(7, 2n));
    l += sprintf("dive_i(%s) %s", "1, 0", dive_i(1, 0));
    l += sprintf("dive_i(%s) %s", "0, 0", dive_i(0, 0));
    l += sprintf("dive_i(%s) %s", "-1, 0", dive_i(-1, 0));
    l += sprintf("dive_i(%s) %s", "MININT, 0", dive_i(MININT, 0));
    l += sprintf("dive_i(%s) %s", "MININT, -1", dive_i(MININT, -1));
    l += sprintf("dive_i(%s) %s", "MAXINT, -1", dive_i(MAXINT, -1));
    l += sprintf("dive_i(%s) %s", "7, -1", dive_i(7, -1));
    l += sprintf("dive_i(%s) %s", "7, 2", dive_i(7, 2));
    l += sprintf("dive_i(%s) %s", "-7, 2", dive_i(-7, 2));
    l += sprintf("dive_f(%s) %s", "1.0, 0.0", dive_f(1.0, 0.0));
    l += sprintf("dive_f(%s) %s", "-1.0, 0.0", dive_f(-1.0, 0.0));
    l += sprintf("dive_f(%s) %s", "0.0, 0.0", dive_f(0.0, 0.0));
    l += sprintf("dive_f(%s) %s", "@inf@, 0.0", dive_f(@inf@, 0.0));
    l += sprintf("dive_f(%s) %s", "-@inf@, 0.0", dive_f(-@inf@, 0.0));
    l += sprintf("dive_f(%s) %s", "@nan@, 0.0", dive_f(@nan@, 0.0));
    l += sprintf("dive_f(%s) %s", "1.0, -0.0", dive_f(1.0, -0.0));
    l += sprintf("dive_f(%s) %s", "0.0, -0.0", dive_f(0.0, -0.0));
    l += sprintf("dive_f(%s) %s", "1.0, @inf@", dive_f(1.0, @inf@));
    l += sprintf("dive_f(%s) %s", "1.0, @nan@", dive_f(1.0, @nan@));
    l += sprintf("dive_f(%s) %s", "@nan@, @nan@", dive_f(@nan@, @nan@));
    l += sprintf("dive_f(%s) %s", "@inf@, @inf@", dive_f(@inf@, @inf@));
    l += sprintf("dive_f(%s) %s", "0.0, @nan@", dive_f(0.0, @nan@));
    l += sprintf("dive_f(%s) %s", "1.0, 5e-324", dive_f(1.0, 5e-324));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_9() {
    list<string> l = ();
    l += sprintf("dive_f(%s) %s", "6.0, 4.0", dive_f(6.0, 4.0));
    l += sprintf("dive_n(%s) %s", "1.0n, 0.0n", dive_n(1.0n, 0.0n));
    l += sprintf("dive_n(%s) %s", "-1.0n, 0.0n", dive_n(-1.0n, 0.0n));
    l += sprintf("dive_n(%s) %s", "0.0n, 0.0n", dive_n(0.0n, 0.0n));
    l += sprintf("dive_n(%s) %s", "@inf@n, 0.0n", dive_n(@inf@n, 0.0n));
    l += sprintf("dive_n(%s) %s", "-@inf@n, 0.0n", dive_n(-@inf@n, 0.0n));
    l += sprintf("dive_n(%s) %s", "@nan@n, 0.0n", dive_n(@nan@n, 0.0n));
    l += sprintf("dive_n(%s) %s", "1.0n, -0.0n", dive_n(1.0n, -0.0n));
    l += sprintf("dive_n(%s) %s", "0.0n, -0.0n", dive_n(0.0n, -0.0n));
    l += sprintf("dive_n(%s) %s", "1.0n, @inf@n", dive_n(1.0n, @inf@n));
    l += sprintf("dive_n(%s) %s", "1.0n, @nan@n", dive_n(1.0n, @nan@n));
    l += sprintf("dive_n(%s) %s", "@nan@n, @nan@n", dive_n(@nan@n, @nan@n));
    l += sprintf("dive_n(%s) %s", "@inf@n, @inf@n", dive_n(@inf@n, @inf@n));
    l += sprintf("dive_n(%s) %s", "0.0n, @nan@n", dive_n(0.0n, @nan@n));
    l += sprintf("dive_n(%s) %s", "1.0n, 5e-324n", dive_n(1.0n, 5e-324n));
    l += sprintf("dive_n(%s) %s", "6.0n, 4.0n", dive_n(6.0n, 4.0n));
    l += sprintf("dive_si(%s) %s", "1, 0", dive_si(1, 0));
    l += sprintf("dive_si(%s) %s", "1, 0.5", dive_si(1, 0.5));
    l += sprintf("dive_si(%s) %s", "1, @nan@", dive_si(1, @nan@));
    l += sprintf("dive_si(%s) %s", "1, 0n", dive_si(1, 0n));
    l += sprintf("dive_sf(%s) %s", "1, 0", dive_sf(1, 0));
    l += sprintf("dive_sf(%s) %s", "1.0, 0.0", dive_sf(1.0, 0.0));
    l += sprintf("dive_sf(%s) %s", "1, @nan@", dive_sf(1, @nan@));
    l += sprintf("dive_sf(%s) %s", "1, 0n", dive_sf(1, 0n));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_10() {
    list<string> l = ();
    l += sprintf("dive_sf(%s) %s", "1, \"0\"", dive_sf(1, "0"));
    l += sprintf("dive_sn(%s) %s", "1, 0", dive_sn(1, 0));
    l += sprintf("dive_sn(%s) %s", "1.0, 0.0", dive_sn(1.0, 0.0));
    l += sprintf("dive_sn(%s) %s", "0n, 0n", dive_sn(0n, 0n));
    l += sprintf("dive_sn(%s) %s", "1, @nan@", dive_sn(1, @nan@));
    l += sprintf("dive_sn(%s) %s", "1, \"0\"", dive_sn(1, "0"));
    l += sprintf("dive_fi(%s) %s", "1.0, 0", dive_fi(1.0, 0));
    l += sprintf("dive_fi(%s) %s", "0.0, 0", dive_fi(0.0, 0));
    l += sprintf("dive_fi(%s) %s", "@nan@, 0", dive_fi(@nan@, 0));
    l += sprintf("dive_nf(%s) %s", "1n, 0.0", dive_nf(1n, 0.0));
    l += sprintf("dive_nf(%s) %s", "0n, 0.0", dive_nf(0n, 0.0));
    l += sprintf("dive_nf(%s) %s", "@nan@n, 0.0", dive_nf(@nan@n, 0.0));
    l += sprintf("dive_nf(%s) %s", "1n, -0.0", dive_nf(1n, -0.0));
    l += sprintf("dive_i0(1) %s", dive_i0(1));
    l += sprintf("dive_i0(0) %s", dive_i0(0));
    l += sprintf("dive_f0(1.0) %s", dive_f0(1.0));
    l += sprintf("dive_f0(0.0) %s", dive_f0(0.0));
    l += sprintf("dive_f0(@nan@) %s", dive_f0(@nan@));
    l += sprintf("dive_fm0(1.0) %s", dive_fm0(1.0));
    l += sprintf("dive_n0(1n) %s", dive_n0(1n));
    l += sprintf("dive_n0(0n) %s", dive_n0(0n));
    l += sprintf("dive_n0(@nan@n) %s", dive_n0(@nan@n));
    l += sprintf("dive_a0(1) %s", dive_a0(1));
    l += sprintf("dive_a0(1.0) %s", dive_a0(1.0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_11() {
    list<string> l = ();
    l += sprintf("dive_a0(1n) %s", dive_a0(1n));
    l += sprintf("mod_a(%s) %s", "5.5, 0.5", mod_a(5.5, 0.5));
    l += sprintf("mod_a(%s) %s", "1.0, 0.0", mod_a(1.0, 0.0));
    l += sprintf("mod_a(%s) %s", "1.0, -0.0", mod_a(1.0, -0.0));
    l += sprintf("mod_a(%s) %s", "1.0, @nan@", mod_a(1.0, @nan@));
    l += sprintf("mod_a(%s) %s", "@nan@, 1.0", mod_a(@nan@, 1.0));
    l += sprintf("mod_a(%s) %s", "@inf@, 2.0", mod_a(@inf@, 2.0));
    l += sprintf("mod_a(%s) %s", "7.9, 2.1", mod_a(7.9, 2.1));
    l += sprintf("mod_a(%s) %s", "1.0, @inf@", mod_a(1.0, @inf@));
    l += sprintf("mod_a(%s) %s", "5.5n, 0.5n", mod_a(5.5n, 0.5n));
    l += sprintf("mod_a(%s) %s", "1.0n, 0.0n", mod_a(1.0n, 0.0n));
    l += sprintf("mod_a(%s) %s", "1.0n, -0.0n", mod_a(1.0n, -0.0n));
    l += sprintf("mod_a(%s) %s", "1.0n, @nan@n", mod_a(1.0n, @nan@n));
    l += sprintf("mod_a(%s) %s", "@nan@n, 1.0n", mod_a(@nan@n, 1.0n));
    l += sprintf("mod_a(%s) %s", "@inf@n, 2.0n", mod_a(@inf@n, 2.0n));
    l += sprintf("mod_a(%s) %s", "7.9n, 2.1n", mod_a(7.9n, 2.1n));
    l += sprintf("mod_a(%s) %s", "1.0n, @inf@n", mod_a(1.0n, @inf@n));
    l += sprintf("mod_a(%s) %s", "1, 0", mod_a(1, 0));
    l += sprintf("mod_a(%s) %s", "0, 0", mod_a(0, 0));
    l += sprintf("mod_a(%s) %s", "-1, 0", mod_a(-1, 0));
    l += sprintf("mod_a(%s) %s", "MININT, 0", mod_a(MININT, 0));
    l += sprintf("mod_a(%s) %s", "MININT, -1", mod_a(MININT, -1));
    l += sprintf("mod_a(%s) %s", "MAXINT, -1", mod_a(MAXINT, -1));
    l += sprintf("mod_a(%s) %s", "7, -1", mod_a(7, -1));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_12() {
    list<string> l = ();
    l += sprintf("mod_a(%s) %s", "7, 2", mod_a(7, 2));
    l += sprintf("mod_a(%s) %s", "-7, 2", mod_a(-7, 2));
    l += sprintf("mod_a(%s) %s", "1, 0.0", mod_a(1, 0.0));
    l += sprintf("mod_a(%s) %s", "1.0, 0", mod_a(1.0, 0));
    l += sprintf("mod_a(%s) %s", "1n, 0", mod_a(1n, 0));
    l += sprintf("mod_a(%s) %s", "1, 0n", mod_a(1, 0n));
    l += sprintf("mod_a(%s) %s", "1n, 0.0", mod_a(1n, 0.0));
    l += sprintf("mod_a(%s) %s", "1.0, 0n", mod_a(1.0, 0n));
    l += sprintf("mod_a(%s) %s", "1, -0.0", mod_a(1, -0.0));
    l += sprintf("mod_a(%s) %s", "\"1\", \"0\"", mod_a("1", "0"));
    l += sprintf("mod_a(%s) %s", "1, \"0\"", mod_a(1, "0"));
    l += sprintf("mod_a(%s) %s", "1, False", mod_a(1, False));
    l += sprintf("mod_a(%s) %s", "1, NULL", mod_a(1, NULL));
    l += sprintf("mod_a(%s) %s", "1, NOTHING", mod_a(1, NOTHING));
    l += sprintf("mod_a(%s) %s", "1, @nan@", mod_a(1, @nan@));
    l += sprintf("mod_a(%s) %s", "\"abc\", 0", mod_a("abc", 0));
    l += sprintf("mod_a(%s) %s", "1n, @nan@", mod_a(1n, @nan@));
    l += sprintf("mod_a(%s) %s", "@nan@, 0n", mod_a(@nan@, 0n));
    l += sprintf("mod_a(%s) %s", "7, 2.0", mod_a(7, 2.0));
    l += sprintf("mod_a(%s) %s", "7, 2n", mod_a(7, 2n));
    l += sprintf("mod_i(%s) %s", "1, 0", mod_i(1, 0));
    l += sprintf("mod_i(%s) %s", "0, 0", mod_i(0, 0));
    l += sprintf("mod_i(%s) %s", "-1, 0", mod_i(-1, 0));
    l += sprintf("mod_i(%s) %s", "MININT, 0", mod_i(MININT, 0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_13() {
    list<string> l = ();
    l += sprintf("mod_i(%s) %s", "MININT, -1", mod_i(MININT, -1));
    l += sprintf("mod_i(%s) %s", "MAXINT, -1", mod_i(MAXINT, -1));
    l += sprintf("mod_i(%s) %s", "7, -1", mod_i(7, -1));
    l += sprintf("mod_i(%s) %s", "7, 2", mod_i(7, 2));
    l += sprintf("mod_i(%s) %s", "-7, 2", mod_i(-7, 2));
    l += sprintf("mod_f(%s) %s", "5.5, 0.5", mod_f(5.5, 0.5));
    l += sprintf("mod_f(%s) %s", "1.0, 0.0", mod_f(1.0, 0.0));
    l += sprintf("mod_f(%s) %s", "1.0, -0.0", mod_f(1.0, -0.0));
    l += sprintf("mod_f(%s) %s", "1.0, @nan@", mod_f(1.0, @nan@));
    l += sprintf("mod_f(%s) %s", "@nan@, 1.0", mod_f(@nan@, 1.0));
    l += sprintf("mod_f(%s) %s", "@inf@, 2.0", mod_f(@inf@, 2.0));
    l += sprintf("mod_f(%s) %s", "7.9, 2.1", mod_f(7.9, 2.1));
    l += sprintf("mod_f(%s) %s", "1.0, @inf@", mod_f(1.0, @inf@));
    l += sprintf("mod_n(%s) %s", "5.5n, 0.5n", mod_n(5.5n, 0.5n));
    l += sprintf("mod_n(%s) %s", "1.0n, 0.0n", mod_n(1.0n, 0.0n));
    l += sprintf("mod_n(%s) %s", "1.0n, -0.0n", mod_n(1.0n, -0.0n));
    l += sprintf("mod_n(%s) %s", "1.0n, @nan@n", mod_n(1.0n, @nan@n));
    l += sprintf("mod_n(%s) %s", "@nan@n, 1.0n", mod_n(@nan@n, 1.0n));
    l += sprintf("mod_n(%s) %s", "@inf@n, 2.0n", mod_n(@inf@n, 2.0n));
    l += sprintf("mod_n(%s) %s", "7.9n, 2.1n", mod_n(7.9n, 2.1n));
    l += sprintf("mod_n(%s) %s", "1.0n, @inf@n", mod_n(1.0n, @inf@n));
    l += sprintf("mod_si(%s) %s", "1, 0", mod_si(1, 0));
    l += sprintf("mod_si(%s) %s", "1, 0.5", mod_si(1, 0.5));
    l += sprintf("mod_si(%s) %s", "1, @nan@", mod_si(1, @nan@));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_14() {
    list<string> l = ();
    l += sprintf("mod_si(%s) %s", "1, 0n", mod_si(1, 0n));
    l += sprintf("mod_sf(%s) %s", "1, 0", mod_sf(1, 0));
    l += sprintf("mod_sf(%s) %s", "1.0, 0.0", mod_sf(1.0, 0.0));
    l += sprintf("mod_sf(%s) %s", "1, @nan@", mod_sf(1, @nan@));
    l += sprintf("mod_sf(%s) %s", "1, 0n", mod_sf(1, 0n));
    l += sprintf("mod_sf(%s) %s", "1, \"0\"", mod_sf(1, "0"));
    l += sprintf("mod_sn(%s) %s", "1, 0", mod_sn(1, 0));
    l += sprintf("mod_sn(%s) %s", "1.0, 0.0", mod_sn(1.0, 0.0));
    l += sprintf("mod_sn(%s) %s", "0n, 0n", mod_sn(0n, 0n));
    l += sprintf("mod_sn(%s) %s", "1, @nan@", mod_sn(1, @nan@));
    l += sprintf("mod_sn(%s) %s", "1, \"0\"", mod_sn(1, "0"));
    l += sprintf("mod_if(%s) %s", "1, 0.0", mod_if(1, 0.0));
    l += sprintf("mod_if(%s) %s", "1, -0.0", mod_if(1, -0.0));
    l += sprintf("mod_if(%s) %s", "1, 0.5", mod_if(1, 0.5));
    l += sprintf("mod_if(%s) %s", "1, @nan@", mod_if(1, @nan@));
    l += sprintf("mod_fi(%s) %s", "1.0, 0", mod_fi(1.0, 0));
    l += sprintf("mod_fi(%s) %s", "0.0, 0", mod_fi(0.0, 0));
    l += sprintf("mod_fi(%s) %s", "@nan@, 0", mod_fi(@nan@, 0));
    l += sprintf("mod_fn(%s) %s", "1.0, 0n", mod_fn(1.0, 0n));
    l += sprintf("mod_fn(%s) %s", "0.0, 0n", mod_fn(0.0, 0n));
    l += sprintf("mod_fn(%s) %s", "@nan@, 0n", mod_fn(@nan@, 0n));
    l += sprintf("mod_nf(%s) %s", "1n, 0.0", mod_nf(1n, 0.0));
    l += sprintf("mod_nf(%s) %s", "0n, 0.0", mod_nf(0n, 0.0));
    l += sprintf("mod_nf(%s) %s", "@nan@n, 0.0", mod_nf(@nan@n, 0.0));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_15() {
    list<string> l = ();
    l += sprintf("mod_nf(%s) %s", "1n, -0.0", mod_nf(1n, -0.0));
    l += sprintf("mod_i0(1) %s", mod_i0(1));
    l += sprintf("mod_i0(0) %s", mod_i0(0));
    l += sprintf("mod_f0(1.0) %s", mod_f0(1.0));
    l += sprintf("mod_f0(0.0) %s", mod_f0(0.0));
    l += sprintf("mod_f0(@nan@) %s", mod_f0(@nan@));
    l += sprintf("mod_fm0(1.0) %s", mod_fm0(1.0));
    l += sprintf("mod_n0(1n) %s", mod_n0(1n));
    l += sprintf("mod_n0(0n) %s", mod_n0(0n));
    l += sprintf("mod_n0(@nan@n) %s", mod_n0(@nan@n));
    l += sprintf("mod_a0(1) %s", mod_a0(1));
    l += sprintf("mod_a0(1.0) %s", mod_a0(1.0));
    l += sprintf("mod_a0(1n) %s", mod_a0(1n));
    l += sprintf("mode_a(%s) %s", "5.5, 0.5", mode_a(5.5, 0.5));
    l += sprintf("mode_a(%s) %s", "1.0, 0.0", mode_a(1.0, 0.0));
    l += sprintf("mode_a(%s) %s", "1.0, -0.0", mode_a(1.0, -0.0));
    l += sprintf("mode_a(%s) %s", "1.0, @nan@", mode_a(1.0, @nan@));
    l += sprintf("mode_a(%s) %s", "@nan@, 1.0", mode_a(@nan@, 1.0));
    l += sprintf("mode_a(%s) %s", "@inf@, 2.0", mode_a(@inf@, 2.0));
    l += sprintf("mode_a(%s) %s", "7.9, 2.1", mode_a(7.9, 2.1));
    l += sprintf("mode_a(%s) %s", "1.0, @inf@", mode_a(1.0, @inf@));
    l += sprintf("mode_a(%s) %s", "5.5n, 0.5n", mode_a(5.5n, 0.5n));
    l += sprintf("mode_a(%s) %s", "1.0n, 0.0n", mode_a(1.0n, 0.0n));
    l += sprintf("mode_a(%s) %s", "1.0n, -0.0n", mode_a(1.0n, -0.0n));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_16() {
    list<string> l = ();
    l += sprintf("mode_a(%s) %s", "1.0n, @nan@n", mode_a(1.0n, @nan@n));
    l += sprintf("mode_a(%s) %s", "@nan@n, 1.0n", mode_a(@nan@n, 1.0n));
    l += sprintf("mode_a(%s) %s", "@inf@n, 2.0n", mode_a(@inf@n, 2.0n));
    l += sprintf("mode_a(%s) %s", "7.9n, 2.1n", mode_a(7.9n, 2.1n));
    l += sprintf("mode_a(%s) %s", "1.0n, @inf@n", mode_a(1.0n, @inf@n));
    l += sprintf("mode_a(%s) %s", "1, 0", mode_a(1, 0));
    l += sprintf("mode_a(%s) %s", "0, 0", mode_a(0, 0));
    l += sprintf("mode_a(%s) %s", "-1, 0", mode_a(-1, 0));
    l += sprintf("mode_a(%s) %s", "MININT, 0", mode_a(MININT, 0));
    l += sprintf("mode_a(%s) %s", "MININT, -1", mode_a(MININT, -1));
    l += sprintf("mode_a(%s) %s", "MAXINT, -1", mode_a(MAXINT, -1));
    l += sprintf("mode_a(%s) %s", "7, -1", mode_a(7, -1));
    l += sprintf("mode_a(%s) %s", "7, 2", mode_a(7, 2));
    l += sprintf("mode_a(%s) %s", "-7, 2", mode_a(-7, 2));
    l += sprintf("mode_a(%s) %s", "1, 0.0", mode_a(1, 0.0));
    l += sprintf("mode_a(%s) %s", "1.0, 0", mode_a(1.0, 0));
    l += sprintf("mode_a(%s) %s", "1n, 0", mode_a(1n, 0));
    l += sprintf("mode_a(%s) %s", "1, 0n", mode_a(1, 0n));
    l += sprintf("mode_a(%s) %s", "1n, 0.0", mode_a(1n, 0.0));
    l += sprintf("mode_a(%s) %s", "1.0, 0n", mode_a(1.0, 0n));
    l += sprintf("mode_a(%s) %s", "1, -0.0", mode_a(1, -0.0));
    l += sprintf("mode_a(%s) %s", "\"1\", \"0\"", mode_a("1", "0"));
    l += sprintf("mode_a(%s) %s", "1, \"0\"", mode_a(1, "0"));
    l += sprintf("mode_a(%s) %s", "1, False", mode_a(1, False));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_17() {
    list<string> l = ();
    l += sprintf("mode_a(%s) %s", "1, NULL", mode_a(1, NULL));
    l += sprintf("mode_a(%s) %s", "1, NOTHING", mode_a(1, NOTHING));
    l += sprintf("mode_a(%s) %s", "1, @nan@", mode_a(1, @nan@));
    l += sprintf("mode_a(%s) %s", "\"abc\", 0", mode_a("abc", 0));
    l += sprintf("mode_a(%s) %s", "1n, @nan@", mode_a(1n, @nan@));
    l += sprintf("mode_a(%s) %s", "@nan@, 0n", mode_a(@nan@, 0n));
    l += sprintf("mode_a(%s) %s", "7, 2.0", mode_a(7, 2.0));
    l += sprintf("mode_a(%s) %s", "7, 2n", mode_a(7, 2n));
    l += sprintf("mode_i(%s) %s", "1, 0", mode_i(1, 0));
    l += sprintf("mode_i(%s) %s", "0, 0", mode_i(0, 0));
    l += sprintf("mode_i(%s) %s", "-1, 0", mode_i(-1, 0));
    l += sprintf("mode_i(%s) %s", "MININT, 0", mode_i(MININT, 0));
    l += sprintf("mode_i(%s) %s", "MININT, -1", mode_i(MININT, -1));
    l += sprintf("mode_i(%s) %s", "MAXINT, -1", mode_i(MAXINT, -1));
    l += sprintf("mode_i(%s) %s", "7, -1", mode_i(7, -1));
    l += sprintf("mode_i(%s) %s", "7, 2", mode_i(7, 2));
    l += sprintf("mode_i(%s) %s", "-7, 2", mode_i(-7, 2));
    l += sprintf("mode_si(%s) %s", "1, 0", mode_si(1, 0));
    l += sprintf("mode_si(%s) %s", "1, 0.5", mode_si(1, 0.5));
    l += sprintf("mode_si(%s) %s", "1, @nan@", mode_si(1, @nan@));
    l += sprintf("mode_si(%s) %s", "1, 0n", mode_si(1, 0n));
    l += sprintf("mode_if(%s) %s", "1, 0.0", mode_if(1, 0.0));
    l += sprintf("mode_if(%s) %s", "1, -0.0", mode_if(1, -0.0));
    l += sprintf("mode_if(%s) %s", "1, 0.5", mode_if(1, 0.5));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes_18() {
    list<string> l = ();
    l += sprintf("mode_if(%s) %s", "1, @nan@", mode_if(1, @nan@));
    l += sprintf("mode_i0(1) %s", mode_i0(1));
    l += sprintf("mode_i0(0) %s", mode_i0(0));
    l += sprintf("mode_a0(1) %s", mode_a0(1));
    l += sprintf("mode_a0(1.0) %s", mode_a0(1.0));
    l += sprintf("mode_a0(1n) %s", mode_a0(1n));
    return foldl $1 + "\n" + $2, l;
}

string sub op_probes() {
    return op_probes_0() + "\n" + op_probes_1() + "\n" + op_probes_2() + "\n" + op_probes_3() + "\n" + op_probes_4() + "\n" + op_probes_5() + "\n" + op_probes_6() + "\n" + op_probes_7() + "\n" + op_probes_8() + "\n" + op_probes_9() + "\n" + op_probes_10() + "\n" + op_probes_11() + "\n" + op_probes_12() + "\n" + op_probes_13() + "\n" + op_probes_14() + "\n" + op_probes_15() + "\n" + op_probes_16() + "\n" + op_probes_17() + "\n" + op_probes_18();
}

string sub math_probes_0() {
    list<string> l = ();
    {
        float m0 = -1.0;
        try { l += sprintf("sqrt(-1.0) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-1.0) %s", e.err); }
    }
    try { l += sprintf("sqrt(-1.0) c %y", sqrt(-1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-1.0) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("sqrt(-@inf@) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-@inf@) %s", e.err); }
    }
    try { l += sprintf("sqrt(-@inf@) c %y", sqrt(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("sqrt(@nan@) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(@nan@) %s", e.err); }
    }
    try { l += sprintf("sqrt(@nan@) c %y", sqrt(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(@nan@) c %s", e.err); }
    {
        float m0 = -0.0;
        try { l += sprintf("sqrt(-0.0) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-0.0) %s", e.err); }
    }
    try { l += sprintf("sqrt(-0.0) c %y", sqrt(-0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-0.0) c %s", e.err); }
    {
        float m0 = 4.0;
        try { l += sprintf("sqrt(4.0) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(4.0) %s", e.err); }
    }
    try { l += sprintf("sqrt(4.0) c %y", sqrt(4.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(4.0) c %s", e.err); }
    {
        number m0 = -1.0n;
        try { l += sprintf("sqrt(-1.0n) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-1.0n) %s", e.err); }
    }
    try { l += sprintf("sqrt(-1.0n) c %y", sqrt(-1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-1.0n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("sqrt(-@inf@n) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("sqrt(-@inf@n) c %y", sqrt(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("sqrt(@nan@n) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(@nan@n) %s", e.err); }
    }
    try { l += sprintf("sqrt(@nan@n) c %y", sqrt(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(@nan@n) c %s", e.err); }
    {
        number m0 = -0.0n;
        try { l += sprintf("sqrt(-0.0n) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-0.0n) %s", e.err); }
    }
    try { l += sprintf("sqrt(-0.0n) c %y", sqrt(-0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(-0.0n) c %s", e.err); }
    {
        number m0 = 4.0n;
        try { l += sprintf("sqrt(4.0n) %y", sqrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(4.0n) %s", e.err); }
    }
    try { l += sprintf("sqrt(4.0n) c %y", sqrt(4.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sqrt(4.0n) c %s", e.err); }
    {
        float m0 = -8.0;
        try { l += sprintf("cbrt(-8.0) %y", cbrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(-8.0) %s", e.err); }
    }
    try { l += sprintf("cbrt(-8.0) c %y", cbrt(-8.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(-8.0) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("cbrt(@nan@) %y", cbrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(@nan@) %s", e.err); }
    }
    try { l += sprintf("cbrt(@nan@) c %y", cbrt(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(@nan@) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_1() {
    list<string> l = ();
    {
        number m0 = -8.0n;
        try { l += sprintf("cbrt(-8.0n) %y", cbrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(-8.0n) %s", e.err); }
    }
    try { l += sprintf("cbrt(-8.0n) c %y", cbrt(-8.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(-8.0n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("cbrt(@nan@n) %y", cbrt(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(@nan@n) %s", e.err); }
    }
    try { l += sprintf("cbrt(@nan@n) c %y", cbrt(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("cbrt(@nan@n) c %s", e.err); }
    {
        float m0 = @inf@;
        try { l += sprintf("sin(@inf@) %y", sin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@inf@) %s", e.err); }
    }
    try { l += sprintf("sin(@inf@) c %y", sin(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@inf@) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("sin(-@inf@) %y", sin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(-@inf@) %s", e.err); }
    }
    try { l += sprintf("sin(-@inf@) c %y", sin(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(-@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("sin(@nan@) %y", sin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@nan@) %s", e.err); }
    }
    try { l += sprintf("sin(@nan@) c %y", sin(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@nan@) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("sin(@inf@n) %y", sin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@inf@n) %s", e.err); }
    }
    try { l += sprintf("sin(@inf@n) c %y", sin(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@inf@n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("sin(-@inf@n) %y", sin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("sin(-@inf@n) c %y", sin(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(-@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("sin(@nan@n) %y", sin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@nan@n) %s", e.err); }
    }
    try { l += sprintf("sin(@nan@n) c %y", sin(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sin(@nan@n) c %s", e.err); }
    {
        float m0 = @inf@;
        try { l += sprintf("cos(@inf@) %y", cos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@inf@) %s", e.err); }
    }
    try { l += sprintf("cos(@inf@) c %y", cos(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("cos(@nan@) %y", cos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@nan@) %s", e.err); }
    }
    try { l += sprintf("cos(@nan@) c %y", cos(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@nan@) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("cos(@inf@n) %y", cos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@inf@n) %s", e.err); }
    }
    try { l += sprintf("cos(@inf@n) c %y", cos(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("cos(@nan@n) %y", cos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@nan@n) %s", e.err); }
    }
    try { l += sprintf("cos(@nan@n) c %y", cos(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("cos(@nan@n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_2() {
    list<string> l = ();
    {
        float m0 = @inf@;
        try { l += sprintf("tan(@inf@) %y", tan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@inf@) %s", e.err); }
    }
    try { l += sprintf("tan(@inf@) c %y", tan(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("tan(@nan@) %y", tan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@nan@) %s", e.err); }
    }
    try { l += sprintf("tan(@nan@) c %y", tan(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@nan@) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("tan(@inf@n) %y", tan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@inf@n) %s", e.err); }
    }
    try { l += sprintf("tan(@inf@n) c %y", tan(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("tan(@nan@n) %y", tan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@nan@n) %s", e.err); }
    }
    try { l += sprintf("tan(@nan@n) c %y", tan(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("tan(@nan@n) c %s", e.err); }
    {
        float m0 = 2.0;
        try { l += sprintf("asin(2.0) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(2.0) %s", e.err); }
    }
    try { l += sprintf("asin(2.0) c %y", asin(2.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(2.0) c %s", e.err); }
    {
        float m0 = -2.0;
        try { l += sprintf("asin(-2.0) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(-2.0) %s", e.err); }
    }
    try { l += sprintf("asin(-2.0) c %y", asin(-2.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(-2.0) c %s", e.err); }
    {
        float m0 = @inf@;
        try { l += sprintf("asin(@inf@) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@inf@) %s", e.err); }
    }
    try { l += sprintf("asin(@inf@) c %y", asin(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("asin(@nan@) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@nan@) %s", e.err); }
    }
    try { l += sprintf("asin(@nan@) c %y", asin(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@nan@) c %s", e.err); }
    {
        number m0 = 2.0n;
        try { l += sprintf("asin(2.0n) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(2.0n) %s", e.err); }
    }
    try { l += sprintf("asin(2.0n) c %y", asin(2.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(2.0n) c %s", e.err); }
    {
        number m0 = -2.0n;
        try { l += sprintf("asin(-2.0n) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(-2.0n) %s", e.err); }
    }
    try { l += sprintf("asin(-2.0n) c %y", asin(-2.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(-2.0n) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("asin(@inf@n) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@inf@n) %s", e.err); }
    }
    try { l += sprintf("asin(@inf@n) c %y", asin(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("asin(@nan@n) %y", asin(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@nan@n) %s", e.err); }
    }
    try { l += sprintf("asin(@nan@n) c %y", asin(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("asin(@nan@n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_3() {
    list<string> l = ();
    {
        float m0 = 2.0;
        try { l += sprintf("acos(2.0) %y", acos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(2.0) %s", e.err); }
    }
    try { l += sprintf("acos(2.0) c %y", acos(2.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(2.0) c %s", e.err); }
    {
        float m0 = -2.0;
        try { l += sprintf("acos(-2.0) %y", acos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(-2.0) %s", e.err); }
    }
    try { l += sprintf("acos(-2.0) c %y", acos(-2.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(-2.0) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("acos(@nan@) %y", acos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(@nan@) %s", e.err); }
    }
    try { l += sprintf("acos(@nan@) c %y", acos(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(@nan@) c %s", e.err); }
    {
        number m0 = 2.0n;
        try { l += sprintf("acos(2.0n) %y", acos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(2.0n) %s", e.err); }
    }
    try { l += sprintf("acos(2.0n) c %y", acos(2.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(2.0n) c %s", e.err); }
    {
        number m0 = -2.0n;
        try { l += sprintf("acos(-2.0n) %y", acos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(-2.0n) %s", e.err); }
    }
    try { l += sprintf("acos(-2.0n) c %y", acos(-2.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(-2.0n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("acos(@nan@n) %y", acos(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(@nan@n) %s", e.err); }
    }
    try { l += sprintf("acos(@nan@n) c %y", acos(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("acos(@nan@n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("atan(@nan@) %y", atan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@nan@) %s", e.err); }
    }
    try { l += sprintf("atan(@nan@) c %y", atan(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@nan@) c %s", e.err); }
    {
        float m0 = @inf@;
        try { l += sprintf("atan(@inf@) %y", atan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@inf@) %s", e.err); }
    }
    try { l += sprintf("atan(@inf@) c %y", atan(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@inf@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("atan(@nan@n) %y", atan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@nan@n) %s", e.err); }
    }
    try { l += sprintf("atan(@nan@n) c %y", atan(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@nan@n) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("atan(@inf@n) %y", atan(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@inf@n) %s", e.err); }
    }
    try { l += sprintf("atan(@inf@n) c %y", atan(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan(@inf@n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("sinh(@nan@) %y", sinh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(@nan@) %s", e.err); }
    }
    try { l += sprintf("sinh(@nan@) c %y", sinh(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(@nan@) c %s", e.err); }
    {
        float m0 = 1000.0;
        try { l += sprintf("sinh(1000.0) %y", sinh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(1000.0) %s", e.err); }
    }
    try { l += sprintf("sinh(1000.0) c %y", sinh(1000.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(1000.0) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_4() {
    list<string> l = ();
    {
        number m0 = @nan@n;
        try { l += sprintf("sinh(@nan@n) %y", sinh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(@nan@n) %s", e.err); }
    }
    try { l += sprintf("sinh(@nan@n) c %y", sinh(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(@nan@n) c %s", e.err); }
    {
        number m0 = 1000.0n;
        try { l += sprintf("sinh(1000.0n) %y", sinh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(1000.0n) %s", e.err); }
    }
    try { l += sprintf("sinh(1000.0n) c %y", sinh(1000.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("sinh(1000.0n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("cosh(@nan@) %y", cosh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(@nan@) %s", e.err); }
    }
    try { l += sprintf("cosh(@nan@) c %y", cosh(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(@nan@) c %s", e.err); }
    {
        float m0 = 1000.0;
        try { l += sprintf("cosh(1000.0) %y", cosh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(1000.0) %s", e.err); }
    }
    try { l += sprintf("cosh(1000.0) c %y", cosh(1000.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(1000.0) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("cosh(@nan@n) %y", cosh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(@nan@n) %s", e.err); }
    }
    try { l += sprintf("cosh(@nan@n) c %y", cosh(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(@nan@n) c %s", e.err); }
    {
        number m0 = 1000.0n;
        try { l += sprintf("cosh(1000.0n) %y", cosh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(1000.0n) %s", e.err); }
    }
    try { l += sprintf("cosh(1000.0n) c %y", cosh(1000.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("cosh(1000.0n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("tanh(@nan@) %y", tanh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("tanh(@nan@) %s", e.err); }
    }
    try { l += sprintf("tanh(@nan@) c %y", tanh(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("tanh(@nan@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("tanh(@nan@n) %y", tanh(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("tanh(@nan@n) %s", e.err); }
    }
    try { l += sprintf("tanh(@nan@n) c %y", tanh(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("tanh(@nan@n) c %s", e.err); }
    {
        float m0 = -1.0;
        try { l += sprintf("nlog(-1.0) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-1.0) %s", e.err); }
    }
    try { l += sprintf("nlog(-1.0) c %y", nlog(-1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-1.0) c %s", e.err); }
    {
        float m0 = 0.0;
        try { l += sprintf("nlog(0.0) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(0.0) %s", e.err); }
    }
    try { l += sprintf("nlog(0.0) c %y", nlog(0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(0.0) c %s", e.err); }
    {
        float m0 = -0.0;
        try { l += sprintf("nlog(-0.0) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-0.0) %s", e.err); }
    }
    try { l += sprintf("nlog(-0.0) c %y", nlog(-0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-0.0) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("nlog(-@inf@) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-@inf@) %s", e.err); }
    }
    try { l += sprintf("nlog(-@inf@) c %y", nlog(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-@inf@) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_5() {
    list<string> l = ();
    {
        float m0 = @nan@;
        try { l += sprintf("nlog(@nan@) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(@nan@) %s", e.err); }
    }
    try { l += sprintf("nlog(@nan@) c %y", nlog(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(@nan@) c %s", e.err); }
    {
        number m0 = -1.0n;
        try { l += sprintf("nlog(-1.0n) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-1.0n) %s", e.err); }
    }
    try { l += sprintf("nlog(-1.0n) c %y", nlog(-1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-1.0n) c %s", e.err); }
    {
        number m0 = 0.0n;
        try { l += sprintf("nlog(0.0n) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(0.0n) %s", e.err); }
    }
    try { l += sprintf("nlog(0.0n) c %y", nlog(0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(0.0n) c %s", e.err); }
    {
        number m0 = -0.0n;
        try { l += sprintf("nlog(-0.0n) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-0.0n) %s", e.err); }
    }
    try { l += sprintf("nlog(-0.0n) c %y", nlog(-0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-0.0n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("nlog(-@inf@n) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("nlog(-@inf@n) c %y", nlog(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(-@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("nlog(@nan@n) %y", nlog(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(@nan@n) %s", e.err); }
    }
    try { l += sprintf("nlog(@nan@n) c %y", nlog(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("nlog(@nan@n) c %s", e.err); }
    {
        float m0 = -1.0;
        try { l += sprintf("log10(-1.0) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-1.0) %s", e.err); }
    }
    try { l += sprintf("log10(-1.0) c %y", log10(-1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-1.0) c %s", e.err); }
    {
        float m0 = 0.0;
        try { l += sprintf("log10(0.0) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(0.0) %s", e.err); }
    }
    try { l += sprintf("log10(0.0) c %y", log10(0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(0.0) c %s", e.err); }
    {
        float m0 = -0.0;
        try { l += sprintf("log10(-0.0) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-0.0) %s", e.err); }
    }
    try { l += sprintf("log10(-0.0) c %y", log10(-0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-0.0) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("log10(-@inf@) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-@inf@) %s", e.err); }
    }
    try { l += sprintf("log10(-@inf@) c %y", log10(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("log10(@nan@) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(@nan@) %s", e.err); }
    }
    try { l += sprintf("log10(@nan@) c %y", log10(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(@nan@) c %s", e.err); }
    {
        number m0 = -1.0n;
        try { l += sprintf("log10(-1.0n) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-1.0n) %s", e.err); }
    }
    try { l += sprintf("log10(-1.0n) c %y", log10(-1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-1.0n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_6() {
    list<string> l = ();
    {
        number m0 = 0.0n;
        try { l += sprintf("log10(0.0n) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(0.0n) %s", e.err); }
    }
    try { l += sprintf("log10(0.0n) c %y", log10(0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(0.0n) c %s", e.err); }
    {
        number m0 = -0.0n;
        try { l += sprintf("log10(-0.0n) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-0.0n) %s", e.err); }
    }
    try { l += sprintf("log10(-0.0n) c %y", log10(-0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-0.0n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("log10(-@inf@n) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("log10(-@inf@n) c %y", log10(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(-@inf@n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("log10(@nan@n) %y", log10(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(@nan@n) %s", e.err); }
    }
    try { l += sprintf("log10(@nan@n) c %y", log10(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log10(@nan@n) c %s", e.err); }
    {
        float m0 = -2.0;
        try { l += sprintf("log1p(-2.0) %y", log1p(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-2.0) %s", e.err); }
    }
    try { l += sprintf("log1p(-2.0) c %y", log1p(-2.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-2.0) c %s", e.err); }
    {
        float m0 = -1.0;
        try { l += sprintf("log1p(-1.0) %y", log1p(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-1.0) %s", e.err); }
    }
    try { l += sprintf("log1p(-1.0) c %y", log1p(-1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-1.0) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("log1p(@nan@) %y", log1p(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(@nan@) %s", e.err); }
    }
    try { l += sprintf("log1p(@nan@) c %y", log1p(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(@nan@) c %s", e.err); }
    {
        number m0 = -2.0n;
        try { l += sprintf("log1p(-2.0n) %y", log1p(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-2.0n) %s", e.err); }
    }
    try { l += sprintf("log1p(-2.0n) c %y", log1p(-2.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-2.0n) c %s", e.err); }
    {
        number m0 = -1.0n;
        try { l += sprintf("log1p(-1.0n) %y", log1p(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-1.0n) %s", e.err); }
    }
    try { l += sprintf("log1p(-1.0n) c %y", log1p(-1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(-1.0n) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("log1p(@nan@n) %y", log1p(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(@nan@n) %s", e.err); }
    }
    try { l += sprintf("log1p(@nan@n) c %y", log1p(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("log1p(@nan@n) c %s", e.err); }
    {
        float m0 = 0.0;
        try { l += sprintf("logb(0.0) %y", logb(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(0.0) %s", e.err); }
    }
    try { l += sprintf("logb(0.0) c %y", logb(0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(0.0) c %s", e.err); }
    {
        float m0 = -0.0;
        try { l += sprintf("logb(-0.0) %y", logb(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(-0.0) %s", e.err); }
    }
    try { l += sprintf("logb(-0.0) c %y", logb(-0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(-0.0) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_7() {
    list<string> l = ();
    {
        float m0 = @inf@;
        try { l += sprintf("logb(@inf@) %y", logb(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(@inf@) %s", e.err); }
    }
    try { l += sprintf("logb(@inf@) c %y", logb(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(@inf@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("logb(@nan@) %y", logb(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(@nan@) %s", e.err); }
    }
    try { l += sprintf("logb(@nan@) c %y", logb(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("logb(@nan@) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("exp(@nan@) %y", exp(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(@nan@) %s", e.err); }
    }
    try { l += sprintf("exp(@nan@) c %y", exp(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(@nan@) c %s", e.err); }
    {
        float m0 = 1000.0;
        try { l += sprintf("exp(1000.0) %y", exp(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(1000.0) %s", e.err); }
    }
    try { l += sprintf("exp(1000.0) c %y", exp(1000.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(1000.0) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("exp(-@inf@) %y", exp(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(-@inf@) %s", e.err); }
    }
    try { l += sprintf("exp(-@inf@) c %y", exp(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(-@inf@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("exp(@nan@n) %y", exp(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(@nan@n) %s", e.err); }
    }
    try { l += sprintf("exp(@nan@n) c %y", exp(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(@nan@n) c %s", e.err); }
    {
        number m0 = 1000.0n;
        try { l += sprintf("exp(1000.0n) %y", exp(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(1000.0n) %s", e.err); }
    }
    try { l += sprintf("exp(1000.0n) c %y", exp(1000.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(1000.0n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("exp(-@inf@n) %y", exp(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("exp(-@inf@n) c %y", exp(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp(-@inf@n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("exp2(@nan@) %y", exp2(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(@nan@) %s", e.err); }
    }
    try { l += sprintf("exp2(@nan@) c %y", exp2(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(@nan@) c %s", e.err); }
    {
        float m0 = 2000.0;
        try { l += sprintf("exp2(2000.0) %y", exp2(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(2000.0) %s", e.err); }
    }
    try { l += sprintf("exp2(2000.0) c %y", exp2(2000.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(2000.0) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("exp2(@nan@n) %y", exp2(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(@nan@n) %s", e.err); }
    }
    try { l += sprintf("exp2(@nan@n) c %y", exp2(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(@nan@n) c %s", e.err); }
    {
        number m0 = 2000.0n;
        try { l += sprintf("exp2(2000.0n) %y", exp2(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(2000.0n) %s", e.err); }
    }
    try { l += sprintf("exp2(2000.0n) c %y", exp2(2000.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("exp2(2000.0n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_8() {
    list<string> l = ();
    {
        float m0 = @nan@;
        try { l += sprintf("expm1(@nan@) %y", expm1(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(@nan@) %s", e.err); }
    }
    try { l += sprintf("expm1(@nan@) c %y", expm1(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(@nan@) c %s", e.err); }
    {
        float m0 = 1000.0;
        try { l += sprintf("expm1(1000.0) %y", expm1(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(1000.0) %s", e.err); }
    }
    try { l += sprintf("expm1(1000.0) c %y", expm1(1000.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(1000.0) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("expm1(@nan@n) %y", expm1(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(@nan@n) %s", e.err); }
    }
    try { l += sprintf("expm1(@nan@n) c %y", expm1(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(@nan@n) c %s", e.err); }
    {
        number m0 = 1000.0n;
        try { l += sprintf("expm1(1000.0n) %y", expm1(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(1000.0n) %s", e.err); }
    }
    try { l += sprintf("expm1(1000.0n) c %y", expm1(1000.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("expm1(1000.0n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("abs(@nan@) %y", abs(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(@nan@) %s", e.err); }
    }
    try { l += sprintf("abs(@nan@) c %y", abs(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(@nan@) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("abs(-@inf@) %y", abs(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(-@inf@) %s", e.err); }
    }
    try { l += sprintf("abs(-@inf@) c %y", abs(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(-@inf@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("abs(@nan@n) %y", abs(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(@nan@n) %s", e.err); }
    }
    try { l += sprintf("abs(@nan@n) c %y", abs(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(@nan@n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("abs(-@inf@n) %y", abs(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("abs(-@inf@n) c %y", abs(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("abs(-@inf@n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("round(@nan@) %y", round(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@nan@) %s", e.err); }
    }
    try { l += sprintf("round(@nan@) c %y", round(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@nan@) c %s", e.err); }
    {
        float m0 = @inf@;
        try { l += sprintf("round(@inf@) %y", round(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@inf@) %s", e.err); }
    }
    try { l += sprintf("round(@inf@) c %y", round(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@inf@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("round(@nan@n) %y", round(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@nan@n) %s", e.err); }
    }
    try { l += sprintf("round(@nan@n) c %y", round(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@nan@n) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("round(@inf@n) %y", round(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@inf@n) %s", e.err); }
    }
    try { l += sprintf("round(@inf@n) c %y", round(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("round(@inf@n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_9() {
    list<string> l = ();
    {
        float m0 = @nan@;
        try { l += sprintf("ceil(@nan@) %y", ceil(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(@nan@) %s", e.err); }
    }
    try { l += sprintf("ceil(@nan@) c %y", ceil(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(@nan@) c %s", e.err); }
    {
        float m0 = -@inf@;
        try { l += sprintf("ceil(-@inf@) %y", ceil(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(-@inf@) %s", e.err); }
    }
    try { l += sprintf("ceil(-@inf@) c %y", ceil(-@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(-@inf@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("ceil(@nan@n) %y", ceil(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(@nan@n) %s", e.err); }
    }
    try { l += sprintf("ceil(@nan@n) c %y", ceil(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(@nan@n) c %s", e.err); }
    {
        number m0 = -@inf@n;
        try { l += sprintf("ceil(-@inf@n) %y", ceil(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(-@inf@n) %s", e.err); }
    }
    try { l += sprintf("ceil(-@inf@n) c %y", ceil(-@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("ceil(-@inf@n) c %s", e.err); }
    {
        float m0 = @nan@;
        try { l += sprintf("floor(@nan@) %y", floor(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@nan@) %s", e.err); }
    }
    try { l += sprintf("floor(@nan@) c %y", floor(@nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@nan@) c %s", e.err); }
    {
        float m0 = @inf@;
        try { l += sprintf("floor(@inf@) %y", floor(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@inf@) %s", e.err); }
    }
    try { l += sprintf("floor(@inf@) c %y", floor(@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@inf@) c %s", e.err); }
    {
        number m0 = @nan@n;
        try { l += sprintf("floor(@nan@n) %y", floor(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@nan@n) %s", e.err); }
    }
    try { l += sprintf("floor(@nan@n) c %y", floor(@nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@nan@n) c %s", e.err); }
    {
        number m0 = @inf@n;
        try { l += sprintf("floor(@inf@n) %y", floor(m0)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@inf@n) %s", e.err); }
    }
    try { l += sprintf("floor(@inf@n) c %y", floor(@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("floor(@inf@n) c %s", e.err); }
    {
        float m0 = -1.0; float m1 = 0.5;
        try { l += sprintf("pow(-1.0, 0.5) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0, 0.5) %s", e.err); }
    }
    try { l += sprintf("pow(-1.0, 0.5) c %y", pow(-1.0, 0.5)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0, 0.5) c %s", e.err); }
    {
        float m0 = 0.0; float m1 = -1.0;
        try { l += sprintf("pow(0.0, -1.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0, -1.0) %s", e.err); }
    }
    try { l += sprintf("pow(0.0, -1.0) c %y", pow(0.0, -1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0, -1.0) c %s", e.err); }
    {
        float m0 = -0.0; float m1 = -1.0;
        try { l += sprintf("pow(-0.0, -1.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-0.0, -1.0) %s", e.err); }
    }
    try { l += sprintf("pow(-0.0, -1.0) c %y", pow(-0.0, -1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-0.0, -1.0) c %s", e.err); }
    {
        float m0 = 0.0; float m1 = -0.5;
        try { l += sprintf("pow(0.0, -0.5) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0, -0.5) %s", e.err); }
    }
    try { l += sprintf("pow(0.0, -0.5) c %y", pow(0.0, -0.5)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0, -0.5) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_10() {
    list<string> l = ();
    {
        float m0 = -8.0; float m1 = 0.5;
        try { l += sprintf("pow(-8.0, 0.5) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-8.0, 0.5) %s", e.err); }
    }
    try { l += sprintf("pow(-8.0, 0.5) c %y", pow(-8.0, 0.5)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-8.0, 0.5) c %s", e.err); }
    {
        float m0 = -1.0; float m1 = @nan@;
        try { l += sprintf("pow(-1.0, @nan@) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0, @nan@) %s", e.err); }
    }
    try { l += sprintf("pow(-1.0, @nan@) c %y", pow(-1.0, @nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0, @nan@) c %s", e.err); }
    {
        float m0 = @nan@; float m1 = 0.0;
        try { l += sprintf("pow(@nan@, 0.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(@nan@, 0.0) %s", e.err); }
    }
    try { l += sprintf("pow(@nan@, 0.0) c %y", pow(@nan@, 0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(@nan@, 0.0) c %s", e.err); }
    {
        float m0 = -@inf@; float m1 = 0.5;
        try { l += sprintf("pow(-@inf@, 0.5) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-@inf@, 0.5) %s", e.err); }
    }
    try { l += sprintf("pow(-@inf@, 0.5) c %y", pow(-@inf@, 0.5)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-@inf@, 0.5) c %s", e.err); }
    {
        float m0 = 0.0; float m1 = -@inf@;
        try { l += sprintf("pow(0.0, -@inf@) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0, -@inf@) %s", e.err); }
    }
    try { l += sprintf("pow(0.0, -@inf@) c %y", pow(0.0, -@inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0, -@inf@) c %s", e.err); }
    {
        float m0 = -1.0; float m1 = @inf@;
        try { l += sprintf("pow(-1.0, @inf@) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0, @inf@) %s", e.err); }
    }
    try { l += sprintf("pow(-1.0, @inf@) c %y", pow(-1.0, @inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0, @inf@) c %s", e.err); }
    {
        float m0 = 1.0; float m1 = @nan@;
        try { l += sprintf("pow(1.0, @nan@) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(1.0, @nan@) %s", e.err); }
    }
    try { l += sprintf("pow(1.0, @nan@) c %y", pow(1.0, @nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(1.0, @nan@) c %s", e.err); }
    {
        float m0 = 2.0; float m1 = 2000.0;
        try { l += sprintf("pow(2.0, 2000.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, 2000.0) %s", e.err); }
    }
    try { l += sprintf("pow(2.0, 2000.0) c %y", pow(2.0, 2000.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, 2000.0) c %s", e.err); }
    {
        float m0 = -2.0; float m1 = 3.0;
        try { l += sprintf("pow(-2.0, 3.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-2.0, 3.0) %s", e.err); }
    }
    try { l += sprintf("pow(-2.0, 3.0) c %y", pow(-2.0, 3.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-2.0, 3.0) c %s", e.err); }
    {
        float m0 = 2.0; float m1 = 0.5;
        try { l += sprintf("pow(2.0, 0.5) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, 0.5) %s", e.err); }
    }
    try { l += sprintf("pow(2.0, 0.5) c %y", pow(2.0, 0.5)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, 0.5) c %s", e.err); }
    {
        float m0 = 2.0; float m1 = -2.0;
        try { l += sprintf("pow(2.0, -2.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, -2.0) %s", e.err); }
    }
    try { l += sprintf("pow(2.0, -2.0) c %y", pow(2.0, -2.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, -2.0) c %s", e.err); }
    {
        float m0 = 2.0; float m1 = -0.5;
        try { l += sprintf("pow(2.0, -0.5) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, -0.5) %s", e.err); }
    }
    try { l += sprintf("pow(2.0, -0.5) c %y", pow(2.0, -0.5)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, -0.5) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_11() {
    list<string> l = ();
    {
        float m0 = 2.0; float m1 = 1e30;
        try { l += sprintf("pow(2.0, 1e30) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, 1e30) %s", e.err); }
    }
    try { l += sprintf("pow(2.0, 1e30) c %y", pow(2.0, 1e30)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, 1e30) c %s", e.err); }
    {
        float m0 = 2.0; float m1 = -9223372036854775808.0;
        try { l += sprintf("pow(2.0, -9223372036854775808.0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, -9223372036854775808.0) %s", e.err); }
    }
    try { l += sprintf("pow(2.0, -9223372036854775808.0) c %y", pow(2.0, -9223372036854775808.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0, -9223372036854775808.0) c %s", e.err); }
    {
        number m0 = -1.0n; number m1 = 0.5n;
        try { l += sprintf("pow(-1.0n, 0.5n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0n, 0.5n) %s", e.err); }
    }
    try { l += sprintf("pow(-1.0n, 0.5n) c %y", pow(-1.0n, 0.5n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0n, 0.5n) c %s", e.err); }
    {
        number m0 = 0.0n; number m1 = -1.0n;
        try { l += sprintf("pow(0.0n, -1.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0n, -1.0n) %s", e.err); }
    }
    try { l += sprintf("pow(0.0n, -1.0n) c %y", pow(0.0n, -1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0n, -1.0n) c %s", e.err); }
    {
        number m0 = -0.0n; number m1 = -1.0n;
        try { l += sprintf("pow(-0.0n, -1.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-0.0n, -1.0n) %s", e.err); }
    }
    try { l += sprintf("pow(-0.0n, -1.0n) c %y", pow(-0.0n, -1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-0.0n, -1.0n) c %s", e.err); }
    {
        number m0 = 0.0n; number m1 = -0.5n;
        try { l += sprintf("pow(0.0n, -0.5n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0n, -0.5n) %s", e.err); }
    }
    try { l += sprintf("pow(0.0n, -0.5n) c %y", pow(0.0n, -0.5n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0n, -0.5n) c %s", e.err); }
    {
        number m0 = -8.0n; number m1 = 0.5n;
        try { l += sprintf("pow(-8.0n, 0.5n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-8.0n, 0.5n) %s", e.err); }
    }
    try { l += sprintf("pow(-8.0n, 0.5n) c %y", pow(-8.0n, 0.5n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-8.0n, 0.5n) c %s", e.err); }
    {
        number m0 = -1.0n; number m1 = @nan@n;
        try { l += sprintf("pow(-1.0n, @nan@n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0n, @nan@n) %s", e.err); }
    }
    try { l += sprintf("pow(-1.0n, @nan@n) c %y", pow(-1.0n, @nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0n, @nan@n) c %s", e.err); }
    {
        number m0 = @nan@n; number m1 = 0.0n;
        try { l += sprintf("pow(@nan@n, 0.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(@nan@n, 0.0n) %s", e.err); }
    }
    try { l += sprintf("pow(@nan@n, 0.0n) c %y", pow(@nan@n, 0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(@nan@n, 0.0n) c %s", e.err); }
    {
        number m0 = -@inf@n; number m1 = 0.5n;
        try { l += sprintf("pow(-@inf@n, 0.5n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-@inf@n, 0.5n) %s", e.err); }
    }
    try { l += sprintf("pow(-@inf@n, 0.5n) c %y", pow(-@inf@n, 0.5n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-@inf@n, 0.5n) c %s", e.err); }
    {
        number m0 = 0.0n; number m1 = -@inf@n;
        try { l += sprintf("pow(0.0n, -@inf@n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0n, -@inf@n) %s", e.err); }
    }
    try { l += sprintf("pow(0.0n, -@inf@n) c %y", pow(0.0n, -@inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0.0n, -@inf@n) c %s", e.err); }
    {
        number m0 = -1.0n; number m1 = @inf@n;
        try { l += sprintf("pow(-1.0n, @inf@n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0n, @inf@n) %s", e.err); }
    }
    try { l += sprintf("pow(-1.0n, @inf@n) c %y", pow(-1.0n, @inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1.0n, @inf@n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_12() {
    list<string> l = ();
    {
        number m0 = 1.0n; number m1 = @nan@n;
        try { l += sprintf("pow(1.0n, @nan@n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(1.0n, @nan@n) %s", e.err); }
    }
    try { l += sprintf("pow(1.0n, @nan@n) c %y", pow(1.0n, @nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(1.0n, @nan@n) c %s", e.err); }
    {
        number m0 = 2.0n; number m1 = 2000.0n;
        try { l += sprintf("pow(2.0n, 2000.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, 2000.0n) %s", e.err); }
    }
    try { l += sprintf("pow(2.0n, 2000.0n) c %y", pow(2.0n, 2000.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, 2000.0n) c %s", e.err); }
    {
        number m0 = -2.0n; number m1 = 3.0n;
        try { l += sprintf("pow(-2.0n, 3.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-2.0n, 3.0n) %s", e.err); }
    }
    try { l += sprintf("pow(-2.0n, 3.0n) c %y", pow(-2.0n, 3.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-2.0n, 3.0n) c %s", e.err); }
    {
        number m0 = 2.0n; number m1 = 0.5n;
        try { l += sprintf("pow(2.0n, 0.5n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, 0.5n) %s", e.err); }
    }
    try { l += sprintf("pow(2.0n, 0.5n) c %y", pow(2.0n, 0.5n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, 0.5n) c %s", e.err); }
    {
        number m0 = 2.0n; number m1 = -2.0n;
        try { l += sprintf("pow(2.0n, -2.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, -2.0n) %s", e.err); }
    }
    try { l += sprintf("pow(2.0n, -2.0n) c %y", pow(2.0n, -2.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, -2.0n) c %s", e.err); }
    {
        number m0 = 2.0n; number m1 = -0.5n;
        try { l += sprintf("pow(2.0n, -0.5n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, -0.5n) %s", e.err); }
    }
    try { l += sprintf("pow(2.0n, -0.5n) c %y", pow(2.0n, -0.5n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, -0.5n) c %s", e.err); }
    {
        number m0 = 2.0n; number m1 = 1e30n;
        try { l += sprintf("pow(2.0n, 1e30n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, 1e30n) %s", e.err); }
    }
    try { l += sprintf("pow(2.0n, 1e30n) c %y", pow(2.0n, 1e30n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, 1e30n) c %s", e.err); }
    {
        number m0 = 2.0n; number m1 = -9223372036854775808.0n;
        try { l += sprintf("pow(2.0n, -9223372036854775808.0n) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, -9223372036854775808.0n) %s", e.err); }
    }
    try { l += sprintf("pow(2.0n, -9223372036854775808.0n) c %y", pow(2.0n, -9223372036854775808.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2.0n, -9223372036854775808.0n) c %s", e.err); }
    {
        float m0 = 0.0; float m1 = 0.0;
        try { l += sprintf("atan2(0.0, 0.0) %y", atan2(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(0.0, 0.0) %s", e.err); }
    }
    try { l += sprintf("atan2(0.0, 0.0) c %y", atan2(0.0, 0.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(0.0, 0.0) c %s", e.err); }
    {
        float m0 = @nan@; float m1 = 1.0;
        try { l += sprintf("atan2(@nan@, 1.0) %y", atan2(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@nan@, 1.0) %s", e.err); }
    }
    try { l += sprintf("atan2(@nan@, 1.0) c %y", atan2(@nan@, 1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@nan@, 1.0) c %s", e.err); }
    {
        float m0 = @inf@; float m1 = @inf@;
        try { l += sprintf("atan2(@inf@, @inf@) %y", atan2(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@inf@, @inf@) %s", e.err); }
    }
    try { l += sprintf("atan2(@inf@, @inf@) c %y", atan2(@inf@, @inf@)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@inf@, @inf@) c %s", e.err); }
    {
        number m0 = 0.0n; number m1 = 0.0n;
        try { l += sprintf("atan2(0.0n, 0.0n) %y", atan2(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(0.0n, 0.0n) %s", e.err); }
    }
    try { l += sprintf("atan2(0.0n, 0.0n) c %y", atan2(0.0n, 0.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(0.0n, 0.0n) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes_13() {
    list<string> l = ();
    {
        number m0 = @nan@n; number m1 = 1.0n;
        try { l += sprintf("atan2(@nan@n, 1.0n) %y", atan2(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@nan@n, 1.0n) %s", e.err); }
    }
    try { l += sprintf("atan2(@nan@n, 1.0n) c %y", atan2(@nan@n, 1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@nan@n, 1.0n) c %s", e.err); }
    {
        number m0 = @inf@n; number m1 = @inf@n;
        try { l += sprintf("atan2(@inf@n, @inf@n) %y", atan2(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@inf@n, @inf@n) %s", e.err); }
    }
    try { l += sprintf("atan2(@inf@n, @inf@n) c %y", atan2(@inf@n, @inf@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("atan2(@inf@n, @inf@n) c %s", e.err); }
    {
        float m0 = @inf@; float m1 = @nan@;
        try { l += sprintf("hypot(@inf@, @nan@) %y", hypot(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@inf@, @nan@) %s", e.err); }
    }
    try { l += sprintf("hypot(@inf@, @nan@) c %y", hypot(@inf@, @nan@)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@inf@, @nan@) c %s", e.err); }
    {
        float m0 = @nan@; float m1 = 1.0;
        try { l += sprintf("hypot(@nan@, 1.0) %y", hypot(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@nan@, 1.0) %s", e.err); }
    }
    try { l += sprintf("hypot(@nan@, 1.0) c %y", hypot(@nan@, 1.0)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@nan@, 1.0) c %s", e.err); }
    {
        number m0 = @inf@n; number m1 = @nan@n;
        try { l += sprintf("hypot(@inf@n, @nan@n) %y", hypot(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@inf@n, @nan@n) %s", e.err); }
    }
    try { l += sprintf("hypot(@inf@n, @nan@n) c %y", hypot(@inf@n, @nan@n)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@inf@n, @nan@n) c %s", e.err); }
    {
        number m0 = @nan@n; number m1 = 1.0n;
        try { l += sprintf("hypot(@nan@n, 1.0n) %y", hypot(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@nan@n, 1.0n) %s", e.err); }
    }
    try { l += sprintf("hypot(@nan@n, 1.0n) c %y", hypot(@nan@n, 1.0n)); } catch (hash<ExceptionInfo> e) { l += sprintf("hypot(@nan@n, 1.0n) c %s", e.err); }
    {
        int m0 = 0; int m1 = -1;
        try { l += sprintf("pow(0, -1) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0, -1) %s", e.err); }
    }
    try { l += sprintf("pow(0, -1) c %y", pow(0, -1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0, -1) c %s", e.err); }
    {
        int m0 = 0; int m1 = -2;
        try { l += sprintf("pow(0, -2) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0, -2) %s", e.err); }
    }
    try { l += sprintf("pow(0, -2) c %y", pow(0, -2)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0, -2) c %s", e.err); }
    {
        int m0 = 2; int m1 = -1;
        try { l += sprintf("pow(2, -1) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2, -1) %s", e.err); }
    }
    try { l += sprintf("pow(2, -1) c %y", pow(2, -1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2, -1) c %s", e.err); }
    {
        int m0 = -1; int m1 = -1;
        try { l += sprintf("pow(-1, -1) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1, -1) %s", e.err); }
    }
    try { l += sprintf("pow(-1, -1) c %y", pow(-1, -1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(-1, -1) c %s", e.err); }
    {
        int m0 = 0; int m1 = 0;
        try { l += sprintf("pow(0, 0) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0, 0) %s", e.err); }
    }
    try { l += sprintf("pow(0, 0) c %y", pow(0, 0)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0, 0) c %s", e.err); }
    {
        int m0 = 2; int m1 = 70;
        try { l += sprintf("pow(2, 70) %y", pow(m0, m1)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2, 70) %s", e.err); }
    }
    try { l += sprintf("pow(2, 70) c %y", pow(2, 70)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(2, 70) c %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub math_probes() {
    return math_probes_0() + "\n" + math_probes_1() + "\n" + math_probes_2() + "\n" + math_probes_3() + "\n" + math_probes_4() + "\n" + math_probes_5() + "\n" + math_probes_6() + "\n" + math_probes_7() + "\n" + math_probes_8() + "\n" + math_probes_9() + "\n" + math_probes_10() + "\n" + math_probes_11() + "\n" + math_probes_12() + "\n" + math_probes_13();
}

string sub flag_probes() {
    list<string> l = ();
    number z = 0n;
    number m = -1n;
    number two = 2n;
    number four = 4n;
    l += sprintf("nlog(0n) %y", nlog(z));
    try { l += sprintf("after nlog(0n): 4n / 2n %y", four / two); } catch (hash<ExceptionInfo> e) { l += sprintf("after nlog(0n): 4n / 2n %s", e.err); }
    try { l += sprintf("pow(0n, -1n) %y", pow(z, m)); } catch (hash<ExceptionInfo> e) { l += sprintf("pow(0n, -1n) %s", e.err); }
    try { l += sprintf("after pow(0n, -1n): 4n / 2n %y", four / two); } catch (hash<ExceptionInfo> e) { l += sprintf("after pow(0n, -1n): 4n / 2n %s", e.err); }
    number nan = @nan@n;
    l += sprintf("nan < 1n %y", nan < 1n);
    try { l += sprintf("nan <=> 1n %y", nan <=> 1n); } catch (hash<ExceptionInfo> e) { l += sprintf("nan <=> 1n %s", e.err); }
    try { l += sprintf("after nan cmp: 4n / 2n %y", four / two); } catch (hash<ExceptionInfo> e) { l += sprintf("after nan cmp: 4n / 2n %s", e.err); }
    try { l += sprintf("0n / 0n %y", z / z); } catch (hash<ExceptionInfo> e) { l += sprintf("0n / 0n %s", e.err); }
    try { l += sprintf("after 0n / 0n: 4n / 2n %y", four / two); } catch (hash<ExceptionInfo> e) { l += sprintf("after 0n / 0n: 4n / 2n %s", e.err); }
    return foldl $1 + "\n" + $2, l;
}

string sub probes() {
    return op_probes() + "\n" + math_probes() + "\n" + flag_probes() + "\n";
}
