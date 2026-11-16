# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the probes of hashdecl-member-default-typeparam.qtest (qore#6240): member defaults of a generic hashdecl whose types
# use the hashdecl's own type parameter

%modern

hashdecl HD2<A, B> {
    *A a;
    *B b;
}

hashdecl HD<T> {
    *T a;
}

class C<T> {
    public {
        *T x;
    }
}

#! member defaults that make hashdecls with the hashdecl's type parameter X
hashdecl O<X> {
    hash<HD2<X, int>> n = new hash<HD2<X, int>>();
    hash<HD<X>> i();
    hash<HD<X>> c = cast<hash<HD<X>>>({});
    list<hash<HD<X>>> lh();
    *X v;
}

#! member defaults that make typed containers and an object of a generic class with the type parameter X
hashdecl OC<X> {
    list<X> l = cast<list<X>>(());
    hash<string, list<X>> hl = cast<hash<string, list<X>>>({});
    hash<string, X> hx();
    C<X> o = new C<X>();
}

#! a generic hashdecl with two type parameters, both in one member default
hashdecl O2<X, Y> {
    hash<HD2<Y, X>> n = new hash<HD2<Y, X>>();
}

#! the same member defaults without type parameters, a control
hashdecl P {
    hash<HD2<string, int>> n = new hash<HD2<string, int>>();
    hash<HD<string>> i();
    list<string> l = cast<list<string>>(());
}

string sub probe(string label, code c) {
    try {
        auto v = c();
        return sprintf("%s: %y %s", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s: %s", label, ex.err, ex.desc);
    }
}

list<string> sub probes() {
    return (
        probe("O<string>.n", auto sub () { hash<O<string>> h(); return h.n; }),
        probe("O<string>.i", auto sub () { hash<O<string>> h(); return h.i; }),
        probe("O<string>.c", auto sub () { hash<O<string>> h(); return h.c; }),
        probe("O<string>.lh", auto sub () { hash<O<string>> h(); return h.lh; }),
        probe("OC<string>.l", auto sub () { hash<OC<string>> h(); return h.l; }),
        probe("OC<string>.hl", auto sub () { hash<OC<string>> h(); return h.hl; }),
        probe("OC<string>.hx", auto sub () { hash<OC<string>> h(); return h.hx; }),
        # the object is a C<string>: its member takes no int
        probe("OC<string>.o", auto sub () {
            hash<OC<string>> h();
            try {
                h.o.x = 1;
            } catch (hash<ExceptionInfo> ex) {
                return ex.err;
            }
            return "assigned";
        }),
        probe("O<string>", auto sub () { hash<O<string>> h(); return h.fullType(); }),
        probe("O<int>.n new", auto sub () { return (new hash<O<int>>()).n; }),
        probe("O<int>.n cast", auto sub () { return cast<hash<O<int>>>({}).n; }),
        probe("O2<int, string>.n", auto sub () { hash<O2<int, string>> h(); return h.n; }),
        probe("P.n", auto sub () { hash<P> h(); return h.n; }),
        probe("P.i", auto sub () { hash<P> h(); return h.i; }),
        probe("P.l", auto sub () { hash<P> h(); return h.l; }),
    );
}
