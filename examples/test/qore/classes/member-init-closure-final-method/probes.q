# probes of member-init-closure-final-method.qtest (qore#5901): a closure in a member initializer that calls a final
# method of its class (also in a try block and through self, a private final method, a method of a final class, a
# final method of a base class from a subclass's member initializer, also when the subclass has a static method with
# the same name, which the call does not resolve to) is read from a qcc executable, whose AOT image reads the closures
# of member initializers before the methods of the classes are committed; probes() returns one line per pass, so that
# the code runs on every tier with tiered execution

%modern

class P {
    public {
        code c = auto sub (int a) { return fm(a); };
        code t = auto sub (int a) { try { return fm(a); } catch (hash<ExceptionInfo> ex) { return ex.err; } };
        code p = auto sub (int a) { return pfm(a) + self.fm(a); };
    }
    final string fm(int x) { return "fm" + x; }
    private final string pfm(int x) { return "pfm" + x; }
}

final class F {
    public {
        code c = auto sub (int a) { return m(a); };
    }
    string m(int x) { return "F" + x; }
}

class S inherits P {
    public {
        code s = auto sub (int a) { return fm(a + 10); };
    }
}

class Q {
    final string x() { return "Q"; }
}

class QS inherits Q {
    public {
        code c = auto sub () { return self.x() + x(); };
    }
    static string x() { return "QS"; }
}

string sub probes() {
    P p();
    F f();
    S s();
    QS qs();
    string rv;
    for (int i = 0; i < 8; ++i) {
        rv += sprintf("%d: %y %y %y %y %y %y\n", i, p.c(1), p.t(2), p.p(3), f.c(4), s.s(5), qs.c());
    }
    return rv;
}
