%modern

# non-generic hashdecls whose members name a class T are initialized while a generic function, whose type parameter
# has the same name, is parsed: the members have the class's type
class T {
    public {
        int x = 7;
    }
}

hashdecl H {
    *T m;
}

hashdecl HD<U> {
    *T m;
    *U u;
}

class Box<V> {
    public {
        hash<H> h();
        *V v;
    }
}

auto sub plain<T>(T a) {
    hash<H> h = <H>{"m": new ::T()};
    return h.m.x;
}

auto sub generic<T>(T a) {
    hash<HD<int>> h();
    h.m = new ::T();
    return h.m.x;
}

auto sub member<T>(T a) {
    Box<T> b();
    b.h.m = new ::T();
    return b.h.m.x;
}

sub check() {
    printf("hashdecl: %y\n", plain<int>(1));
    printf("generic hashdecl: %y\n", generic<int>(1));
    printf("class member: %y\n", member<int>(1));
    try {
        hash<H> h();
        # a value whose type is not known at parse time
        hash<auto> v = {"a": 1};
        h.m = v.a;
        printf("no error\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}

check();
