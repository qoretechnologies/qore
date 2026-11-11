%modern
list<T> sub c<T>() { return new list<T>(); }
list<T> sub b<T>() { return c<T>(); }
list<T> sub a<T>(T v) { return b<T>(); }
hash<string, U> sub two<T, U>() { return new hash<string, U>(); }
hash<string, U> sub fwd2<T, U>(T t, U u) { return two<T, U>(); }
hash<string, int> sub fwd3<T>(T t) { return two<T, int>(); }
hash<string, T> sub fwd4<T>(T t) { return two<int, T>(); }

class K<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    list<T> viaFunction() { return c<T>(); }
    list<T> viaMethod() { return self.own<T>(); }
    list<T> own<U>() { return new list<U>(); }
    static list<T> viaStatic() { return c<T>(); }
}

class Q {
    list<U> gm<U>(U u) { return c<U>(); }
}

printf("%s\n", a(1).fullType());
printf("%s\n", a("s").fullType());
printf("%s\n", a(1.5).fullType());
printf("%s\n", fwd2(1, "x").fullType());
printf("%s\n", fwd3("x").fullType());
printf("%s\n", fwd4(2.5).fullType());
K<float> k(1.5);
printf("%s\n", k.viaFunction().fullType());
printf("%s\n", k.viaMethod().fullType());
printf("%s\n", K<string>::viaStatic().fullType());
Q q();
printf("%s\n", q.gm(5).fullType());
printf("%s\n", q.gm<float>(5.0).fullType());
# the same generic function called with different types in turn
printf("%s %s %s\n", a(1).fullType(), a("s").fullType(), a(1).fullType());
