# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The default arguments of closures that call functions and use other symbols of the Program (#5827); see
# aot-closure-default-call.qtest
%modern

int sub g() { return 1; }

# a closure in a function, whose default calls a function declared before and after it
string sub f1() {
    code cl = string sub (int z = g(), int y = h()) { return sprintf("z=%d y=%d", z, y); };
    return cl();
}
# a lambda
string sub f2() {
    code cl = string sub (int z = g() + 10) { return sprintf("z=%d", z); };
    return cl() + " " + cl(5);
}
# nested closures
string sub f3() {
    code outer = string sub () {
        code inner = string sub (int z = h() * 2) { return sprintf("z=%d", z); };
        return inner();
    };
    return outer();
}
# a closure in a method
class C {
    public {
        # a closure in a member initializer
        code mc = string sub (int z = g() + 100) { return sprintf("z=%d", z); };
    }
    string m() {
        code cl = string sub (int z = h() + 1000) { return sprintf("z=%d", z); };
        return cl();
    }
}
# a closure in a global variable's initializer
code gcl = string sub (int z = h() + 7) { return sprintf("z=%d", z); };
# a closure in a function's default argument
string sub f4(code c = string sub (int z = g() + 3) { return sprintf("z=%d", z); }) {
    return c();
}

int sub h() { return 2; }

printf("%s\n", f1());
printf("%s\n", f2());
printf("%s\n", f3());
C c();
printf("%s\n", c.mc());
printf("%s\n", c.m());
printf("%s\n", gcl());
printf("%s\n", f4());
# a closure in top-level code
code tcl = string sub (int z = h() + g()) { return sprintf("z=%d", z); };
printf("%s\n", tcl());
# defaults that are resolved in the Program: a namespace constant, a constructor call and a hashdecl
namespace N {
    const K = 40;
}
class D {
    public {
        int v = 5;
    }
}
hashdecl HD {
    int a = 6;
}
string sub f5() {
    code cl = string sub (int z = N::K + g(), D d = new D(), hash<HD> hd = <HD>{}) {
        return sprintf("z=%d v=%d a=%d", z, d.v, hd.a);
    };
    return cl();
}
printf("%s\n", f5());
