#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# fixture of private-internal-member-init.qtest: each class's private:internal members are separate and initialized
# for every object of the class or of a subclass, however deep (issue #6402)

%modern
%enable-all-warnings

# issue: a member with the same name in a class and in its parent
class B0 {
    private:internal { int x = 0; }
    int g0() { return x; }
}
class B1 inherits B0 {
    private:internal { int x = 1; }
    int g1() { return x; }
}
class S inherits B1 {}
class S2 inherits S {}

# a subclass that also inherits a public member with the same name from another parent
class PubX {
    public { int x = 9; }
    int px() { return x; }
}
class W inherits S, PubX {}
class W2 inherits W {}

# the same name at five levels, with levels between them that do not declare it
class C0 {
    private:internal { int x = 0; }
    int g0() { return x; }
    set0(int v) { x = v; }
}
class C1 inherits C0 {
    private:internal { int x = 1; }
    int g1() { return x; }
    set1(int v) { x = v; }
}
class C2 inherits C1 {
    private:internal { int x = 2; }
    int g2() { return x; }
    code c2() { return int sub () { return x; }; }
}
class C3 inherits C2 {}
class C4 inherits C3 {
    private:internal { int x = 4; }
    int g4() { return x; }
    code c4() { return int sub () { return x; }; }
}
class C5 inherits C4 {}
class C6 inherits C5 {
    private:internal { int x = 6; }
    int g6() { return x; }
}
class C7 inherits C6 {}

# non-adjacent levels
class D0 {
    private:internal { int x = 0; }
    int g0() { return x; }
}
class D1 inherits D0 {}
class D2 inherits D1 {
    private:internal { int x = 2; }
    int g2() { return x; }
}
class D3 inherits D2 {}

# members without initializers, assigned in the constructors
class N0 {
    private:internal { *int x; int y; }
    constructor() { x = 100; }
    *int g0() { return x; }
    *int y0() { return y; }
}
class N1 inherits N0 {
    private:internal { *int x; int y; }
    constructor() { x = 101; y = 11; }
    *int g1() { return x; }
    *int y1() { return y; }
}
class N2 inherits N1 {}

# a private:internal member and a public member with the same name in a subclass (a private:internal member
# cannot have the name of an inherited private or public member)
class F0 {
    private:internal { int x = 0; }
    int g0() { return x; }
}
class F1 inherits F0 {
    public { int x = 1; }
    int g1() { return x; }
}
class F2 inherits F1 {
    int g2() { return x; }
}
class F3 inherits F2 {}

# multiple inheritance: the same name in two parents, with and without one in the class
class P1 {
    private:internal { int x = 10; }
    int p1() { return x; }
}
class P2 {
    private:internal { int x = 20; }
    int p2() { return x; }
}
class M inherits P1, P2 {
    private:internal { int x = 30; }
    int m() { return x; }
}
class M2 inherits M {}
class MM inherits P1, P2 {}
class MM2 inherits MM {}

# a diamond: VB is inherited once
class VB {
    private:internal { int x = 0; }
    int v() { return x; }
    setv(int n) { x = n; }
}
class L inherits VB {
    private:internal { int x = 1; }
    int l() { return x; }
}
class R inherits VB {
    private:internal { int x = 2; }
    int r() { return x; }
}
class Dm inherits L, R {
    private:internal { int x = 3; }
    int d() { return x; }
}
class Dm2 inherits Dm {}
class Dn inherits L, R {}
class Dn2 inherits Dn {}

# object members: each level's object is destroyed with the object
class T {
    public {
        static list<string> log = ();
        string n;
    }
    constructor(string n) { self.n = n; }
    destructor() { T::log += n; }
}
class O0 {
    private:internal { T t = new T("O0"); }
    string g0() { return t.n; }
}
class O1 inherits O0 {
    private:internal { T t = new T("O1"); }
    string g1() { return t.n; }
}
class O2 inherits O1 {
    private:internal { T t = new T("O2"); }
    string g2() { return t.n; }
}
class O3 inherits O2 {}

# transient members and serialization
class Z0 inherits Serializable {
    private:internal { int x = 0; transient int t = 50; }
    int g0() { return x; }
    int t0() { return t; }
    set0(int v) { x = v; }
}
class Z1 inherits Z0 {
    private:internal { int x = 1; transient int t = 51; }
    int g1() { return x; }
    int t1() { return t; }
    set1(int v) { x = v; }
}
class Z2 inherits Z1 {}

