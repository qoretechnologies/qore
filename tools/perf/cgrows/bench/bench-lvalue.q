%modern
# lvalue loops: L=rl (remove a list element), rh (remove a hash key, h.k), dh (delete a hash key), ri (remove a typed
# int local holding a large int), la (assign and += of an int lvalue); N iterations
int sub f(int a) { return a; }
int sub run_rl(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        list<auto> l = (i, 2);
        auto v = remove l[0];
        t += v;
    }
    return t;
}
int sub run_rh(int n) {
    int t = 0;
    hash<auto> h;
    for (int i = 0; i < n; ++i) {
        h.k = i;
        auto v = remove h.k;
        t += v;
    }
    return t;
}
int sub run_dh(int n) {
    int t = 0;
    hash<auto> h;
    for (int i = 0; i < n; ++i) {
        h.k = i;
        delete h.k;
        ++t;
    }
    return t;
}
int sub run_ri(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        int x = 0x7fffffffffff + i;
        auto v = remove x;
        t += v;
    }
    return t;
}
int sub run_la(int n) {
    int t = 0;
    int x;
    for (int i = 0; i < n; ++i) {
        x = i;
        x += 3;
        t += x;
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "rl": run_rl(n); break;
    case "rh": run_rh(n); break;
    case "dh": run_dh(n); break;
    case "ri": run_ri(n); break;
    case "la": run_la(n); break;
}
