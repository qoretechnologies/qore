%modern
# an implicit self call to a method of a generic base class returns the type of the specialization that the class
# inherits, as an explicit self.method() call does (issue #5769); see generic-implicit-self-call.qtest
class Foo { string n() { return "foo"; } }
class Box<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    T get() { return v; }
    static T id(T x) { return x; }
    # inside the generic class, the type of an implicit self call stays the type parameter
    T again() { return get(); }
}
class FooBox inherits Box<Foo> {
    constructor() : Box(new Foo()) {}
    string name() { return get().n(); }
    string nameAgain() { return again().n(); }
    string staticName(Foo f) { return id(f).n(); }
    Foo typed() { return get(); }
}
# two levels below the specialization
class FooBox2 inherits FooBox {
    string name2() { return get().n() + "2"; }
}
class IntBox inherits Box<int> {
    constructor(int i) : Box(i) {}
    int twice() { return get() * 2; }
    int plus(auto x) { return id(x) + 1; }
    int plusSelf(auto x) { return self.id(x) + 1; }
    int plusScoped(auto x) { return Box<int>::id(x) + 1; }
}
# a generic subclass: the type of the call is its own type parameter
class Wrap<U> inherits Box<U> {
    constructor(U u) : Box(u) {}
    U unwrap() { return get(); }
}
class FooWrap inherits Wrap<Foo> {
    constructor() : Wrap(new Foo()) {}
    string name() { return unwrap().n() + "/" + get().n(); }
}
sub run(string label, code<string()> f) {
    try { printf("%s: %s\n", label, f()); } catch (hash<ExceptionInfo> ex) { printf("%s: %s\n", label, ex.err); }
}
sub main() {
    for (int i = 0; i < 3; ++i) {
        run("name", string sub () { return (new FooBox()).name(); });
        run("again", string sub () { return (new FooBox()).nameAgain(); });
        run("static", string sub () { return (new FooBox()).staticName(new Foo()); });
        run("typed", string sub () { return (new FooBox()).typed().n(); });
        run("two levels", string sub () { return (new FooBox2()).name2(); });
        run("int", string sub () { return string((new IntBox(21)).twice()); });
        run("generic subclass", string sub () { return (new FooWrap()).name(); });
        run("int arg", string sub () { return string((new IntBox(1)).plus(4)); });
        run("int arg self", string sub () { return string((new IntBox(1)).plusSelf(4)); });
        run("int arg scoped", string sub () { return string((new IntBox(1)).plusScoped(4)); });
        # negative: a value of another type, known only at run time
        run("int wrong arg", string sub () { auto s = "x"; return string((new IntBox(1)).plus(s)); });
        run("int wrong arg self", string sub () { auto s = "x"; return string((new IntBox(1)).plusSelf(s)); });
    }
}
main();
