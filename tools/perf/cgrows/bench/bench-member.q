%modern
# member access loops (qore#6760): L=mr (public member read through a variable), mw (public member write), pr (private
# member read through a variable of the class in its own method), pi (private member of a base class read in its
# method on an object of a subclass); N iterations
class A {
    public { int x = 1; }
    private { int p = 2; }
    int run_pr(A o, int n) {
        int t = 0;
        for (int i = 0; i < n; ++i) {
            t += o.p;
        }
        return t;
    }
    int run_pi(object o, int n) {
        int t = 0;
        for (int i = 0; i < n; ++i) {
            t += o.p;
        }
        return t;
    }
}
class S inherits A {}
# called by the dead statements that the row tool inserts
int sub f(int a) { return a; }
int sub run_mr(A o, int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += o.x;
    }
    return t;
}
int sub run_mw(A o, int n) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        o.x = i;
    }
    return t;
}
int n = ENV.N.toInt();
A a();
S s();
switch (ENV.L) {
    case "mr": run_mr(a, n); break;
    case "mw": run_mw(a, n); break;
    case "pr": a.run_pr(a, n); break;
    case "pi": s.run_pi(s, n); break;
}
