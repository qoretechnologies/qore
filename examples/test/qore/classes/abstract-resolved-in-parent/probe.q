#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a qcc executable resolves the abstract methods that a class inherits from a parent class as the interpreter does:
# an abstract method that the parent class implements with a method of another of its parent classes is not pending in
# the class, which does not implement it again with the first method of its whole hierarchy (qore#7219, qore#7206);
# the test compares the output of each execution mode and of qcc executables with probe.expected

%modern
%no-child-restrictions
%disable-warning non-existent-method-call

#! runs a call three times (a call site resolved at run time caches its method), printing its value or its error
sub check(string label, code c) {
    list<string> l = ();
    for (int i = 0; i < 3; ++i) {
        try {
            l += sprintf("%y", c());
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err;
        }
    }
    # printed once when every call gives the same outcome
    printf("%s: %s\n", label, (select l, $1 != l[0]) ? l.join(" | ") : l[0]);
}

# the classes of the issue: D2 implements Abs2's private abstract m() with P::m(); X5 reaches C's private m() through
# A first, which code of P cannot call
class R {
    string m() { return "R::m"; }
}
class C inherits R {
    private string m() { return "C::m"; }
    string cUntyped(auto o) { return o.m(); }
}
class A inherits C {}
class P {
    string m() { return "P::m"; }
    string pTyped(X5 o) { return o.m(); }
    string pUntyped(auto o) { return o.m(); }
    string pRef(auto o) { code c = \o.m(); return c(); }
    string pCom(auto o) { return call_object_method(o, "m"); }
}
class Abs2 {
    abstract private string m();
}
class D2 inherits Abs2, P {
    string dUntyped(auto o) { return o.m(); }
}
class X5 inherits A, D2 {
    string xUntyped(auto o) { return o.m(); }
}

# a public abstract method implemented the same way
class Abs3 {
    abstract string m();
}
class D3 inherits Abs3, P {}
class X6 inherits A, D3 {}

string sub untyped(auto o) {
    return o.m();
}
string sub com(auto o) {
    return call_object_method(o, "m");
}

X5 x5();
check("X5 P typed", string sub () { return x5.pTyped(x5); });
check("X5 P untyped", string sub () { return x5.pUntyped(x5); });
check("X5 P call reference", string sub () { return x5.pRef(x5); });
check("X5 P call_object_method", string sub () { return x5.pCom(x5); });
check("X5 C untyped", string sub () { return x5.cUntyped(x5); });
check("X5 D2 untyped", string sub () { return x5.dUntyped(x5); });
check("X5 X5 untyped", string sub () { return x5.xUntyped(x5); });
check("X5 outside untyped", string sub () { return untyped(x5); });
check("X5 outside call_object_method", string sub () { return com(x5); });

X6 x6();
check("X6 P untyped", string sub () { return x6.pUntyped(x6); });
check("X6 P call reference", string sub () { return x6.pRef(x6); });
check("X6 outside untyped", string sub () { return untyped(x6); });
