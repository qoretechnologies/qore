%modern
# a variant resolved at run time for a static method call is resolved in the class context of the caller: a private variant
# is not visible to a call from outside the class (issue #5717); see compiled-overload-errors.qtest
class C { public { int m = 1; } reference<int> get() { return \m; } }
class P {
    private static string sm(auto p) { return "P::sm(auto) private"; }
    static string sm(int p) { return "P::sm(int)"; }
    private static string only(auto p) { return "P::only(auto) private"; }
    static string inside(C c) { return P::sm(c.get()) + " / " + only(c.get()) + " / " + sm(c.get()); }
    static code getClosure(C c) { return string sub () { return P::sm(c.get()); }; }
}
class Q inherits P {
    static string fromChild(C c) { return P::sm(c.get()); }
}
sub run(string label, code<string()> f) {
    try { printf("%s: %s\n", label, f()); } catch (hash<ExceptionInfo> ex) { printf("%s: %s: %s\n", label, ex.err, ex.desc); }
}
sub main() {
    C c();
    for (int i = 0; i < 3; ++i) {
        run("outside", string sub () { return P::sm(c.get()); });
        run("inside", string sub () { return P::inside(c); });
        run("closure from inside", string sub () { return P::getClosure(c)(); });
        run("child class", string sub () { return Q::fromChild(c); });
    }
}
main();
