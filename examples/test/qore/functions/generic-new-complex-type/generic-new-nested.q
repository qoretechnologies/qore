%modern
hash<string, list<T>> sub gn<T>(T v) { return new hash<string, list<T>>({"a": (v, v)}); }
list<hash<string, T>> sub gh<T>(T v) { return new list<hash<string, T>>({"a": v}); }
hash<string, T> sub gk<T, U>(T v, U u) { return new hash<string, T>({"a": v}); }
list<U> sub gu<T, U>(T v, U u) { return new list<U>(u); }

class GPair<T, U> {
    private { T a; U b; }
    constructor(T a, U b) { self.a = a; self.b = b; }
    hash<string, U> hb() { return new hash<string, U>({"b": b}); }
    list<T> la() { return new list<T>(a, a); }
    static list<T> make(T x) { return new list<T>(x); }
}

printf("%y\n", gn(1));
printf("%y\n", gh("x"));
printf("%y\n", gk(1, "u"));
printf("%y\n", gu(1, "u"));
GPair<int, string> p(5, "five");
printf("%y %y\n", p.hb(), p.la());
printf("%y\n", GPair<float, int>::make(2.5));
printf("%s %s\n", gn(1).fullType(), gu(1, 2.5).fullType());
