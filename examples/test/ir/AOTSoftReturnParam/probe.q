#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o.
# Functions with soft return types that return a parameter of another type: each call must return the converted
# value as in AST, also when the compiled caller inlines the callee's summary instead of calling it (#5724)

%modern
# fp_neg() negates a bool parameter on purpose
%disable-warning invalid-operation

softfloat sub fb(bool b) { return b; }
softint sub ib(bool b) { return b; }
softfloat sub fi(int i) { return i; }
softfloat sub fsel(bool b) { return b ? 2.5 : -1.5; }
softfloat sub fwrap(bool b) { return fb(b); }
float sub fsum(bool b) { return fb(b) + 1.0; }
*softfloat sub fbn(bool b) { return b; }

softint sub si_b(bool v) { return v; }
softint sub si_f(float v) { return v; }
softint sub si_s(string v) { return v; }
softfloat sub sf_b(bool v) { return v; }
softfloat sub sf_i(int v) { return v; }
softfloat sub sf_s(string v) { return v; }
softnumber sub sn_b(bool v) { return v; }
softnumber sub sn_f(float v) { return v; }
softbool sub sb_i(int v) { return v; }
softbool sub sb_f(float v) { return v; }
softbool sub sb_s(string v) { return v; }
softstring sub ss_b(bool v) { return v; }
softstring sub ss_i(int v) { return v; }
softstring sub ss_f(float v) { return v; }
softdate sub sd_i(int v) { return v; }
softlist sub sl_b(bool v) { return v; }

# a bool parameter as an operand or a Select value in a float summary (hash projection and plain float
# parameters); a bool parameter is a Select condition only, so these functions are called (#5724)
softfloat sub fh_add(hash<string, float> h, bool b) { return h.x + b; }
softfloat sub fh_sub(hash<string, float> h, bool b) { return h.x - b; }
softfloat sub fh_mul(hash<string, float> h, bool b) { return h.x * b; }
softfloat sub fh_div(hash<string, float> h, bool b) { return h.x / b; }
softfloat sub fh_neg(hash<string, float> h, bool b) { return h.x + -b; }
softfloat sub fh_selv(hash<string, float> h, bool c, bool b) { return c ? b : h.x; }
softfloat sub fh_selc(hash<string, float> h, bool c) { return c ? h.x : 2.0; }
softfloat sub fp_add(float x, bool b) { return x + b; }
softfloat sub fp_sub(float x, bool b) { return x - b; }
softfloat sub fp_mul(float x, bool b) { return x * b; }
softfloat sub fp_div(float x, bool b) { return x / b; }
softfloat sub fp_neg(bool b) { return -b; }
softfloat sub fp_selv(bool c, bool b, float y) { return c ? b : y; }
softfloat sub fp_selc(bool c, float x, float y) { return c ? x : y; }
float sub w_fh_add(hash<string, float> h, bool b) { return fh_add(h, b); }
float sub w_fh_sub(hash<string, float> h, bool b) { return fh_sub(h, b); }
float sub w_fh_mul(hash<string, float> h, bool b) { return fh_mul(h, b); }
float sub w_fh_div(hash<string, float> h, bool b) { return fh_div(h, b); }
float sub w_fh_neg(hash<string, float> h, bool b) { return fh_neg(h, b); }
float sub w_fh_selv(hash<string, float> h, bool c, bool b) { return fh_selv(h, c, b); }
float sub w_fh_selc(hash<string, float> h, bool c) { return fh_selc(h, c); }
float sub w_fp_add(float x, bool b) { return fp_add(x, b); }
float sub w_fp_sub(float x, bool b) { return fp_sub(x, b); }
float sub w_fp_mul(float x, bool b) { return fp_mul(x, b); }
float sub w_fp_div(float x, bool b) { return fp_div(x, b); }
float sub w_fp_neg(bool b) { return fp_neg(b); }
float sub w_fp_selv(bool c, bool b, float y) { return fp_selv(c, b, y); }
float sub w_fp_selc(bool c, float x, float y) { return fp_selc(c, x, y); }

