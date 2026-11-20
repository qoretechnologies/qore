# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# fixture of inherits-namespace-lookup.qtest (issue #6710): an unqualified parent class or hashdecl name is resolved
# in the namespace of the class or hashdecl that inherits it, as an unqualified class name in its code is

%modern
%no-child-restrictions

class RootOnly {
    string m() { return "::RootOnly"; }
}

class Own {
    string m() { return "::Own"; }
}

hashdecl OwnH {
    string from = "::OwnH";
}

class OwnGen<T> {
    string m() { return "::OwnGen"; }
}

hashdecl OwnGenH<T> {
    string from = "::OwnGenH";
}

public namespace N1 {
    public class A { string m() { return "N1::A"; } string a() { return "N1::A"; } static string s() { return "N1::A"; } }
    public class B { string m() { return "N1::B"; } static string s() { return "N1::B"; } }
    class OnlyN1 { string m() { return "N1::OnlyN1"; } }
    class Later { string m() { return "N1::Later"; } }
    hashdecl P { string from = "N1::P"; }
}

public namespace N2 {
    public class A { string m() { return "N2::A"; } string a() { return "N2::A"; } static string s() { return "N2::A"; } }
    public class B { string m() { return "N2::B"; } static string s() { return "N2::B"; } }
    hashdecl P { string from = "N2::P"; }

    # the issue's case: both parents are in this namespace and in N1
    class X inherits B, A {}

    # a parent that is only in the root namespace or only in another namespace is found as an unqualified class name
    # in code is
    class XRoot inherits RootOnly {}
    class XOther inherits OnlyN1 {}

    # a parent declared after its subclass
    class XLater inherits Later {}
    class Later { string m() { return "N2::Later"; } }

    # a hashdecl parent
    hashdecl H inherits P { int z = 1; }

    # a class or hashdecl cannot inherit itself: a parent with its own name is another one of the name
    class Own inherits Own {}
    hashdecl OwnH inherits OwnH { int z = 1; }
    class OwnGen<T> inherits OwnGen<int> {}
    hashdecl OwnGenH<T> inherits OwnGenH<int> { int z = 1; }
    # a scoped parent with the name of the inheriting class is another class
    class OwnScoped inherits Inner::OwnScoped {}

    # other unqualified class names in this namespace, as the reference
    class Ref {
        static string newB() { return (new B()).m(); }
        static string varB() { B b(); return b.m(); }
        static string castB() { object o = new N2::B(); return cast<B>(o).m(); }
        static string staticB() { return B::s(); }
    }

    namespace Inner {
        # a nested namespace: the parent is the class that the same name in code is
        class X inherits B {}
        class Ref { static string newB() { return (new B()).m(); } }
        class OwnScoped { string m() { return "N2::Inner::OwnScoped"; } }
    }
}

public namespace N1 {
    class Y inherits B {}
}

const Decls = "namespace N1 { class B { string m() { return \"N1::B\"; } } }\n"
    "namespace N2 { class B { string m() { return \"N2::B\"; } } }\n";
const Sub = "namespace N2 { class X inherits B {} }\nstring sub t() { return (new N2::X()).m(); }\n";

printf("issue: %s %s\n", (new N2::X()).m(), (new N2::X()).a());
printf("reference: %s %s %s %s\n", N2::Ref::newB(), N2::Ref::varB(), N2::Ref::castB(), N2::Ref::staticB());
printf("root only: %s\n", (new N2::XRoot()).m());
printf("other namespace only: %s\n", (new N2::XOther()).m());
printf("declared later: %s\n", (new N2::XLater()).m());
hash<N2::H> h();
printf("hashdecl: %s %d\n", h.from, h.z);
hash<N2::OwnH> oh();
printf("own name: %s %s %d\n", (new N2::Own()).m(), oh.from, oh.z);
hash<N2::OwnGenH<string>> ogh();
printf("own generic name: %s %s %d\n", (new N2::OwnGen<string>()).m(), ogh.from, ogh.z);
printf("own scoped name: %s\n", (new N2::OwnScoped()).m());
printf("nested namespace: %y\n", (new N2::Inner::X()).m() == N2::Inner::Ref::newB());
printf("N1: %s\n", (new N1::Y()).m());

{
    # the subclass is parsed and committed after its parents
    Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES | PO_ALLOW_REPARSE);
    p.parsePending(Decls, "decls");
    p.parseCommit();
    p.parsePending(Sub, "sub");
    p.parseCommit();
    printf("separate commits: %s\n", p.callFunction("t"));
}
{
    Program p(PO_NEW_STYLE | PO_NO_INHERIT_USER_CLASSES);
    p.parsePending(Decls, "decls");
    p.parsePending(Sub, "sub");
    p.parseCommit();
    printf("one commit: %s\n", p.callFunction("t"));
}
{
    # the parents are imported from this Program
    Program p(PO_NEW_STYLE);
    p.parse(Sub, "sub");
    printf("child Program: %s\n", p.callFunction("t"));
}
{
    # a namespace of the child Program; N1::B and N2::B are imported from this Program
    Program p(PO_NEW_STYLE);
    p.parse("namespace N3 { class B { string m() { return \"N3::B\"; } } class X inherits B {} }\n"
        "string sub t() { return (new N3::X()).m(); }\n", "sub");
    printf("child Program namespace: %s\n", p.callFunction("t"));
}
