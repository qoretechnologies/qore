%modern
# calls whose variant is resolved again at run time and found to be another one: it is called with the arguments as the
# caller gave them; see compiled-overload-errors.qtest
class C {
    public { int m = 1; string s = "1.5"; }
    reference<int> get() { return \m; }
    reference<string> gets() { return \s; }
}
class Tr {
    public { string n; }
    constructor(string n) { self.n = n; }
}
# methods: the derived class's variant does not accept a reference, the base class's does
class B5 {
    string m1(auto p) { return sprintf("B5::m1(auto) p=%y", p); }
    string m2(auto p, auto q) { return sprintf("B5::m2(auto, auto) p=%y q=%y", p, q); }
}
class D5 inherits B5 {
    string m1(int p) { return "D5::m1(int)"; }
    string m2(int p, int q) { return "D5::m2(int, int)"; }
}
# static methods
class B {
    static string s(Tr t, auto p) { return sprintf("B::s(Tr, auto) t=%y p=%y", t.n, p); }
}
class CC inherits B {
    static string s(Tr t, int p) { return "CC::s(Tr, int)"; }
}
# an argument converted for the variant that does not match (softint "1.5" -> 1): the other variant gets the original
class B6 {
    string m(auto a, auto p) { return sprintf("B6::m(auto, auto) a=%y p=%y", a, p); }
}
class D6 inherits B6 {
    string m(softint a, int p) { return "D6::m(softint, int)"; }
}
# functions
string sub fn(int a, int p) { return "fn(int, int)"; }
string sub fn(auto a, auto p) { return sprintf("fn(auto, auto) a=%y p=%y", a, p); }
# a deleted object matches the declared type of an object parameter at parse time, but not at run time: the variant
# resolved again gets the argument
class X {
}
string sub dobj(X x) { return "dobj(X)"; }
string sub dobj(auto a) { return sprintf("dobj(auto) with an argument: %y", a !== NOTHING); }
sub run(string label, code<string()> f) {
    try {
        printf("%s: %s\n", label, f());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}
sub main() {
    C c();
    D5 d5();
    D6 d6();
    # more than once, so that the compiled tiers run the calls too
    for (int i = 0; i < 3; ++i) {
        run("method", string sub () { return d5.m1(c.get()); });
        run("method, 2 args", string sub () { return d5.m2(1, c.get()); });
        run("static", string sub () { return CC::s(new Tr("x"), c.get()); });
        run("converted arg", string sub () { return d6.m(c.gets(), c.get()); });
        run("function", string sub () { return fn(1, c.get()); });
        code cr = \fn();
        run("call reference", string sub () { return cr(1, c.get()); });
        # typed arguments: the variant committed at parse time is called
        run("typed method", string sub () { return d5.m1(1); });
        run("typed function", string sub () { return fn(1, 2); });
        X x();
        X x2 = x;
        delete x;
        run("deleted object", string sub () { return dobj(x2); });
    }
}
main();
