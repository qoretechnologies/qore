%modern
list<U> sub mk0<U>() { return new list<U>(); }
hash<string, U> sub mkh<T, U>() { return new hash<string, U>(); }

class G<T> {
    constructor() {}
    list<U> nu<U>(int n) { return new list<U>(); }
    list<U> mu<U>() { return mk0<U>(); }
    list<T> both<U>() { return mk0<T>(); }
    hash<string, U> mixed<U>() { return mkh<T, U>(); }
    list<U> viaSelf<U>() { return self.nu<U>(1); }
    static list<U> su<U>() { return new list<U>(); }
}

class P {
    list<U> gm<U>() { return new list<U>(); }
}

G<int> g();
printf("%s\n", g.nu<string>(1).fullType());
printf("%s\n", g.mu<string>().fullType());
printf("%s\n", g.both<float>().fullType());
printf("%s\n", g.mixed<string>().fullType());
printf("%s\n", g.viaSelf<float>().fullType());
printf("%s\n", G<int>::su<string>().fullType());
G<string> gs();
printf("%s\n", gs.nu<int>(1).fullType());
printf("%s\n", gs.both<int>().fullType());
P p();
printf("%s\n", p.gm<float>().fullType());
# the explicit type argument of a call in generic code
list<U> sub forward<U>(U u) { return g.nu<U>(1); }
printf("%s %s\n", forward(1).fullType(), forward("s").fullType());
