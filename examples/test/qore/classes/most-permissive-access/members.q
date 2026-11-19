%modern
# qore#6523: a member of a class reached through a private:internal path and another path (see
# most-permissive-access.qtest)

class C0 {
    public { int m0 = 10; }
    auto get0() { return m0; }
    set0(int v) { m0 = v; }
}
class C1 inherits private:internal C0 {
    auto get1() { return m0; }
    set1(int v) { m0 = v; }
}
class C2 inherits private:internal C0 {
    auto get2() { return m0; }
    set2(int v) { m0 = v; }
}
class C4 inherits C1 {}
# C0 through a private:internal path and a public path, in both orders
class C3 inherits C1, C0 {}
class C3b inherits C0, C1 {}
# qore#6523
class C6 inherits C3, C4 {}
class C6b inherits C4, C3 {}
# C0 through two private:internal paths
class C5 inherits C1, C2 {}
# a class that declares the member, hidden from its subclasses, beside C0 reached through two paths
class X { private:internal { int m0 = 100; } auto getx() { return m0; } setx(int v) { m0 = v; } }
class Z inherits X, C3 {}
# C3 inherited with private:internal access: its subclass keeps C0's member in one storage of its own
class P inherits private:internal C3 {
    auto getp() { return m0; }
    auto getp0() { return get0(); }
    auto getp1() { return get1(); }
    setp0(int v) { set0(v); }
    setp1(int v) { set1(v); }
}
class W inherits P {}
class W5 inherits P, C5 {}
# C0 through a private path and a public path
class B inherits private C0 {}
class D inherits B, C0 {}

# C0 through a private:internal path, where a private:internal member of K hides it, and a public path
class K inherits private:internal C0 {
    private:internal { int m0 = 50; }
    auto getk() { return m0; }
}
class L inherits K, C0 {}
# C0 through two private:internal paths, with a private:internal member of X visible first on both
class Y inherits X, C1 {}
class Y2 inherits C2, X {}
class M inherits Y, Y2 {}

# the values that each class context reads after each write
list<auto> sub views(object o, list<string> getters, list<string> setters) {
    list<auto> rv = ();
    rv += (map call_object_method(o, $1), getters);
    foreach string s in (setters) {
        call_object_method(o, s, $# + 20);
        rv += (map call_object_method(o, $1), getters);
    }
    return rv;
}

C3 c3();
printf("C3: %y %y\n", c3.m0, views(c3, ("get0", "get1"), ("set0", "set1")));
C3b c3b();
printf("C3b: %y %y\n", c3b.m0, views(c3b, ("get0", "get1"), ("set0", "set1")));
C6 c6();
printf("C6: %y %y\n", c6.m0, views(c6, ("get0", "get1"), ("set0", "set1")));
c6.m0 = 7;
printf("C6 public write: %y %y\n", c6.get0(), c6.get1());
C6b c6b();
printf("C6b: %y %y\n", c6b.m0, views(c6b, ("get0", "get1"), ("set0", "set1")));
C5 c5();
printf("C5: %y\n", views(c5, ("get1", "get2"), ("set1", "set2")));
Z z();
printf("Z: %y %y\n", z.m0, views(z, ("getx", "get0", "get1"), ("setx", "set0", "set1")));
W w();
printf("W: %y\n", views(w, ("getp", "getp0", "getp1"), ("setp0", "setp1")));
W5 w5();
printf("W5: %y\n", views(w5, ("getp", "getp0", "getp1", "get1", "get2"), ("setp0", "setp1", "set1", "set2")));
D d();
printf("D: %y %y\n", d.m0, views(d, ("get0",), ("set0",)));
L l();
printf("L: %y %y\n", l.m0, views(l, ("getk", "get0"), ("set0",)));
M mm();
printf("M: %y\n", views(mm, ("getx", "get1", "get2"), ("setx", "set1", "set2")));
