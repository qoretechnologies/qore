%modern
class G<T> {
    constructor() {}
    list<U> nu<U>(int n) { return new list<U>(); }
    hash<string, U> hu<U>() { return new hash<string, U>(); }
}

class H<A> {
    private { G<A> g; }
    constructor() { g = new G<A>(); }
    list<A> o() { return g.nu<A>(1); }
    list<A> o2() { G<string> g2(); return g2.nu<A>(1); }
    list<A> o3() { return self.own<A>(); }
    list<U> own<U>() { return new list<U>(); }
    hash<string, A> o4() { return g.hu<A>(); }
    list<list<A>> o5() { return g.nu<list<A>>(1); }
}

class K<T> {
    private { G<T> g; }
    constructor() { g = new G<T>(); }
    list<T> o() { return g.nu<T>(1); }
    static list<T> s() { G<T> sg(); return sg.nu<T>(1); }
}

H<int> h();
printf("%s\n", h.o().fullType());
printf("%s\n", h.o2().fullType());
printf("%s\n", h.o3().fullType());
printf("%s\n", h.o4().fullType());
printf("%s\n", h.o5().fullType());
H<string> hs();
printf("%s\n", hs.o().fullType());
K<float> k();
printf("%s\n", k.o().fullType());
printf("%s\n", K<int>::s().fullType());
