# -*- mode: qore; indent-tabs-mode: nil -*-
# the probes of explicit-type-param-method-call.qtest (qore#5892): calls of another object's generic method whose
# explicit type arguments name type parameters of the calling generic class

%modern

class Util {
    public {}

    X mid<X>(X x) { return x; }

    list<auto> two<X, Y>(X x, Y y) { return (x, y); }

    nothing put<X>(Queue q, X x) { q.push(x); }

    static X smid<X>(X x) { return x; }
}

X sub fmid<X>(X x) { return x; }

#! a generic class whose generic method takes a parameter of the class's type parameter as well
class Gen<U> {
    public {}

    list<auto> both<X>(X x, U u) { return (x, u); }
}

class Box<T> {
    public {
        # a closure in a member initializer
        code init_closure = auto sub (auto x) { Util u(); return u.mid<T>(x); };
    }

    auto direct(auto a) { Util u(); return u.mid<T>(a); }
    auto concrete(auto a) { Util u(); return u.mid<int>(a); }
    auto stat(auto a) { return Util::smid<T>(a); }
    auto func(auto a) { return fmid<T>(a); }
    auto closure(auto a) { code c = auto sub (auto x) { Util u(); return u.mid<T>(x); }; return c(a); }
    auto member(auto a) { return init_closure(a); }
    auto named(auto a) { Util u(); return u.mid<T>(x: a); }
    auto composite(auto a) { Util u(); return u.mid<list<T>>(a); }
    # the callee's receiver is a generic class of its own, whose type parameter its parameter types take
    auto gen(auto a, auto b) { Gen<string> g(); return g.both<T>(a, b); }
    # a nested generic class: its type argument is the type parameter of this class
    auto nested(auto a) { Box<list<T>> inner(); return inner.direct(a); }
    # the callee is another instantiation of the calling class: T is the caller's
    auto cross(auto a) { Box<string> o(); return o.ident<T>(a); }
    X ident<X>(X x) { return x; }
    # the parameter types of the callee take its own receiver's T
    auto crossT(auto a, auto b) { Box<string> o(); return o.withT<T>(a, b); }
    list<auto> withT<X>(X x, T t) { return (x, t); }
    # a typed argument: the variant is committed at parse time
    auto typed(T a) { Util u(); return u.mid<T>(a); }
    auto bg(auto a) {
        Util u();
        Queue q();
        background u.put<T>(q, a);
        return q.get(5s);
    }
}

class Pair<A, B> {
    auto swap(auto a, auto b) { Util u(); return u.two<B, A>(b, a); }
}

class IntBox inherits Box<int> {
}

string sub probe(code c) {
    try {
        return sprintf("%y", c());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    Box<int> bi();
    Box<string> bs();
    Pair<int, string> p();
    IntBox ib();
    list<string> l = (
        probe(auto sub () { return bi.direct(7); }),
        probe(auto sub () { return bs.direct("s"); }),
        # T is bound to int: a string is not accepted
        probe(auto sub () { return bi.direct("s"); }),
        probe(auto sub () { return bi.concrete(7); }),
        probe(auto sub () { return bi.stat(7); }),
        probe(auto sub () { return bi.func(7); }),
        probe(auto sub () { return bi.closure(7); }),
        probe(auto sub () { return bi.member(7); }),
        probe(auto sub () { return bi.named(7); }),
        probe(auto sub () { return bi.composite((1, 2)); }),
        probe(auto sub () { return bi.composite(("a",)); }),
        probe(auto sub () { return bi.gen(7, "s"); }),
        probe(auto sub () { return bi.nested((1, 2)); }),
        probe(auto sub () { return bi.cross(7); }),
        probe(auto sub () { return bs.cross("s"); }),
        probe(auto sub () { return bi.crossT(7, "s"); }),
        # T of the callee is string: an int is not accepted
        probe(auto sub () { return bi.crossT(7, 8); }),
        probe(auto sub () { return bi.typed(7); }),
        probe(auto sub () { return bi.bg(7); }),
        probe(auto sub () { return p.swap(1, "a"); }),
        probe(auto sub () { return ib.direct(7); }),
    );
    return foldl $1 + "|" + $2, l;
}
