%modern
class G<T> {
    constructor() {}
    list<U> nu<U>(int n) { return new list<U>(); }
    list<U> nu<U>(string s) { list<U> l(); return l; }
    hash<string, U> nu<U>(float f) { return new hash<string, U>(); }
}
class H<A> {
    private { G<A> g; }
    constructor() { g = new G<A>(); }
    list<A> a() { return g.nu<A>(1); }
    list<A> b() { return g.nu<A>("s"); }
    hash<string, A> c() { return g.nu<A>(1.5); }
}
H<int> h();
printf("%s %s %s\n", h.a().fullType(), h.b().fullType(), h.c().fullType());
H<string> hs();
printf("%s %s %s\n", hs.a().fullType(), hs.b().fullType(), hs.c().fullType());
