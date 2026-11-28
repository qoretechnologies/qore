%modern
# tight loops (qore#5578, theme B: inline cancellation polls): L=tw (while), tf (for), te (foreach over a range), tn
# (a for loop in a for loop of 4 iterations); N iterations, each of an integer addition
int sub f(int a) { return a; }
int sub run_tw(int n) {
    int t = 0;
    int i = 0;
    while (i < n) {
        t += i;
        ++i;
    }
    return t;
}
int sub run_tf(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += i;
    }
    return t;
}
int sub run_te(int n) {
    int t = 0;
    foreach int i in (xrange(n)) {
        t += i;
    }
    return t;
}
int sub run_tn(int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < 4; ++j) {
            t += j;
        }
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "tw": run_tw(n); break;
    case "tf": run_tf(n); break;
    case "te": run_te(n); break;
    case "tn": run_tn(n); break;
}
