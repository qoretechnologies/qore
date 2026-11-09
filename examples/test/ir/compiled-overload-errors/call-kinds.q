%modern
# calls whose arguments match no variant (issue #5686); see compiled-overload-errors.qtest
class C {
    public { int m = 1; }
    reference<int> get() { return \m; }
    meth(int p) { }
    static smeth(int p) { }
}
sub g(int p, C c) { }
sub rec(int p, int d, C c) { if (d) { rec(c.get(), d - 1, c); } }
sub ov(int p) { printf("ov int\n"); }
sub ov(string p) { printf("ov string\n"); }
sub run(string label, code f) {
    try { f(); printf("%s: no error\n", label); } catch (hash<ExceptionInfo> ex) { printf("%s: %s: %s\n", label, ex.err, ex.desc); }
}
sub main() {
    C c();
    for (int i = 0; i < 2; ++i) {
        run("function", sub () { g(c.get(), c); });
        run("method", sub () { c.meth(c.get()); });
        run("static method", sub () { C::smeth(c.get()); });
        run("recursion", sub () { rec(1, 1, c); });
        run("overloaded", sub () { ov(c.get()); });
        code cl = sub (int p) { };
        run("closure", sub () { cl(c.get()); });
    }
}
main();
