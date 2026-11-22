%modern
# batch S rows, N iterations each: float-to-int conversion (#6181), unary plus/minus (#5712, #6132), class cast
# (#5712)
# L=cv: conversion of a float to a softint on assignment; L=ci: int(float); L=cs: <float>.toInt()
# L=up: unary plus of an int; L=um: unary minus of an int; L=ua: unary plus of an untyped value
# L=cc: a class cast
# post/pre increment as a statement (#6199): L=lp/lr: a local int (post/pre), L=mp/mr: a member, L=gp/gr: a global,
# L=hp/hr: a hash key
class B {
}
class C inherits B {
    public {
        int v = 1;
    }
}

int sub f(int a) { return a; }

int sub run_cv(int n) {
    float fv = 1.5;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        softint x = fv + i;
        t += x;
    }
    return t;
}
int sub run_ci(int n) {
    float fv = 2.5;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += int(fv * i);
    }
    return t;
}
int sub run_cs(int n) {
    float fv = 3.5;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (fv + i).toInt();
    }
    return t;
}
int sub run_up(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += +i;
    }
    return t;
}
int sub run_um(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += -i;
    }
    return t;
}
int sub run_ua(int n) {
    auto a = 5;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += +a;
    }
    return t;
}
int sub run_cc(int n) {
    B b = new C();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += cast<C>(b).v;
    }
    return t;
}
int g = 0;
class Ctr {
    public {
        int c = 0;
    }
    int post(int n) { int t = 0; for (int i = 0; i < n; ++i) { c++; } return c + t; }
    int pre(int n) { int t = 0; for (int i = 0; i < n; ++i) { ++c; } return c + t; }
}
int sub run_lp(int n) { int t = 0; int x = 0; for (int i = 0; i < n; ++i) { x++; } return x + t; }
int sub run_lr(int n) { int t = 0; int x = 0; for (int i = 0; i < n; ++i) { ++x; } return x + t; }
int sub run_gp(int n) { int t = 0; for (int i = 0; i < n; ++i) { g++; } return g + t; }
int sub run_gr(int n) { int t = 0; for (int i = 0; i < n; ++i) { ++g; } return g + t; }
int sub run_hp(int n) { int t = 0; hash<auto> h = {"k": 0}; for (int i = 0; i < n; ++i) { h.k++; } return h.k + t; }
int sub run_hr(int n) { int t = 0; hash<auto> h = {"k": 0}; for (int i = 0; i < n; ++i) { ++h.k; } return h.k + t; }
int n = ENV.N.toInt();
switch (ENV.L) {
    case "cv": run_cv(n); break;
    case "ci": run_ci(n); break;
    case "cs": run_cs(n); break;
    case "up": run_up(n); break;
    case "um": run_um(n); break;
    case "ua": run_ua(n); break;
    case "cc": run_cc(n); break;
    case "lp": run_lp(n); break;
    case "lr": run_lr(n); break;
    case "mp": { Ctr c(); c.post(n); break; }
    case "mr": { Ctr c(); c.pre(n); break; }
    case "gp": run_gp(n); break;
    case "gr": run_gr(n); break;
    case "hp": run_hp(n); break;
    case "hr": run_hr(n); break;
}
