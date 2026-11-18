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
# a const abstract method and a list<T> parameter, implemented two levels below
class CBox<T> {
    abstract string cc() const;
    abstract string pp2(list<T> l);
}
class CMid inherits CBox<int> {
    string cc() const { return "cc"; }
    string pp2(list<int> l) { return sprintf("%y", l); }
}
class CLeaf inherits CMid {}
# a generic interface with two type parameters
class I2<K, V> { abstract V get(K k); }
class M2 inherits I2<string, int> { int get(string k) { return k.size(); } }
# a parameterized parent of a parameterized parent
class B2<T> { abstract T ab(); }
class Md<U> inherits B2<list<U>> { }
class L2 inherits Md<int> { list<int> ab() { return (1, 2); } }
Leaf l();
printf("leaf: %y %y %y\n", l.ab(), l.f(3), l.g(3, 4));
SBox s();
printf("sbox: %y %y %y\n", s.ab(), s.f("x"), s.g("ab", 2));
IntWrap w();
printf("wrap: %y %y %y\n", w.ab(), w.f(5), w.g(6, 1));
Multi m();
printf("multi: %y %y %y\n", m.ab(), m.f(3), m.g(3, 4));
CLeaf cl();
printf("const: %y %y\n", cl.cc(), cl.pp2((1, 2)));
CBox<int> cb = cl;
printf("through the base: %y\n", cb.cc());
M2 m2();
printf("two params: %y\n", m2.get("abc"));
L2 l2();
printf("nested: %y\n", l2.ab());

# negative: an implementation whose types are not those of the specialization does not implement the abstract method
Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
try {
    p.parse("class Box2<T> { abstract T ab(); abstract string f(T x); }\n"
        "class Bad inherits Box2<string> { string ab() { return \"\"; } string f(int x) { return \"\"; } }\n"
        "Bad b();\n", "neg");
    printf("negative: no error\n");
} catch (hash<ExceptionInfo> ex) {
    printf("negative: %s: %d unimplemented\n", ex.err, (ex.desc =~ x/(\* abstract)/g).size());
}

# negative: or-nothing types are not the type argument
Program p2(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
try {
    p2.parse("class Box3<T> { abstract T ab(); abstract string f(T x); }\n"
        "class Bad3 inherits Box3<int> { int ab() { return 1; } string f(*int x) { return \"\"; } }\n"
        "Bad3 b();\n", "neg2");
    printf("negative or-nothing: no error\n");
} catch (hash<ExceptionInfo> ex) {
    printf("negative or-nothing: %s: %d unimplemented\n", ex.err, (ex.desc =~ x/(\* abstract)/g).size());
}

# negative: an implementation that returns another type than the specialization's is refused (issue #5838)
foreach string impl in (("int ab() { return 1; }", "*int ab() { return 1; }")) {
    Program p3(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p3.parse("class Box4<T> { abstract T ab(); }\n"
            "class Bad4 inherits Box4<" + (impl =~ /^\*/ ? "int" : "string") + "> { " + impl + " }\n", "neg3");
        printf("negative return type: no error\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("negative return type: %s\n", ex.err);
    }
}
