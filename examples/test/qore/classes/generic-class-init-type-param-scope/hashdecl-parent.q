%modern

# a generic hashdecl that inherits a generic parent, both with a type parameter T, is first used in a generic function
# whose type parameter is also named T: the hashdecls' members have the hashdecl's type argument
hashdecl P<T> {
    *T a;
}

hashdecl HD<T> inherits P<T> {
    *T b;
    code c = auto sub (T x) { return x; };
}

auto sub get<T>(T a) {
    hash<HD<int>> h();
    h.a = 1;
    h.b = 2;
    return h.c(3) + h.a + h.b;
}

sub check() {
    printf("inherited: %y\n", get<string>("x"));
    try {
        # a value whose type is not known at parse time
        hash<auto> v = {"a": "s"};
        hash<HD<int>> h();
        h.a = v.a;
        printf("no error\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}

check();
