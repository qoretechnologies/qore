# probes of static-default-closure-class.qtest (qore#5907): a closure parsed in a class without an object context
# runs in the class context of its class: a closure as a static method's default value calls a private static method
# of its class also when the call comes from another class or from a subclass, as do a closure in a static method and
# one in a static member initializer, and a closure as a static method's default value that reads a private member or
# makes a call reference to a private static method of its class when called from another class; native code checks
# the access to the private method when it runs, so the closure needs its class also in a qcc executable; probes()
# returns one line per pass, so that the code runs on every tier with tiered execution

%modern

class B {
    public {
        static code smc = auto sub () { return B::sp() + 1; };
    }

    private {
        int x = 7;
    }

    static auto spm(code c = auto sub () { return B::sp(); }) { return c(); }
    static auto body() { code c = auto sub () { return B::sp() * 2; }; return c(); }
    static auto spv() { return sp(); }
    static auto pm(code c = auto sub (B b) { return b.x; }) { return c(new B()); }
    static auto cr(code c = auto sub () { return \B::sp(); }) { return c()(); }
    private static int sp() { return 42; }
}

class C {
    static auto callFromOther() { return B::spm(); }
    static auto pm() { return B::pm(); }
    static auto cr() { return B::cr(); }
    private static int sp() { return -1; }
}

class D inherits B {
    static auto viaSub() { return B::spm(); }
}

string sub probes() {
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y %y %y %y %y %y %y %y\n", i, B::spm(), C::callFromOther(), D::viaSub(), B::body(),
            B::smc(), B::spv(), C::pm(), C::cr());
    }
    return rv;
}
