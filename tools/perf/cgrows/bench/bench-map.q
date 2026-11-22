%modern
# map loops: L=mp (map of an expression), L=mm (map calling a method), L=mmse (map calling a method with an argument
# that is a call); N iterations of a map over a list of 4 elements
class C {
    int m(int a) { return a + 1; }
}
int sub f(int a) { return a; }
int sub run_mp(int n) {
    list<int> l = (1, 2, 3, 4);
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (map $1 + i, l).size();
    }
    return t;
}
int sub run_mm(int n) {
    C o();
    list<int> l = (1, 2, 3, 4);
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (map o.m($1), l).size();
    }
    return t;
}
int sub run_mmse(int n) {
    C o();
    list<int> l = (1, 2, 3, 4);
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (map o.m(f($1)), l).size();
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "mp": run_mp(n); break;
    case "mm": run_mm(n); break;
    case "mmse": run_mmse(n); break;
}
