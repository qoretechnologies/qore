#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# an abstract declaration is not implemented by a variant that the class reaches only through the private:internal
# inheritance of one of its parent classes: the class stays abstract; the test compares the output of each execution
# mode and of qcc executables with probe.expected

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

#! prints the outcome of calls of m() on an object of the class \a cn from outside the class hierarchy
sub calls(string cn) {
    check(cn + " create", string sub () { return get_class_name(create_object(cn)); });
    check(cn + " out", string sub () { auto o = create_object(cn); return o.m(); });
    check(cn + " ref", string sub () { auto o = create_object(cn); code c = \o.m(); return c(); });
    check(cn + " call_object_method", string sub () { return call_object_method(create_object(cn), "m"); });
    check(cn + " call_function_args", string sub () {
        auto o = create_object(cn);
        return call_function_args(\call_object_method(), (o, "m"));
    });
}

class Secret {
    string m() { return "Secret::m"; }
    string m(int i) { return sprintf("Secret::m(%d)", i); }
}
class Iface {
    abstract string m();
}
class IfaceInt {
    abstract string m(int i);
}

# the parent class inherits Secret with private:internal: its subclasses cannot reach it
class K2 inherits private:internal Secret {
    static string u(auto o) { return o.m(); }
}
class W2 inherits K2 {
    string w() { return self.m(); }
}

# 1: another parent class declares the abstract method: abstract
class J2 inherits K2, Iface {}
# 2: the class declares it itself: abstract
class J1 inherits K2 { abstract string m(); }
# 3: a subclass of the parent class: abstract
class K3 inherits K2 {}
class J3 inherits K3, Iface {}
# 4: the private:internal edge below a public one: abstract
class Mid inherits Secret {}
class K4 inherits private:internal Mid {}
class J4 inherits K4, Iface {}
# 5: the private:internal edge two levels down: abstract
class L5 inherits private:internal Secret {}
class M5 inherits L5 {}
class J5 inherits M5, Iface {}
# 6: a privately inherited parent class with the private:internal edge: abstract
class J6 inherits private K2, Iface {}
# 7: an overload: abstract
class J7 inherits K2, IfaceInt {}
# 8: the parent class has an overload of its own: abstract
class K8 inherits private:internal Secret {
    string m(int i) { return sprintf("K8::m(%d)", i); }
}
class J8 inherits K8, Iface {}
# 9: the class's own code calls the method
class J9 inherits K2, Iface {
    string w() { return m(); }
}
# 10: a parent class implements another overload with its own private:internal inheritance: abstract (the class
# that declares the overload comes first: the AOT loader keeps one parent class's abstract declarations of a name)
class P6 inherits private:internal Secret, Iface {}
class J10 inherits IfaceInt, P6 {}

# subclasses that implement the method can be instantiated
class J2s inherits J2 {
    string m() { return "J2s::m"; }
}
class J9s inherits J9 {
    string m() { return "J9s::m"; }
}

# controls: Secret is reached through another path that the class can follow: implemented
class P1 inherits K2, Secret, Iface {}
class P2 inherits K2, Iface, Secret {}
class P3 inherits K2, private Secret, Iface {}
class P4 inherits K2, Mid, Iface {}
class P5 inherits K2, Iface, IfaceInt, Secret {}
# the class's own private:internal inheritance: implemented, the class's own code can reach Secret
class P7 inherits private:internal Secret { abstract string m(); }
# a parent class that implements the method itself: implemented with its method
class P8 inherits Iface, P7 {}
class P9 inherits P6, Iface {}
# the overload through a public edge: implemented
class P10 inherits Secret, Iface {}
class P11 inherits P10, IfaceInt {}

check("K2 out", string sub () { auto o = new K2(); return o.m(); });
check("K2 self", string sub () { return K2::u(new K2()); });
check("W2 self.m", string sub () { return (new W2()).w(); });

foreach string cn in ("J2", "J1", "J3", "J4", "J5", "J6", "J7", "J8", "J9", "J10") {
    check(cn + " create", string sub () { return get_class_name(create_object(cn)); });
}

calls("J2s");
check("J2s typed", string sub () { J2s o(); return o.m(); });
check("J9s w", string sub () { J9s o(); return o.w(); });
foreach string cn in ("P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8", "P9", "P11") {
    calls(cn);
}
check("P5 m(int)", string sub () { auto o = new P5(); return o.m(1); });
check("P11 m(int)", string sub () { auto o = new P11(); return o.m(1); });
check("P11 m(int) call_object_method", string sub () { return call_object_method(new P11(), "m", 1); });
