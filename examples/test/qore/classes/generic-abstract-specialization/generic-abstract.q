%modern
# an abstract method of a generic class is implemented in a class that inherits a specialization of it: the abstract
# variant's types are those of the specialization (issue #5773); see generic-abstract-specialization.qtest
class Box<T> {
    abstract T ab();
    abstract string f(T x);
    abstract T g(T x, int n);
}
class Mid inherits Box<int> {
    int ab() { return 1; }
    string f(int x) { return "f" + x; }
    int g(int x, int n) { return x * n; }
}
class Leaf inherits Mid {}
class SBox inherits Box<string> {
    string ab() { return "s"; }
    string f(string x) { return "f" + x; }
    string g(string x, int n) { return strmul(x, n); }
}
# a generic subclass implements with its own type parameter
class Wrap<U> inherits Box<U> {
    private { U u; }
    constructor(U u) { self.u = u; }
    U ab() { return u; }
    string f(U x) { return sprintf("w%y", x); }
    U g(U x, int n) { return x; }
}
class IntWrap inherits Wrap<int> {
    constructor() : Wrap(7) {}
}
# a sibling base class provides the implementation
class Impl {
    int ab() { return 2; }
    string f(int x) { return "i" + x; }
    int g(int x, int n) { return x + n; }
}
class Multi inherits Impl, Box<int> {}
Leaf l();
printf("leaf: %y %y %y\n", l.ab(), l.f(3), l.g(3, 4));
SBox s();
printf("sbox: %y %y %y\n", s.ab(), s.f("x"), s.g("ab", 2));
IntWrap w();
printf("wrap: %y %y %y\n", w.ab(), w.f(5), w.g(6, 1));
Multi m();
printf("multi: %y %y %y\n", m.ab(), m.f(3), m.g(3, 4));

# negative: an implementation whose types are not those of the specialization does not implement the abstract method
Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
try {
    p.parse("class Box2<T> { abstract T ab(); abstract string f(T x); }\n"
        "class Bad inherits Box2<string> { int ab() { return 1; } string f(int x) { return \"\"; } }\n"
        "Bad b();\n", "neg");
    printf("negative: no error\n");
} catch (hash<ExceptionInfo> ex) {
    printf("negative: %s: %d unimplemented\n", ex.err, (ex.desc =~ x/(\* abstract)/g).size());
}
