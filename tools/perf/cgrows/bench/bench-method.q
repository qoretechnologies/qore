%modern
# call loops: L=m (plain method calls), L=cr (call reference calls), L=mse (method calls with a side-effecting
# argument); N iterations
class C {
    int m(int a) { return a + 1; }
}
int sub f(int a) { return a; }
int sub run_m(int n) {
    C o();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += o.m(i);
    }
    return t;
}
int sub run_cr(int n) {
    C o();
    code r = \o.m();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += r(i);
    }
    return t;
}
int sub run_ma(int n) {
    C o();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        auto x = i;
        t += o.m(x);
    }
    return t;
}
int sub run_mse(int n) {
    C o();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += o.m(f(i));
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "m": run_m(n); break;
    case "cr": run_cr(n); break;
    case "mse": run_mse(n); break;
    case "ma": run_ma(n); break;
}
