%modern

# a typedef whose type names a class T is first used in a generic function, and another one in a method of a generic
# class, whose type parameters have the same name: the typedefs have the class's type, which they keep for every
# later use
class T {
    public {
        int x = 7;
    }
}

typedef TD = *T;
typedef TC = *T;

auto sub get<T>(T a) {
    TD v = new ::T();
    return v.x;
}

class C<T> {
    auto m(T a) {
        TC v = new ::T();
        return v.x;
    }
}

sub check() {
    printf("function: %y\n", get<int>(1));
    printf("method: %y\n", (new C<int>()).m(1));
    try {
        # a value whose type is not known at parse time
        hash<auto> h = {"a": 1};
        TD v = h.a;
        printf("no error: %y\n", v);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s\n", ex.err);
    }
}

check();
