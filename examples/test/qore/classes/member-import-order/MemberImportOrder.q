#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright (C) 2026 Qore Technologies, s.r.o.

# a hierarchy in which a private:internal member hides a member of the same name parses whatever the order of the
# parent classes, with the same storage (issue #6614); its output is MemberImportOrder.expected in every execution
# mode and in programs compiled with qcc

%modern

class C0 {
    public {
        int m0 = 10;
    }

    auto get0() {
        return m0;
    }

    set0(int v) {
        m0 = v;
    }
}

# C0 is inherited privately for this class only
class C1 inherits private:internal C0 {
    auto get1() {
        return m0;
    }

    set1(int v) {
        m0 = v;
    }
}

# a private:internal member of the same name, which is separate from C0's in every hierarchy
class X {
    private:internal {
        int m0 = 100;
    }

    auto getx() {
        return m0;
    }

    setx(int v) {
        m0 = v;
    }
}

class Y inherits X, C1 {
}

# the accessible member is imported first
class Z1 inherits C0, Y {
}

# the hidden member is imported first
class Z2 inherits Y, C0 {
}

# the same with a public member of C0's in a class between
class C2 inherits C0 {
}

class Z3 inherits C2, Y {
}

class Z4 inherits Y, C2 {
}

sub show(string name, object o) {
    printf("%s: m0 %y get0 %y get1 %y getx %y\n", name, o.m0, o.get0(), o.get1(), o.getx());
}

sub check(string name, object o) {
    show(name, o);
    # C0's member is one value for C0, C1 and the object
    o.set1(5);
    show(name + " after set1(5)", o);
    o.set0(6);
    show(name + " after set0(6)", o);
    # X's member is its own
    o.setx(7);
    show(name + " after setx(7)", o);
    o.m0 = 8;
    show(name + " after m0 = 8", o);
}

# the cases run more than once, so that the tiered execution mode compiles them
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    check("Z1 (C0, Y)", new Z1());
    check("Z2 (Y, C0)", new Z2());
    check("Z3 (C2, Y)", new Z3());
    check("Z4 (Y, C2)", new Z4());
}
