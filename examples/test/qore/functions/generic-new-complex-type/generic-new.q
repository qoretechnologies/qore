%modern
hash<string, T> sub g<T>(T v) { return new hash<string, T>({"a": v}); }
list<T> sub gl<T>(T v) { return new list<T>(v); }

class GBox<T> {
    private { T v; }
    constructor(T v) { self.v = v; }
    hash<string, T> h() { return new hash<string, T>({"k": v}); }
    list<T> l() { return new list<T>(v); }
}

printf("%y\n", g(19));
printf("%y\n", g("s"));
printf("%y\n", gl(19));
printf("%y\n", gl(1.5));
GBox<int> b(7);
printf("%y\n", b.h());
printf("%y\n", b.l());
GBox<string> bs("x");
printf("%y\n", bs.h());
printf("%y\n", bs.l());
printf("%s\n", g(19).fullType());
printf("%s\n", gl("s").fullType());
