# probes of method-default-arg-closure.qtest (qore#5898): a closure that is the default value of a method's parameter
# is created in the method's class context with its object, and is called with both (a debug build aborted in
# UserClosureVariant::evalClosure() on an object without a class): in a plain and in a generic class, a base class
# method called on an object of a subclass, a call from a method of another class, a constructor, private members and
# methods, a static method (no object), and member initializer closures, which run in the class that declares them,
# not the object's (a private:internal member of a subclass with the same name).  probes() returns one line per pass,
# so that the code runs on every tier with tiered execution

%modern

class Base {
    private {
        int base_priv = 10;
    }
    auto bm(code c = auto sub () { return base_priv + basePriv(); }) { return c(); }
    private int basePriv() { return 1; }
}

class B inherits Base {
    public {
        auto ctor_value;
    }
    private {
        int priv = 5;
    }
    constructor(code c = auto sub () { return priv * 2; }) { ctor_value = c(); }
    auto m(auto w, code c = auto sub (auto a) { return a; }) { return c(w); }
    auto member(code c = auto sub () { return priv; }) { return c(); }
    auto method(code c = auto sub () { return self.privm(); }) { return c(); }
    # a member read at run time: it depends on the class context of the closure
    auto dynamic(code c = auto sub () { return self.("priv"); }) { return c(); }
    private int privm() { return 6; }
    static auto sm(auto w, code c = auto sub (auto a) { return a; }) { return c(w); }
}

class IA {
    private:internal {
        int i = 1;
    }
    public {
        code direct = auto sub () { return i; };
        code dynamic = auto sub () { return self.("i"); };
    }
}

class IB inherits IA {
    private:internal {
        int i = 100;
    }
}

class Other {
    public {}
    auto callOther(B b) { return b.member() + b.bm() + b.dynamic(); }
}

class G<T> {
    private {
        T v;
    }
    constructor(T v) { self.v = v; }
    auto m(code c = auto sub () { return v; }) { return c(); }
}

string sub probes() {
    B b();
    G<string> g("g");
    Other other();
    IB ib();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y; %y\n", i, b.m(1), b.member(), b.method(),
            b.bm(), b.ctor_value, B::sm(2), g.m(), b.m(3, auto sub (auto a) { return a * 2; }), b.dynamic(),
            other.callOther(b), ib.direct(), ib.dynamic());
    }
    return rv;
}
