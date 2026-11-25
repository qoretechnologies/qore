%modern
# A list<int> built with push and changed in place with an indexed compound assignment, as in the list_mutate kernel
# of bench/runtime-comparison, and the same loop reading the element without changing it (qore#5578).
int sub f(int a) { return a + 1; }
int sub run_lm(int n) {
    int t = 0;
    int seed = 7;
    list<int> data = ();
    for (int i = 0; i < n; ++i) {
        seed = (seed * 33 + 17) & 65535;
        push data, seed;
        data[i] += 1;
        t += data[i];
    }
    return t;
}
int sub run_lms(int n) {
    int t = 0;
    int seed = 7;
    list<int> data = ();
    for (int i = 0; i < n; ++i) {
        seed = (seed * 33 + 17) & 65535;
        push data, seed;
        data[i] -= 1;
        t += data[i];
    }
    return t;
}
int sub run_lp(int n) {
    int t = 0;
    int seed = 7;
    list<int> data = ();
    for (int i = 0; i < n; ++i) {
        seed = (seed * 33 + 17) & 65535;
        push data, seed;
        t += data[i];
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "lm": run_lm(n); break;
    case "lms": run_lms(n); break;
    case "lp": run_lp(n); break;
}
