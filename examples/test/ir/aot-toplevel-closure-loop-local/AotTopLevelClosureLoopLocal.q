# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Closures of top-level code that capture a local of a loop body (#5869); see aot-toplevel-closure-loop-local.qtest

%modern

# the issue's cases: a closure created in each iteration captures the iteration's own variable
list<code> l = ();
for (int i = 0; i < 2; ++i) {
    int z;
    l += *int sub () { return z; };
    z = i + 10;
}
printf("vals=%y\n", (map $1(), l));
list<code> l2 = ();
for (int i = 0; i < 2; ++i) {
    int z;
    on_exit { l2 += *int sub () { return z; }; }
    z = i + 10;
}
printf("vals2=%y\n", (map $1(), l2));

# a second loop of closures of a local of the same name
list<code> l3 = ();
for (int i = 0; i < 2; ++i) {
    int z;
    l3 += *int sub () { return z; };
    z = i + 20;
}
printf("vals3=%y vals=%y\n", (map $1(), l3), (map $1(), l));

# foreach, while and nested loops
list<code> l4 = ();
foreach int v in ((1, 2, 3)) {
    int w = v * 2;
    l4 += int sub () { return w + v; };
}
int n = 0;
while (n < 2) {
    string s = sprintf("w%d", n);
    l4 += string sub () { return s; };
    ++n;
}
for (int i = 0; i < 2; ++i) {
    int outer = i;
    for (int j = 0; j < 2; ++j) {
        int inner = j;
        l4 += string sub () { return sprintf("%d.%d", outer, inner); };
    }
}
printf("vals4=%y\n", (map $1(), l4));

# a closure that changes the variable of its iteration
list<code> inc = ();
for (int i = 0; i < 2; ++i) {
    int c = i * 100;
    inc += int sub () { return ++c; };
}
printf("inc=%y %y\n", (map $1(), inc), (map $1(), inc));

# a closure that creates a closure, and a getter and a setter that share the variable of their iteration
list<code> nested = ();
list<hash<auto>> gs = ();
for (int i = 0; i < 2; ++i) {
    int base = 100 + i * 10;
    code make = code sub () { return int sub () { return ++base; }; };
    nested += make();
    int shared = 0;
    gs += {"get": int sub () { return shared; }, "set": sub (int v) { shared = v; }};
}
gs[0].set(42);
printf("nested=%y gs=%y\n", (map $1(), nested), (map $1.get(), gs));

# a captured loop local that shadows a local of the top-level block
int shadowed = -1;
list<code> sh = ();
for (int i = 0; i < 2; ++i) {
    int shadowed = i;
    sh += int sub () { return shadowed; };
}
printf("shadowed=%d sh=%y\n", shadowed, (map $1(), sh));

# closures of loop locals that a background thread calls
list<code> bgl = ();
for (int i = 0; i < 2; ++i) {
    int bv = i * 7;
    bgl += int sub () { return bv; };
}
Counter bgc(1);
list<auto> bgr;
background sub () {
    on_exit bgc.dec();
    bgr = map $1(), bgl;
}();
bgc.waitForZero();
printf("bg=%y\n", bgr);

# a local of a loop body that no closure captures is a new variable in each iteration
list<auto> plain = ();
for (int i = 0; i < 3; ++i) {
    *int p;
    plain += p;
    p = i;
}
printf("plain=%y\n", plain);

# a local of the top-level block that a closure captures is the Program's
int top = 1;
code tc = int sub () { return top; };
top = 2;
printf("top=%d\n", tc());

# the locals of the top-level block in a background thread
Counter cnt(1);
background sub () {
    on_exit cnt.dec();
    printf("thread: top=%d tc=%d vals=%y\n", top, tc(), (map $1(), l));
}();
cnt.waitForZero();
