%modern
%allow-weak-references

our auto observed;
hash<auto> sub makeValue() {
    hash<auto> h = {"n": 1};
    observed := h;
    return h;
}

list<auto> sub makeList() {
    list<auto> l = (1,);
    observed := l;
    return l;
}

auto sub inspect(auto arg) {
    printf("untyped inside %y\n", observed.n);
    return arg;
}
auto sub inspectTyped(hash<auto> arg) {
    printf("typed inside %y\n", observed.n);
    return arg;
}
class Inspector {
    auto inspect(auto arg) {
        printf("method inside %y\n", observed.n);
        return arg;
    }
}
sub checkPair(auto first, auto second) {
    printf("pair %y %y\n", first, second);
}
sub stores() {
    try {
        auto x;
        checkPair(x = makeValue(), 0);
        x.n = 2;
        printf("try assign %y\n", observed.n);
        auto y;
        checkPair(y += makeValue(), 0);
        y.n = 3;
        printf("try add %y\n", observed.n);
        checkPair(x = makeValue(), x.n = 4);
        printf("try nested write %y\n", observed.n);
    } catch (hash<ExceptionInfo> ex) {
        printf("unexpected %s\n", ex.err);
    }
    auto z;
    checkPair(z = makeValue(), z.n = 5);
    printf("nested write %y\n", observed.n);
    hash<auto> t;
    checkPair(t = makeValue(), t.n = 6);
    printf("typed nested write %y\n", observed.n);
    auto l;
    checkPair(l = makeList(), l[0] = 7);
    printf("list nested write %y\n", observed);
    checkPair(z = makeValue(), z = {"other": 7});
    printf("replaced local %y\n", z);
}
sub arguments(bool choose) {
    Inspector obj();
    checkPair(inspect(makeValue()), observed.n);
    printf("after untyped %y\n", observed.n);
    checkPair(inspectTyped(makeValue()), observed.n);
    printf("after typed %y\n", observed.n);
    checkPair(obj.inspect(makeValue()), observed.n);
    printf("after method %y\n", observed.n);
    checkPair(inspect(choose ? makeValue() : makeValue()), observed.n);
    printf("after conditional %y\n", observed.n);
    try {
        for (int i = 0; i < 2; ++i) {
            auto item;
            checkPair(item += makeValue(), item.n = i);
            printf("iteration %d %y\n", i, observed.n);
        }
        checkPair(inspectTyped(makeValue()), observed.n);
        printf("after try argument %y\n", observed.n);
    } catch (hash<ExceptionInfo> ex) {
        printf("unexpected %s\n", ex.err);
    }
}

our int destroyed;
class Token {
    destructor() { ++destroyed; }
}
auto sub ordinary() { return new Token(); }
int sub consume(auto arg) { return 1; }
sub fail(auto arg) {
    printf("before throw %y\n", observed.n);
    throw "EXPECTED", "end the statement";
}
sub boundaries() {
    printf("ordinary %d %d\n", consume(ordinary()), destroyed);
    try {
        fail(makeValue());
    } catch (hash<ExceptionInfo> ex) {
        printf("caught %s %y\n", ex.err, observed.n);
    }
}

stores();
arguments(True);
boundaries();
