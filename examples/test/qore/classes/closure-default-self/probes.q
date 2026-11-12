# probes of closure-default-self.qtest (qore#5931): a default value of a closure created in a method that uses the
# method's self sees the object, also in qcc code, whose closures reach their object through their object context
# and not as a captured variable: self in a closure in the default value together with a captured local, self as the
# default value, a private method called through self, a closure of a base class method on an object of a subclass,
# a closure stored and called later, which does not keep the object alive, and a closure called from a method of
# another class; probes() returns one line per pass, so that the code runs on every tier with tiered execution

%modern

class Base {
    public {
        int a = 11;
        static int destroyed = 0;
    }
    destructor() { ++destroyed; }

    code withLocal() {
        int n = 3;
        return auto sub (code c = auto sub () { return self.a + n; }) { return c(); };
    }

    code selfDefault() {
        return auto sub (*Base o = self) { return o.a; };
    }

    code privateCall() {
        return auto sub (code c = auto sub () { return self.priv(); }) { return c(); };
    }

    private int priv() { return a * 2; }
}

class Sub inherits Base {
    constructor() { a = 20; }
}

class Other {
    int call(code c) { return c(); }
}

string sub stored() {
    code f;
    int destroyed = Base::destroyed;
    {
        Base b();
        f = b.withLocal();
        int v = f();
        delete b;
        return sprintf("%d %d", v, Base::destroyed - destroyed);
    }
}

string sub probes() {
    Base b();
    Sub s();
    Other o();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y; %y\n", i, b.withLocal()(), b.selfDefault()(), b.privateCall()(),
            s.withLocal()(), s.selfDefault()(), stored(), o.call(b.privateCall()));
    }
    return rv;
}
