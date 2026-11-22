%modern
# call_function() loops (batch AS, #6052 rows): L=cf (call_function() of a call reference), cfa (call_function_args()
# with a list of one argument); N iterations
int sub f(int a) { return a + 1; }
int sub run_cf(int n) {
    code c = \f();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += call_function(c, i);
    }
    return t;
}
int sub run_cfa(int n) {
    code c = \f();
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += call_function_args(c, (i,));
    }
    return t;
}
int n = ENV.N.toInt();
switch (ENV.L) {
    case "cf": run_cf(n); break;
    case "cfa": run_cfa(n); break;
}
