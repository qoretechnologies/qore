%modern
# calls with reference parameters (qore#7271): L=rf (a function with a reference<int> parameter that it does not
# read), ru (the same with an untyped reference parameter), rw (a function that assigns through its reference<int>
# parameter), rh (a function with a reference<hash<auto>> parameter that it does not read), rm (a method with a
# reference<int> parameter), rs (a static method with a reference<int> parameter); N iterations
int sub f(int a) { return a; }
int sub rf(reference<int> r, int a) { return a + 1; }
int sub ru(reference r, int a) { return a + 1; }
int sub rw(reference<int> r, int a) { r = a; return a + 1; }
int sub rh(reference<hash<auto>> h, int a) { return a + 1; }
class C {
    int m(reference<int> r, int a) { return a + 1; }
    static int s(reference<int> r, int a) { return a + 1; }
}
int sub run_rf(int n) {
    int x = 1;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += rf(\x, i);
    }
    return t;
}
int sub run_ru(int n) {
    int x = 1;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += ru(\x, i);
    }
    return t;
}
int sub run_rw(int n) {
    int x = 1;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += rw(\x, i);
    }
    return t + x;
}
int sub run_rh(int n) {
    hash<auto> h = {"a": 1, "b": 2};
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += rh(\h, i);
    }
    return t;
}
int sub run_rm(int n) {
    C o();
    int x = 1;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += o.m(\x, i);
    }
    return t;
}
int sub run_rs(int n) {
    int x = 1;
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += C::s(\x, i);
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "rf": run_rf(n); break;
    case "ru": run_ru(n); break;
    case "rw": run_rw(n); break;
    case "rh": run_rh(n); break;
    case "rm": run_rm(n); break;
    case "rs": run_rs(n); break;
}
