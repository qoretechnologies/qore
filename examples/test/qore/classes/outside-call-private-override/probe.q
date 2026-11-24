#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# a call from outside the class hierarchy resolved when parsing (a typed call, a call reference through a typed
# receiver, a static method call) calls the method that a call resolved at run time calls: a private method that the
# caller cannot call hides the methods of the name of the parent classes of its class (qore#7211, qore#7218); the test
# compares the output of each execution mode and of qcc executables with probe.expected

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

#! parses the given program in a child Program and runs its functions: a call that cannot be resolved when parsing
#! is a parse error
sub parse_check(string label, string src, list<string> funcs) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    try {
        p.parse(src, label);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
        return;
    }
    foreach string f in (funcs) {
        check(label + " " + f, any sub () { return p.callFunction(f); });
    }
}

# a class tree: X1's private m() overrides P1's public m()
class P1 {
    string m() { return "P1::m"; }
}
class X1 inherits P1 {
    private string m() { return "X1::m"; }
}

# deeper: X3 inherits C3, whose private m() overrides R3's public m()
class R3 {
    string m() { return "R3::m"; }
}
class C3 inherits R3 {
    private string m() { return "C3::m"; }
}
class X3 inherits C3 {}

# the diamond of qore#7211: the private override in C2 hides R2::m() in the first branch; P2::m() is reached through D2
class R2 {
    string m() { return "R2::m"; }
}
class C2 inherits R2 {
    private string m() { return "C2::m"; }
}
class A2 inherits C2 {}
class P2 {
    string m() { return "P2::m"; }
}
class D2 inherits P2 {}
class B2 inherits P2 {
    string m() { return "B2::m"; }
}
class X2 inherits A2, D2, B2 {}

# a private method of the first parent class and a public method of a later one: the later one is called (qore#6704)
class A4 {
    private string m() { return "A4::m"; }
}
class B4 {
    string m() { return "B4::m"; }
}
class X4 inherits A4, B4 {}

# qore#7218: code of Z, which X5 reaches only through the private:internal edge of Y, has no private access to X5, so
# its calls are calls from outside the class hierarchy
class Z5 {
    static string typed(X5 o) { return o.m(); }
    static string untyped(auto o) { return o.m(); }
    static string ref(auto o) { code c = \o.m(); return c(); }
    static string com(auto o) { return call_object_method(o, "m"); }
}
class Y5 inherits private:internal Z5 {}
class P5 inherits Y5 {
    string m() { return "P5::m"; }
}
class X5 inherits P5 {
    private string m() { return "X5::m"; }
}

string sub untyped(auto o) {
    return o.m();
}
string sub ref(auto o) {
    code c = \o.m();
    return c();
}
string sub com(auto o) {
    return call_object_method(o, "m");
}
string sub callable(object o) {
    return o.hasCallableMethod("m") ? "yes" : "no";
}

X1 x1();
check("X1 typed", string sub () { return x1.m(); });
check("X1 through P1", string sub () { P1 p = x1; return p.m(); });
check("X1 untyped", string sub () { return untyped(x1); });
check("X1 call reference", string sub () { return ref(x1); });
check("X1 call_object_method", string sub () { return com(x1); });
check("X1 hasCallableMethod", string sub () { return callable(x1); });

X3 x3();
check("X3 typed", string sub () { return x3.m(); });
check("X3 untyped", string sub () { return untyped(x3); });
check("X3 call reference", string sub () { return ref(x3); });
check("X3 call_object_method", string sub () { return com(x3); });

X2 x2();
check("X2 typed", string sub () { return x2.m(); });
check("X2 typed call reference", string sub () { code c = \x2.m(); return c(); });
check("X2 untyped", string sub () { return untyped(x2); });
check("X2 call reference", string sub () { return ref(x2); });
check("X2 call_object_method", string sub () { return com(x2); });
check("X2 hasCallableMethod", string sub () { return callable(x2); });

X4 x4();
check("X4 typed", string sub () { return x4.m(); });
check("X4 typed call reference", string sub () { code c = \x4.m(); return c(); });
check("X4 untyped", string sub () { return untyped(x4); });

X5 x5();
check("Z5 typed", string sub () { return Z5::typed(x5); });
check("Z5 untyped", string sub () { return Z5::untyped(x5); });
check("Z5 call reference", string sub () { return Z5::ref(x5); });
check("Z5 call_object_method", string sub () { return Z5::com(x5); });
check("X5 typed outside", string sub () { return x5.m(); });

# the calls that are parse errors when the private override hides the parent class's method, as for a class with only
# a private method of the name
const Classes = "class P { string m() { return \"P::m\"; } static string sm() { return \"P::sm\"; } }
class X inherits P { private string m() { return \"X::m\"; } private static string sm() { return \"X::sm\"; } }
class R { string m() { return \"R::m\"; } static string sm() { return \"R::sm\"; } }
class C inherits R { private string m() { return \"C::m\"; } private static string sm() { return \"C::sm\"; } }
class A inherits C {}
class Q { string m() { return \"Q::m\"; } static string sm() { return \"Q::sm\"; } }
class D inherits Q {}
class B inherits Q { string m() { return \"B::m\"; } static string sm() { return \"B::sm\"; } }
class X2 inherits A, D, B {}
class O { private string m() { return \"O::m\"; } private static string sm() { return \"O::sm\"; } }
";
parse_check("typed call reference, tree", Classes + "string sub f() { X x(); code c = \\x.m(); return c(); }",
    ("f",));
parse_check("typed call reference, private only", Classes + "string sub f() { O o(); code c = \\o.m(); return c(); }",
    ("f",));
parse_check("static call, tree", Classes + "string sub f() { return X::sm(); }", ("f",));
parse_check("static call, private only", Classes + "string sub f() { return O::sm(); }", ("f",));
parse_check("static call reference, tree", Classes + "string sub f() { code c = \\X::sm(); return c(); }", ("f",));
parse_check("static call reference, private only", Classes + "string sub f() { code c = \\O::sm(); return c(); }",
    ("f",));
parse_check("static call, diamond", Classes + "string sub f() { return X2::sm(); }
string sub r() { code c = \\X2::sm(); return c(); }", ("f", "r"));
parse_check("call_static_method", Classes + "string sub f() { return call_static_method(\"X\", \"sm\"); }
string sub d() { return call_static_method(\"X2\", \"sm\"); }", ("f", "d"));
