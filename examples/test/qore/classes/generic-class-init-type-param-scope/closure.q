%modern

# the class is initialized while a closure in the generic function, whose type parameter has the class's type
# parameter's name, is parsed: the member access in the closure initializes it
class Box<T> {
    public {
        *T v;
    }

    constructor(*T v) {
        self.v = v;
    }
}

code sub mk<T>() {
    return auto sub (Box<T> b) { return b.v; };
}

sub check() {
    code c = mk<int>();
    printf("closure: %y\n", c(new Box<int>(2)));
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
