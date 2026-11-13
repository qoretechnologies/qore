%modern

# a hash literal whose value type is taken from the type of the lvalue that it is assigned to, where that type has a
# type parameter (qore#5872)
class GenericHashLiteralBox<T> {
    public {
        T v;
        hash<string, T> h;
        *hash<string, T> oh;
        list<T> l;
        hash<string, list<T>> hl;
        list<hash<string, T>> lh;
    }

    constructor(T v) {
        self.v = v;
    }

    T get() {
        return v;
    }

    auto member() {
        h = {"a": v};
        return h;
    }

    auto orNothingMember() {
        oh = {"b": v, "c": get()};
        return oh;
    }

    auto call() {
        h = {"d": get()};
        return h;
    }

    auto empty() {
        h = {};
        return h;
    }

    auto list() {
        l = (v, v);
        return l;
    }

    auto nestedList() {
        hl = {"e": (v,)};
        return hl;
    }

    auto listOfHashes() {
        lh = ({"f": v},);
        return lh;
    }

    hash<string, T> local() {
        hash<string, T> x = {"g": v};
        return x;
    }

    auto closure() {
        code f = sub () { h = {"i": v}; };
        f();
        return h;
    }

    setH(hash<string, T> x) {
        h = x;
    }

    auto backgroundArg() {
        Counter c(1);
        background bgSet(c);
        c.waitForZero();
        return h;
    }

    private bgSet(Counter c) {
        on_exit c.dec();
        h = {"j": v};
    }

    auto wrongType() {
        auto x = "str";
        h = {"k": x};
        return h;
    }
}

hash<string, T> sub generic_hash_literal_func<T>(T v) {
    hash<string, T> x = {"m": v};
    return x;
}

string sub generic_hash_literal_show(code c) {
    try {
        auto v = c();
        return sprintf("%y (%s)", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err + ": " + ex.desc;
    }
}

string sub probes() {
    GenericHashLiteralBox<int> bi(1);
    GenericHashLiteralBox<string> bs("x");
    list<string> l = (
        generic_hash_literal_show(\bi.member()),
        generic_hash_literal_show(\bi.orNothingMember()),
        generic_hash_literal_show(\bi.call()),
        generic_hash_literal_show(\bi.empty()),
        generic_hash_literal_show(\bi.list()),
        generic_hash_literal_show(\bs.nestedList()),
        generic_hash_literal_show(\bs.listOfHashes()),
        generic_hash_literal_show(\bi.local()),
        generic_hash_literal_show(\bi.closure()),
        generic_hash_literal_show(\bi.backgroundArg()),
        generic_hash_literal_show(\bi.wrongType()),
        generic_hash_literal_show(auto sub () { return generic_hash_literal_func<int>(3); }),
        generic_hash_literal_show(auto sub () { return generic_hash_literal_func<string>("y"); }),
    );
    return foldl $1 + "\n" + $2, l;
}
