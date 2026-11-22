%modern

# a subclass of the generic class is initialized while the generic function, whose type parameter has the classes'
# type parameter's name, is parsed: its base class is initialized with it; the function's own type parameter is
# visible again after the classes are initialized
class Box<T> {
    public {
        *T v;
    }

    constructor(*T v) {
        self.v = v;
    }
}

class Child<T> inherits Box<T> {
    constructor(*T v) : Box(v) {
    }

    *T get() {
        return v;
    }

    set(*T n) {
        v = n;
    }
}

auto sub get<T>(T x) {
    auto v = (new Child<T>()).get();
    T y = x;
    return v ?? y;
}

sub check() {
    printf("subclass: %y\n", get<int>(4));
    Child<int> c(1);
    printf("new: %y\n", c.get());
    Child<string> b("s");
    printf("string: %y\n", b.get());
    try {
        # a value whose type is not known at parse time
        hash<auto> h = {"a": "x"};
        c.set(h.a);
        printf("no error\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}

check();