# a comparison as an operand or a Select value in an integer summary
softint sub ih_sadd(hash<string, string> h, int b) { return (h.x.size() == b) + 1; }
softint sub ih_add(hash<string, int> h, int b) { return (h.x == b) + 1; }
softint sub ih_sel(hash<string, int> h, int a, int b) { return a > 0 ? (h.x == b) : b; }
softint sub ih_neg(hash<string, int> h, int b) { return -(h.x == b); }
softint sub il_add(list<int> l, int b) { return (l.size() == b) + 1; }
softint sub ip_add(int a, int b) { return (a == b) * 3 + a; }
int sub w_ih_sadd(hash<string, string> h, int b) { return ih_sadd(h, b); }
int sub w_ih_add(hash<string, int> h, int b) { return ih_add(h, b); }
int sub w_ih_sel(hash<string, int> h, int a, int b) { return ih_sel(h, a, b); }
int sub w_ih_neg(hash<string, int> h, int b) { return ih_neg(h, b); }
int sub w_il_add(list<int> l, int b) { return il_add(l, b); }
int sub w_ip_add(int a, int b) { return ip_add(a, b); }

sub show(string label, auto v) {
    printf("%s: %y (%s)\n", label, v, v.type());
}

int x = 2;
bool t = True;
bool f = False;
show("fb(True)", fb(True));
show("fb(False)", fb(False));
show("fb(local)", fb(t));
show("fb(x > 1)", fb(x > 1));
show("fb(x < 1)", fb(x < 1));
show("ib(True)", ib(True));
show("fi(2)", fi(2));
show("fsel(True)", fsel(True));
show("fsel(local)", fsel(f));
show("fwrap(True)", fwrap(True));
show("fwrap(local)", fwrap(f));
show("fsum(True)", fsum(True));
show("fsum(local)", fsum(f));
show("fbn(True)", fbn(True));
show("fb(True) + fb(False)", fb(True) + fb(False));
float acc = 0.0;
for (int i = 0; i < 4; ++i) {
    acc += fb(i % 2 == 0);
}
show("loop sum", acc);

foreach bool v in ((True, False)) {
    show(sprintf("si_b(%y)", v), si_b(v));
    show(sprintf("sf_b(%y)", v), sf_b(v));
    show(sprintf("sn_b(%y)", v), sn_b(v));
    show(sprintf("ss_b(%y)", v), ss_b(v));
    show(sprintf("sl_b(%y)", v), sl_b(v));
}
show("si_f(2.5)", si_f(2.5));
show("si_s('3')", si_s("3"));
show("sf_i(2)", sf_i(2));
show("sf_s('2.5')", sf_s("2.5"));
show("sn_f(2.5)", sn_f(2.5));
show("sb_i(2)", sb_i(2));
show("sb_i(0)", sb_i(0));
show("sb_f(0.5)", sb_f(0.5));
show("sb_s('')", sb_s(""));
show("ss_i(3)", ss_i(3));
show("ss_f(2.5)", ss_f(2.5));
show("sd_i(0)", sd_i(0));

