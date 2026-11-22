%modern
# object member enumeration loops (batch AS, #7048 rows): L=ok (keys of an object), osz (size()), ofk (firstKey()),
# oel (elements operator); N iterations; an object with 4 public members, called from outside the class
class E {
    public { int a = 1; int b = 2; int c = 3; int d = 4; }
}
int sub f(int a) { return a + 1; }
int sub run_ok(int n) {
    E e();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (keys e).size();
    }
    return t;
}
int sub run_osz(int n) {
    E e();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += e.size();
    }
    return t;
}
int sub run_ofk(int n) {
    E e();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += e.firstKey().size();
    }
    return t;
}
int sub run_oel(int n) {
    E e();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += elements e;
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "ok": run_ok(n); break;
    case "osz": run_osz(n); break;
    case "ofk": run_ofk(n); break;
    case "oel": run_oel(n); break;
}
