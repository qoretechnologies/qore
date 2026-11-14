# probes of closure-default-self.qtest (qore#5931): a default value of a closure created in a method that uses the
# method's self sees the object, also in qcc code, whose closures reach their object through their object context
# and not as a captured variable: self in a closure in the default value together with a captured local, self as the
# default value, a private method called through self, a closure of a base class method on an object of a subclass,
# closures stored and called later, which do not keep the object alive (also when their default value uses self) and
# whose default value does not see the caller's object after the object is deleted, and a closure called from a method
# of another class, also with a member and a method used without self; probes() returns one line per pass, so that
# the code runs on every tier with tiered execution

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

    code bare() {
        return auto sub (int x = priv(), int y = a) { return x + y; };
    }

    code selfOrNone() {
        return auto sub (auto o = self) { return o ? o.a : "none"; };
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
    public { int a = 99; }
    auto call(code c) { return c(); }
}

string sub stored(Other o) {
    int destroyed = Base::destroyed;
    Base b();
    code f = b.withLocal();
    code g = b.selfOrNone();
    int v = f();
    remove b;
    return sprintf("%d %d %y %y", v, Base::destroyed - destroyed, o.call(g), o.call(f));
}

string sub probes() {
    Base b();
    Sub s();
    Other o();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y; %y; %y; %y; %y; %y; %y\n", i, b.withLocal()(), b.selfDefault()(), b.privateCall()(),
            s.withLocal()(), s.selfDefault()(), stored(o), o.call(b.privateCall()) + o.call(b.bare()));
    }
    return rv;
}