hash<string, float> fh = {"x": 1.5};
foreach bool v in ((True, False)) {
    show(sprintf("fh_add(%y)", v), fh_add(fh, v));
    show(sprintf("w_fh_add(%y)", v), w_fh_add(fh, v));
    show(sprintf("fh_sub(%y)", v), fh_sub(fh, v));
    show(sprintf("w_fh_sub(%y)", v), w_fh_sub(fh, v));
    show(sprintf("fh_mul(%y)", v), fh_mul(fh, v));
    show(sprintf("w_fh_mul(%y)", v), w_fh_mul(fh, v));
    show(sprintf("fh_neg(%y)", v), fh_neg(fh, v));
    show(sprintf("w_fh_neg(%y)", v), w_fh_neg(fh, v));
    show(sprintf("fh_selv(True, %y)", v), fh_selv(fh, True, v));
    show(sprintf("w_fh_selv(True, %y)", v), w_fh_selv(fh, True, v));
    show(sprintf("fh_selv(False, %y)", v), fh_selv(fh, False, v));
    show(sprintf("w_fh_selv(False, %y)", v), w_fh_selv(fh, False, v));
    show(sprintf("fh_selc(%y)", v), fh_selc(fh, v));
    show(sprintf("w_fh_selc(%y)", v), w_fh_selc(fh, v));
    show(sprintf("fp_add(%y)", v), fp_add(1.5, v));
    show(sprintf("w_fp_add(%y)", v), w_fp_add(1.5, v));
    show(sprintf("fp_sub(%y)", v), fp_sub(1.5, v));
    show(sprintf("w_fp_sub(%y)", v), w_fp_sub(1.5, v));
    show(sprintf("fp_mul(%y)", v), fp_mul(1.5, v));
    show(sprintf("w_fp_mul(%y)", v), w_fp_mul(1.5, v));
    show(sprintf("fp_neg(%y)", v), fp_neg(v));
    show(sprintf("w_fp_neg(%y)", v), w_fp_neg(v));
    show(sprintf("fp_selv(True, %y)", v), fp_selv(True, v, 2.0));
    show(sprintf("w_fp_selv(True, %y)", v), w_fp_selv(True, v, 2.0));
    show(sprintf("fp_selv(False, %y)", v), fp_selv(False, v, 2.0));
    show(sprintf("w_fp_selv(False, %y)", v), w_fp_selv(False, v, 2.0));
    show(sprintf("fp_selc(%y)", v), fp_selc(v, 1.0, 2.0));
    show(sprintf("w_fp_selc(%y)", v), w_fp_selc(v, 1.0, 2.0));
}
# division by a bool parameter: True divides by 1.0, False raises DIVISION-BY-ZERO
show("fh_div(True)", fh_div(fh, True));
show("w_fh_div(True)", w_fh_div(fh, True));
show("fp_div(True)", fp_div(1.5, True));
show("w_fp_div(True)", w_fp_div(1.5, True));
foreach code c in ((float sub () { return fh_div(fh, False); }, float sub () { return w_fh_div(fh, False); },
        float sub () { return fp_div(1.5, False); }, float sub () { return w_fp_div(1.5, False); })) {
    try {
        show("division by False", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("division by False: %s\n", ex.err);
    }
}

foreach int v in ((1, 2)) {
    show(sprintf("ih_sadd(%y)", v), ih_sadd({"x": "a"}, v));
    show(sprintf("w_ih_sadd(%y)", v), w_ih_sadd({"x": "a"}, v));
    show(sprintf("ih_add(%y)", v), ih_add({"x": 1}, v));
    show(sprintf("w_ih_add(%y)", v), w_ih_add({"x": 1}, v));
    show(sprintf("ih_sel(1, %y)", v), ih_sel({"x": 1}, 1, v));
    show(sprintf("w_ih_sel(1, %y)", v), w_ih_sel({"x": 1}, 1, v));
    show(sprintf("ih_sel(0, %y)", v), ih_sel({"x": 1}, 0, v));
    show(sprintf("w_ih_sel(0, %y)", v), w_ih_sel({"x": 1}, 0, v));
    show(sprintf("ih_neg(%y)", v), ih_neg({"x": 1}, v));
    show(sprintf("w_ih_neg(%y)", v), w_ih_neg({"x": 1}, v));
    show(sprintf("il_add(%y)", v), il_add((1,), v));
    show(sprintf("w_il_add(%y)", v), w_il_add((1,), v));
    show(sprintf("ip_add(%y)", v), ip_add(1, v));
    show(sprintf("w_ip_add(%y)", v), w_ip_add(1, v));
}
