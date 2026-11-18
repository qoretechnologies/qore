%modern
# the type parameters of generic code (qore#5774): a value of a bounded type parameter has its bound (an upper bound):
# it is assignable to the bound and has the bound class's methods; a value of an unbounded one is a may-match, checked
# at run time; see compiled-overload-errors.qtest

class Num<T: int> {
    int twice(T v) { int i = v; return i * 2; }
    int arg(T v) { return takesInt(v); }
    static int takesInt(int i) { return i + 1; }
    int orNothing(*T v) { return v ?? -1; }
}

class Base {
    string name() { return "base"; }
}

class Derived inherits Base {
    string name() { return "derived"; }
}

class Box<T: Base> {
    string who(T v) { return v.name(); }
    Base up(T v) { return v; }
}

class Any<T> {
    T id(auto h) { return h; }
    int asInt(T v) { int i = v; return i; }
}

class Lists<T: list<int>> {
    auto twice(T v) { return map $1 * 2, v; }
}

class AnyList<T> {
    auto twice(T v) { return map $1 * 2, v; }
}

# a soft bound accepts type arguments that it converts, but a value bound to T is not converted: T is a may-match
class Soft<T: softint> {
    auto plus(T v) { return v + 1; }
    int asInt(T v) { return v; }
}

class Cell<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    T get() { return v; }
}

class CellUser<T: Cell<int>> {
    int get(T c) { return c.get(); }
}

class OrNothing<T: Base> {
    *T keep(*T v) { *T x = v; return x; }
    *string name(*T v) { return v ? v.name() : NOTHING; }
}

# compound assignments take the operation of the bound, or decide it at run time for an unbounded type parameter
class Compound<T: float> {
    float plus(T v) { T x = v; x += 1; return x; }
    float minus(T v) { T x = v; x -= 1; return x; }
    float times(T v) { T x = v; x *= 2; return x; }
}

class CompoundAny<T> {
    auto plus(T v, auto a) { T x = v; x += a; return x; }
}

class CompoundList<T: list<int>> {
    auto plus(T v) { T x = v; x += 3; return x; }
}

class Forward<U: int> {
    int twice(U v) { Num<U> n(); return n.twice(v); }
}

class Sub<U: int> inherits Num<U> {
}

T sub f<T>(auto h) { return h; }
U sub g<U>(int a) { return a; }
int sub b<V: int>(V v) { int i = v; return i + 100; }

sub t(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

Num<int> n();
t("bounded assign", auto sub () { return n.twice(21); });
t("bounded arg", auto sub () { return n.arg(1); });
t("bounded or-nothing", auto sub () { return n.orNothing(5); });
t("bounded or-nothing nothing", auto sub () { return n.orNothing(); });
Box<Derived> bx();
t("class bound method", auto sub () { return bx.who(new Derived()); });
t("class bound up", auto sub () { return bx.up(new Derived()).name(); });
Any<int> a();
t("unbounded return", auto sub () { return a.id(3); });
t("unbounded return mismatch", auto sub () { return a.id("x"); });
t("unbounded assign", auto sub () { return a.asInt(4); });
Any<string> as();
t("unbounded assign mismatch", auto sub () { return as.asInt("x"); });
t("function unbounded", auto sub () { return f<int>(5); });
t("function unbounded mismatch", auto sub () { return g<string>(7); });
t("function bounded", auto sub () { return b<int>(1); });

Lists<list<int>> ls();
t("map bounded list", auto sub () { return ls.twice((1, 2)); });
AnyList<list<int>> al();
t("map unbounded list", auto sub () { return al.twice((1, 2)); });
AnyList<int> ai();
t("map unbounded scalar", auto sub () { return ai.twice(3); });
Soft<string> ss();
t("soft bound plus", auto sub () { return ss.plus("1"); });
t("soft bound mismatch", auto sub () { return ss.asInt("1"); });
Soft<int> si();
t("soft bound int", auto sub () { return si.asInt(2); });
CellUser<Cell<int>> cu();
t("class bound with type arguments", auto sub () { return cu.get(new Cell<int>(8)); });
OrNothing<Derived> on();
t("or-nothing keep", auto sub () { return on.keep(new Derived()).name(); });
t("or-nothing keep nothing", auto sub () { return on.keep(); });
t("or-nothing receiver", auto sub () { return on.name(new Derived()); });
t("or-nothing receiver nothing", auto sub () { return on.name(); });
Forward<int> fw();
t("type parameter as type argument", auto sub () { return fw.twice(5); });
Sub<int> sb();
t("type parameter as base type argument", auto sub () { return sb.twice(6); });

Compound<float> cf();
t("compound plus bounded", auto sub () { return cf.plus(1.5); });
t("compound minus bounded", auto sub () { return cf.minus(1.5); });
t("compound times bounded", auto sub () { return cf.times(1.5); });
CompoundAny<string> cs();
t("compound plus unbounded string", auto sub () { return cs.plus("a", 1); });
CompoundAny<int> ci();
t("compound plus unbounded int", auto sub () { return ci.plus(1, 2); });
CompoundList<list<int>> cl();
t("compound plus bounded list", auto sub () { return cl.plus((1, 2)); });

# at top level (the JIT tier in jit mode)
printf("top: %y %y %y %y %y %y\n", n.twice(2), bx.who(new Derived()), a.id(9), b<int>(2), ls.twice((3,)),
    al.twice((4,)));
