# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A subclass cannot use a private:internal member of its base class through a variable of the base class's type, as it
# cannot through a variable of its own type or through auto and object (#7258); see internal-member-base-typed.qtest

%modern
%no-child-restrictions

#! the classes of every case: A declares the member and uses it through references, Mid inherits A with
#! private:internal, and the subclasses read it in the way of the case
const Header = "
class A {
    public {
        int pub = 2;
    }
    private:internal {
        int x = 1;
    }
    int getX() { return x; }

    # the class that declares the member uses it through any reference
    static auto viaBase(A a) { return a.x; }
    static auto viaAuto(auto a) { return a.x; }
    static auto writeViaAuto(auto a) { a.x = 5; return a.x; }
}
class Mid inherits private:internal A {
}
";

#! the cases: the name, and the code of a subclass of Mid or A whose static method f is called with a new object of the
#! subclass
const Cases = (
    ("an unrelated class, a base-typed variable", "class S { static auto f(A a) { return a.x; } }", "S", False),
    ("a subclass of A, a base-typed variable", "class S inherits A { static auto f(A a) { return a.x; } }", "S", True),
    ("a subclass of A, an optional base-typed variable",
        "class S inherits A { static auto f(*A a) { return a.x; } }", "S", True),
    ("a subclass of A, its own type", "class S inherits A { static auto f(S s) { return s.x; } }", "S", True),
    ("a subclass of A, auto", "class S inherits A { static auto f(auto a) { return a.x; } }", "S", True),
    ("a subclass of A, object", "class S inherits A { static auto f(object a) { return a.x; } }", "S", True),
    ("a subclass of A, a dynamic access",
        "class S inherits A { static auto f(auto a) { return a.(\"x\"); } }", "S", True),
    ("a subclass of A, a base-typed write",
        "class S inherits A { static auto f(A a) { a.x = 7; return a.getX(); } }", "S", True),
    ("a subclass of A, an auto write",
        "class S inherits A { static auto f(auto a) { a.x = 7; return a.getX(); } }", "S", True),
    ("a subclass of A, a method of A through a base-typed variable",
        "class S inherits A { static auto f(A a) { return a.getX(); } }", "S", True),
    ("a subclass of A, the public member through a base-typed variable",
        "class S inherits A { static auto f(A a) { return a.pub; } }", "S", True),
    ("a subclass of Mid, a base-typed variable", "class S inherits Mid { static auto f(A a) { return a.x; } }", "S",
        True),
    ("a subclass of Mid, auto", "class S inherits Mid { static auto f(auto a) { return a.x; } }", "S", True),
    ("A's code, a base-typed variable", "class S inherits A { static auto f(A a) { return A::viaBase(a); } }", "S",
        True),
    ("A's code, auto, on an object of a subclass of Mid",
        "class S inherits Mid { static auto f(Mid m) { return A::viaAuto(m); } }", "S", True),
    ("A's code, an auto write, on an object of a subclass of Mid",
        "class S inherits Mid { static auto f(Mid m) { return A::writeViaAuto(m); } }", "S", True),
);

#! runs a case in a child Program, and returns what the parse or the call gives
string sub run(string name, string code, string cls, bool create) {
    Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS);
    try {
        p.parse(Header + code, "case");
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: parse: %s", name, (ex.desc =~ x/^([^\n]*)/)[0]);
    }
    try {
        return sprintf("%s: %y", name, p.callFunction("run_" + cls));
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("%s: %s: %s", name, ex.err, ex.desc);
    }
}

foreach list<auto> c in (Cases) {
    # the case calls its method f with a new object of its class; the function that does it is part of the Program
    string call = sprintf("auto sub run_%s() { return %s::f(new %s()); }", c[2], c[2], c[2]);
    printf("%s\n", run(c[0], c[1] + "\n" + call, c[2], c[3]));
}
