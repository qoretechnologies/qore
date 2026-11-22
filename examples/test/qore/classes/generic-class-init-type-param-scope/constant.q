%modern

# class and namespace constants whose initializers name a class T are first used in a generic function, whose type
# parameter has the same name: the initializers name the class
class T {
    public {
        int x = 7;
    }
}

class K {
    const C = cast<*T>(new T());
}

const NC = cast<*T>(new T());

auto sub get<T>(T a) {
    return K::C.x + NC.x;
}

sub check() {
    printf("constants: %y\n", get<int>(1));
}

check();
