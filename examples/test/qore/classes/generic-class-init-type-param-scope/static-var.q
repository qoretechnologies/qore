%modern

# a static variable of a non-generic class whose type names a class T is first used in a generic function, whose type
# parameter has the same name: the variable has the class's type
class T {
    public {
        int x = 7;
    }
}

class K {
    public {
        static *T sv = new T();
    }
}

auto sub get<T>(T a) {
    return K::sv.x;
}

sub check() {
    printf("static: %y\n", get<int>(1));
    try {
        # a value whose type is not known at parse time
        hash<auto> v = {"a": 1};
        K::sv = v.a;
        printf("no error\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}

check();
