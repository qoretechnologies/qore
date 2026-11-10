#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o.
# Functions with soft return types that return a parameter of another type: each call must return the converted
# value as in AST, also when the compiled caller inlines the callee's summary instead of calling it (#5724)

%modern

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
