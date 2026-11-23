%modern

# the class is initialized while the generic function, whose type parameter has the class's type parameter's name, is
# parsed: the call is parsed before the top-level code that uses the class
class Box<T> {
    public {
        *T v;
    }

    constructor(*T v) {
        self.v = v;
    }
}

auto sub get<T>(T x) {
    auto v = (new Box<T>()).v;
    # the function's own type parameter after the class is initialized
    T y = x;
    return v ?? y;
}

sub check() {
    printf("get: %y\n", get<int>(3));
    printf("new: %y\n", new Box<int>(1).v);
    Box<string> b("s");
    printf("string: %y\n", b.v);
    try {
        Box<int> i();
        i.v = "x";
        printf("no error\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}

check();
