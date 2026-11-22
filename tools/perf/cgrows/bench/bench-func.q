%modern
# function call loops: L=fc (plain function call), sc (static method call), mf (map calling a function over a list of
# 4 elements); N iterations
int sub f(int a) { return a + 1; }
class S {
    static int s(int a) { return a + 1; }
}
int sub run_fc(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += f(i);
    }
    return t;
}
int sub run_sc(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += S::s(i);
    }
    return t;
}
int sub run_mf(int n) {
    list<int> l = (1, 2, 3, 4);
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (map f($1), l).size();
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "fc": run_fc(n); break;
    case "sc": run_sc(n); break;
    case "mf": run_mf(n); break;
}
