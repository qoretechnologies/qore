%modern
# calls through function references whose targets the loop does not know (qore#5578): L=fr (a reference to a function
# of one int parameter, given as a typed parameter), fr0 (a reference to a function without parameters); N iterations
int sub f(int a) { return a + 1; }
int sub g() { return 1; }
int sub run_fr(code<int(int)> c, int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += c(i);
    }
    return t;
}
int sub run_fr0(code<int()> c, int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += c();
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "fr": run_fr(\f(), n); break;
    case "fr0": run_fr0(\g(), n); break;
}
