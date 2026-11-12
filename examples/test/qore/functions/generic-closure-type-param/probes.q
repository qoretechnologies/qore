# probes of generic-closure-type-param.qtest (qore#5733): a closure in generic code names the type parameters of
# that code, in its signature and in its code, and is bound by the type arguments of the call that created it,
# wherever it is called; probes() returns one line per pass, and each pass runs every probe, so that the code runs on
# every tier with tiered execution

%modern

X sub identity<X>(X x) { return x; }

auto sub call(code c, auto v) { return c(v); }

class G {
    # a closure called in the generic method: its parameter and return types and an explicit type argument
    static auto inside<U>(U v) {
        code c = U sub (U a) { return identity<U>(a); };
        return c(v);
    }
    # a closure returned and called after the generic call has returned
    static code make<U>() {
        return U sub (auto a) { return identity<U>(a); };
    }
    # a closure in a closure
    static code nested<U>() {
        return code sub () { return auto sub (auto a) { return identity<U>(a); }; };
    }
    # a closure returned with a parameter typed with the type parameter
    static code typedParam<U>() {
        return U sub (U a) { return a; };
    }
    # a closure passed to another function, which calls it
    static auto callback<U>(auto v) {
        return call(auto sub (auto a) { return identity<U>(a); }, v);
    }
}

# a closure in a generic function
code sub make_fn<X>() { return auto sub (auto a) { return identity<X>(a); }; }

# a closure in a default argument of a generic function
code sub dflt<X>(code c = X sub (X a) { return a; }) { return c; }

# a closure in a method of a generic class, naming the type parameter of the class
class Box<T> {
    code make() { return auto sub (auto a) { return identity<T>(a); }; }
    # the type parameter of the class in the closure's parameter and return types
    code typedParam() { return auto sub (T a) { return a; }; }
    code typedReturn() { return T sub (auto a) { return identity<T>(a); }; }
}

# a class that is not generic, of which a base class binds the type parameter
class IntBox inherits Box<int> {}

string sub t(code c) {
    try {
        auto v = c();
        return sprintf("%y %s", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    code mi = G::make<int>();
    code ms = G::make<string>();
    code ni = G::nested<int>()();
    code fi = make_fn<int>();
    code ti = G::typedParam<int>();
    Box<int> b();
    code bi = b.make();
    code bp = b.typedParam();
    code br = b.typedReturn();
    IntBox ib();
    code ip = ib.typedParam();
    code di = dflt<int>();
    # a closure that is not in generic code
    code plain = int sub (int a) { return a; };
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; "
            "%s; %s\n", i,
            t(auto sub () { return G::inside<int>(1); }),
            t(auto sub () { return mi(2); }),
            t(auto sub () { return mi("a"); }),
            t(auto sub () { return ms("b"); }),
            t(auto sub () { return ms(3); }),
            t(auto sub () { return ni(4); }),
            t(auto sub () { return ni("c"); }),
            t(auto sub () { return G::callback<int>(5); }),
            t(auto sub () { return G::callback<int>("d"); }),
            t(auto sub () { return fi(6); }),
            t(auto sub () { return fi("e"); }),
            t(auto sub () { return bi(7); }),
            t(auto sub () { return bi("f"); }),
            t(auto sub () { return ti(8); }),
            t(auto sub () { return ti("g"); }),
            t(auto sub () { return bp(9); }),
            t(auto sub () { return bp("h"); }),
            t(auto sub () { return br(10); }),
            t(auto sub () { return br("i"); }),
            t(auto sub () { return ip(11); }),
            t(auto sub () { return ip("j"); }),
            t(auto sub () { return di(12); }),
            t(auto sub () { return di("k"); }),
            t(auto sub () { return plain(13); }));
    }
    return rv;
}
