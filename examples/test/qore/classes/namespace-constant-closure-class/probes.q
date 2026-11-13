# probes of namespace-constant-closure-class.qtest (qore#5961): a closure in a namespace constant runs in no class
# context, also when the constant's value is first evaluated for a class constant or a static member initializer of a
# class, so it cannot read the private members of that class (PRIVATE-MEMBER), also through a closure that it creates,
# and neither can a closure in a hashdecl member initializer when the hashdecl is first used in a class; probes()
# returns one line per pass, so that the code runs on every tier with tiered execution

%modern

class B {
    public {
        const C2 = NS2::K3;
        static code s2 = NS2::K4;
        const C5 = NS2::K5;
    }
    private {
        int x = 7;
    }
    int get(code c) { return c(self); }
    static code mkh() { hash<H> h(); return h.hc; }
}

hashdecl H {
    code hc = auto sub (object o) { return o.x; };
}

namespace NS2 {
    const K3 = auto sub (object o) { return o.x; };
    const K4 = auto sub (object o) { return o.x; };
    const K5 = auto sub (B b) { return b.get(auto sub (object b2) { return b2.x; }); };
}

auto sub t(code c) {
    try {
        return c(new B());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        hash<H> h();
        rv += sprintf("%d: %y %y %y %y %y %y %y %y\n", i, t(NS2::K3), t(B::C2), t(NS2::K4), t(B::s2), t(NS2::K5),
            t(B::C5), t(B::mkh()), t(h.hc));
    }
    return rv;
}