# static private:internal members with the same name
class St0 {
    private:internal { static int x = 0; }
    static int g0() { return x; }
    int i0() { return x; }
}
class St1 inherits St0 {
    private:internal { static int x = 1; }
    static int g1() { return x; }
    int i1() { return x; }
}
class St2 inherits St1 {}

# copy() with copy() methods at the levels
class K0 {
    private:internal { int x = 0; list<int> l = (0,); }
    int g0() { return x; }
    list<int> l0() { return l; }
    set0(int v) { x = v; l += v; }
    copy() { l += -1; }
}
class K1 inherits K0 {
    private:internal { int x = 1; list<int> l = (1,); }
    int g1() { return x; }
    list<int> l1() { return l; }
    set1(int v) { x = v; l += v; }
    copy() { l += -2; }
}
class K2 inherits K1 {}

sub check(string label, auto v) {
    printf("%s: %y\n", label, v);
}

sub run(string label, code c) {
    try {
        check(label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

run("B1", auto sub () { B1 o(); return (o.g0(), o.g1()); });
run("S", auto sub () { S o(); return (o.g0(), o.g1()); });
run("S2", auto sub () { S2 o(); return (o.g0(), o.g1()); });
run("W", auto sub () { W o(); return (o.g0(), o.g1(), o.px(), o.x); });
run("W2", auto sub () { W2 o(); return (o.g0(), o.g1(), o.px(), o.x); });

run("C7", auto sub () {
    C7 o();
    list<auto> rv = (o.g0(), o.g1(), o.g2(), o.c2()(), o.g4(), o.c4()(), o.g6());
    o.set0(-10);
    o.set1(-11);
    return rv + (o.g0(), o.g1(), o.g2(), o.g4(), o.g6());
});
run("C5", auto sub () { C5 o(); return (o.g0(), o.g1(), o.g2(), o.c2()(), o.g4(), o.c4()()); });
run("C3", auto sub () { C3 o(); return (o.g0(), o.g1(), o.g2(), o.c2()()); });

run("D2", auto sub () { D2 o(); return (o.g0(), o.g2()); });
run("D3", auto sub () { D3 o(); return (o.g0(), o.g2()); });

run("N1", auto sub () { N1 o(); return (o.g0(), o.g1(), o.y0(), o.y1()); });
run("N2", auto sub () { N2 o(); return (o.g0(), o.g1(), o.y0(), o.y1()); });

run("F2", auto sub () { F2 o(); return (o.g0(), o.g1(), o.g2(), o.x); });
run("F3", auto sub () { F3 o(); return (o.g0(), o.g1(), o.g2(), o.x); });

run("M", auto sub () { M o(); return (o.p1(), o.p2(), o.m()); });
run("M2", auto sub () { M2 o(); return (o.p1(), o.p2(), o.m()); });
run("MM", auto sub () { MM o(); return (o.p1(), o.p2()); });
run("MM2", auto sub () { MM2 o(); return (o.p1(), o.p2()); });

run("Dm", auto sub () { Dm o(); return (o.v(), o.l(), o.r(), o.d()); });
run("Dm2", auto sub () {
    Dm2 o();
    list<auto> rv = (o.v(), o.l(), o.r(), o.d());
    o.setv(-1);
    return rv + (o.v(), o.l(), o.r(), o.d());
});
run("Dn2", auto sub () { Dn2 o(); return (o.v(), o.l(), o.r()); });

run("O3", auto sub () {
    T::log = ();
    {
        O3 o();
        check("O3 members", (o.g0(), o.g1(), o.g2()));
    }
    return T::log;
});
run("O2", auto sub () {
    T::log = ();
    {
        O2 o();
        check("O2 members", (o.g0(), o.g1(), o.g2()));
        delete o;
    }
    return T::log;
});

run("Z2", auto sub () {
    Z2 o();
    o.set0(7);
    o.set1(8);
    list<auto> rv = (o.g0(), o.g1(), o.t0(), o.t1());
    Z2 c = Serializable::deserialize(o.serialize());
    return rv + (c.g0(), c.g1(), c.t0(), c.t1());
});

run("St2", auto sub () { St2 o(); return (St0::g0(), St1::g1(), o.i0(), o.i1()); });

run("K2", auto sub () {
    K2 o();
    o.set0(5);
    o.set1(6);
    K2 c = o.copy();
    c.set0(15);
    c.set1(16);
    return (o.g0(), o.g1(), o.l0(), o.l1(), c.g0(), c.g1(), c.l0(), c.l1());
});
