%modern
# self call loops: L=fs (final class, self.m(i)), fp (final class, m(i)), cp (class, m(i)), cf (class, m(f(i)),
# checked), ff (final class, m(f(i))); N iterations
int sub f(int a) { return a; }
final class F {
    int m(int a) { return a + 1; }
    int run_fs(int n) { int t = 0; for (int i = 0; i < n; ++i) { t += self.m(i); } return t; }
    int run_fp(int n) { int t = 0; for (int i = 0; i < n; ++i) { t += m(i); } return t; }
    int run_ff(int n) { int t = 0; for (int i = 0; i < n; ++i) { t += m(f(i)); } return t; }
}
class C {
    int m(int a) { return a + 1; }
    int run_cp(int n) { int t = 0; for (int i = 0; i < n; ++i) { t += m(i); } return t; }
    int run_cf(int n) { int t = 0; for (int i = 0; i < n; ++i) { t += m(f(i)); } return t; }
}
int n = ENV.N.toInt();
F fo();
C co();
switch (ENV.L) {
    case "fs": fo.run_fs(n); break;
    case "fp": fo.run_fp(n); break;
    case "ff": fo.run_ff(n); break;
    case "cp": co.run_cp(n); break;
    case "cf": co.run_cf(n); break;
}
