%modern
# Temporary fixed aggregates read with a computed selector: an integer pair read with an index of (x & 1), a hash
# read with a key that a conditional chooses from two literals, and a hash read with a constant key (qore#5578).
int sub f(int a) { return a + 1; }
int sub run_tl(int n) {
    int t = 0;
    int seed = 7;
    for (int i = 0; i < n; ++i) {
        seed = (seed * 33 + 17) & 65535;
        list<int> pair = (seed, seed + 1);
        t += pair[seed & 1];
    }
    return t;
}
int sub run_th(int n) {
    int t = 0;
    int seed = 7;
    for (int i = 0; i < n; ++i) {
        seed = (seed * 33 + 17) & 65535;
        hash<string, int> pair = {"left": seed, "right": seed + 1};
        string key = seed & 1 ? "right" : "left";
        t += pair{key};
    }
    return t;
}
int sub run_thc(int n) {
    int t = 0;
    int seed = 7;
    for (int i = 0; i < n; ++i) {
        seed = (seed * 33 + 17) & 65535;
        hash<string, int> pair = {"left": seed, "right": seed + 1};
        t += pair.right;
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "tl": run_tl(n); break;
    case "th": run_th(n); break;
    case "thc": run_thc(n); break;
}
