%modern

# the class is initialized while the member initializer of a generic hashdecl, whose type parameter has the class's
# type parameter's name, is parsed: the class's member and constructor parameter types are its own
class Box<T> {
    public {
        *T v;
    }

    constructor(*T v) {
        self.v = v;
    }
}

hashdecl HD<T> {
    auto x = (new Box<T>()).v;
}

sub check() {
    hash<HD<int>> h();
    printf("hashdecl: %y\n", h.x);
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
