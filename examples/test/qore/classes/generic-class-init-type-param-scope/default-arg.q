%modern

# a constant and a hashdecl whose types name a class T are first used in the default arguments of a generic function
# whose type parameter has the same name
class T {
    public {
        int x = 7;
    }
}

const NC = cast<*T>(new T());

hashdecl H {
    *T m;
}

auto sub get<T>(T a, int y = NC.x, hash<H> h = <H>{"m": new ::T()}) {
    return y + h.m.x;
}

sub check() {
    printf("default arguments: %y\n", get<int>(1));
}

check();
