# probes of generic-forward-type-param.qtest (qore#5791): generic code that forwards its type parameters as explicit
# type arguments, and explicit type arguments of calls whose variant is chosen at run time; probes() returns one line
# per pass, and each pass runs every probe, so that the code runs on every tier with tiered execution

%modern

X sub identity<X>(X x) { return x; }
X sub fwd<X>(X x) { return identity<X>(x); }
X sub fwd2<X>(X x) { return fwd<X>(x); }
list<X> sub fwd_list<X>(list<X> l) { return identity<list<X>>(l); }
X sub fwd_auto<X>(auto x) { return identity<X>(x); }

class C {
    static U sm<U>(U u) { return identity<U>(u); }
    U m<U>(U u) { return identity<U>(u); }
    U self_fwd<U>(U u) { return m<U>(u); }
    static U static_fwd<U>(U u) { return C::sm<U>(u); }
}

X sub to_static<X>(X x) { return C::sm<X>(x); }
X sub to_method<X>(C c, X x) { return c.m<X>(x); }

# the variant of these calls is chosen at run time, as the type of the argument is not known at parse time
auto sub plain_auto(auto v) { return identity<int>(v); }
auto sub sm_auto(auto v) { return C::sm<int>(v); }
auto sub m_auto(C c, auto v) { return c.m<int>(v); }

string sub t(code c) {
    try {
        auto v = c();
        return sprintf("%y %s", v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    C c();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s; %s\n", i,
            t(auto sub () { return fwd<int>(1); }),
            t(auto sub () { return fwd<string>("s"); }),
            t(auto sub () { return fwd2<int>(2); }),
            t(auto sub () { return fwd_list<int>((1, 2)); }),
            t(auto sub () { return fwd_auto<int>(3); }),
            t(auto sub () { return fwd_auto<int>("x"); }),
            t(auto sub () { return C::sm<int>(4); }),
            t(auto sub () { return c.m<string>("m"); }),
            t(auto sub () { return plain_auto(5); }),
            t(auto sub () { return plain_auto("y"); }),
            t(auto sub () { return sm_auto(6); }),
            t(auto sub () { return m_auto(c, 7); }),
            t(auto sub () { return to_static<int>(8); }),
            t(auto sub () { return to_method<string>(c, "9"); }),
            t(auto sub () { return c.self_fwd<int>(10); }),
            t(auto sub () { return C::static_fwd<string>("11"); }));
    }
    return rv;
}
