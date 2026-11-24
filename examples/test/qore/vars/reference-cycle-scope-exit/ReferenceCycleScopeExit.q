#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

%modern

# Regression fixture for issue #7329: a cycle of references between local variables is freed when the function that
# made it returns, and not while a closure that captured one of the variables can still use it.  The program prints what
# each case did; under valgrind, nothing may be lost at exit (reference-cycle-scope-exit.qtest).

#! a cycle of two variables: a holds a reference to r, which refers to a
sub cycle2() {
    auto a = 1;
    reference r = \a;
    a = \r;
}

#! a longer chain that closes on itself
sub chain3() {
    auto a = 1;
    reference b = \a;
    reference c = \b;
    a = \c;
}

#! two cycles in one frame, and one that a loop makes again and again
sub several() {
    auto a = 1;
    reference r = \a;
    a = \r;
    auto x = 2;
    reference y = \x;
    x = \y;
    for (int i = 0; i < 50; ++i) {
        cycle2();
    }
}

#! a closure that captured a variable of the cycle and a plain variable outlives the function: the variables stay valid
#! (using the cycle is the error that it has always been), and they are freed with the closure
code sub kept() {
    auto a = 1;
    reference r = \a;
    a = \r;
    auto v = 7;
    return string sub () {
        try {
            return sprintf("%y", a);
        } catch (hash<ExceptionInfo> ex) {
            return sprintf("%s v=%d", ex.err, v++);
        }
    };
}

#! two closures share the cycle: it is freed only when the last of them is gone
list<code> sub shared() {
    auto a = 1;
    reference b = \a;
    reference c = \b;
    a = \c;
    auto n = 0;
    code first = int sub () { return ++n; };
    code second = string sub () {
        try {
            return sprintf("%y", a);
        } catch (hash<ExceptionInfo> ex) {
            return sprintf("%s n=%d", ex.err, n);
        }
    };
    return (first, second);
}

#! a reference to a plain variable is passed to a function while a cycle exists in the frame
sub take(reference<auto> ref) {
    ref = 7;
}

int sub returned() {
    auto a = 1;
    reference r = \a;
    a = \r;
    auto v = 5;
    take(\v);
    return v;
}

cycle2();
chain3();
several();
printf("returned: %d\n", returned());
code c = kept();
printf("kept: %y %y\n", c(), c());
delete c;
list<code> l = shared();
printf("shared: %y %y %y\n", l[0](), l[1](), l[0]());
splice l, 1, 1;
printf("one left: %y\n", l[0]());
delete l;
printf("done\n");
