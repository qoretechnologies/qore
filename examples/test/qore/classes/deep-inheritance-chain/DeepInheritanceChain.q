#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of deep-inheritance-chain.qtest (issue #6106): deep class hierarchies whose classes call their base class
# constructors, override methods, declare members, implement abstract methods and inherit several classes; the
# static hierarchies here are compiled with the program, and a deeper generated one is parsed in a Program

%modern
%enable-all-warnings

our list<string> events = ();

# a chain of 24 classes: each declares a member, calls its base class constructor, has a destructor, overrides
# depth() and, on even levels, who()
class L0 {
    public { int m0 = 0; }
    constructor(int x) { events += sprintf("L0(%d)", x); }
    destructor() { events += "~L0"; }
    int depth() { return 0; }
    string who() { return "L0"; }
}
class L1 inherits L0 {
    public { int m1 = 10; }
    constructor(int x) : L0(x + 1) { events += sprintf("L1(%d)", x); }
    destructor() { events += "~L1"; }
    int depth() { return 1; }
    string up() { return L0::who(); }
}
class L2 inherits L1 {
    public { int m2 = 20; }
    constructor(int x) : L1(x + 1) { events += sprintf("L2(%d)", x); }
    destructor() { events += "~L2"; }
    int depth() { return 2; }
    string who() { return "L2"; }
    string up() { return L1::who(); }
}
class L3 inherits L2 {
    public { int m3 = 30; }
    constructor(int x) : L2(x + 1) { events += sprintf("L3(%d)", x); }
    destructor() { events += "~L3"; }
    int depth() { return 3; }
    string up() { return L2::who(); }
}
class L4 inherits L3 {
    public { int m4 = 40; }
    constructor(int x) : L3(x + 1) { events += sprintf("L4(%d)", x); }
    destructor() { events += "~L4"; }
    int depth() { return 4; }
    string who() { return "L4"; }
    string up() { return L3::who(); }
}
class L5 inherits L4 {
    public { int m5 = 50; }
    constructor(int x) : L4(x + 1) { events += sprintf("L5(%d)", x); }
    destructor() { events += "~L5"; }
    int depth() { return 5; }
    string up() { return L4::who(); }
}
class L6 inherits L5 {
    public { int m6 = 60; }
    constructor(int x) : L5(x + 1) { events += sprintf("L6(%d)", x); }
    destructor() { events += "~L6"; }
    int depth() { return 6; }
    string who() { return "L6"; }
    string up() { return L5::who(); }
}
class L7 inherits L6 {
    public { int m7 = 70; }
    constructor(int x) : L6(x + 1) { events += sprintf("L7(%d)", x); }
    destructor() { events += "~L7"; }
    int depth() { return 7; }
    string up() { return L6::who(); }
}
class L8 inherits L7 {
    public { int m8 = 80; }
    constructor(int x) : L7(x + 1) { events += sprintf("L8(%d)", x); }
    destructor() { events += "~L8"; }
    int depth() { return 8; }
    string who() { return "L8"; }
    string up() { return L7::who(); }
}
class L9 inherits L8 {
    public { int m9 = 90; }
    constructor(int x) : L8(x + 1) { events += sprintf("L9(%d)", x); }
    destructor() { events += "~L9"; }
    int depth() { return 9; }
    string up() { return L8::who(); }
}
class L10 inherits L9 {
    public { int m10 = 100; }
    constructor(int x) : L9(x + 1) { events += sprintf("L10(%d)", x); }
    destructor() { events += "~L10"; }
    int depth() { return 10; }
    string who() { return "L10"; }
    string up() { return L9::who(); }
}
class L11 inherits L10 {
    public { int m11 = 110; }
    constructor(int x) : L10(x + 1) { events += sprintf("L11(%d)", x); }
    destructor() { events += "~L11"; }
    int depth() { return 11; }
    string up() { return L10::who(); }
}
class L12 inherits L11 {
    public { int m12 = 120; }
    constructor(int x) : L11(x + 1) { events += sprintf("L12(%d)", x); }
    destructor() { events += "~L12"; }
    int depth() { return 12; }
    string who() { return "L12"; }
    string up() { return L11::who(); }
}
class L13 inherits L12 {
    public { int m13 = 130; }
    constructor(int x) : L12(x + 1) { events += sprintf("L13(%d)", x); }
    destructor() { events += "~L13"; }
    int depth() { return 13; }
    string up() { return L12::who(); }
}
class L14 inherits L13 {
    public { int m14 = 140; }
    constructor(int x) : L13(x + 1) { events += sprintf("L14(%d)", x); }
    destructor() { events += "~L14"; }
    int depth() { return 14; }
    string who() { return "L14"; }
    string up() { return L13::who(); }
}
class L15 inherits L14 {
    public { int m15 = 150; }
    constructor(int x) : L14(x + 1) { events += sprintf("L15(%d)", x); }
    destructor() { events += "~L15"; }
    int depth() { return 15; }
    string up() { return L14::who(); }
}
class L16 inherits L15 {
    public { int m16 = 160; }
    constructor(int x) : L15(x + 1) { events += sprintf("L16(%d)", x); }
    destructor() { events += "~L16"; }
    int depth() { return 16; }
    string who() { return "L16"; }
    string up() { return L15::who(); }
}
class L17 inherits L16 {
    public { int m17 = 170; }
    constructor(int x) : L16(x + 1) { events += sprintf("L17(%d)", x); }
    destructor() { events += "~L17"; }
    int depth() { return 17; }
    string up() { return L16::who(); }
}
class L18 inherits L17 {
    public { int m18 = 180; }
    constructor(int x) : L17(x + 1) { events += sprintf("L18(%d)", x); }
    destructor() { events += "~L18"; }
    int depth() { return 18; }
    string who() { return "L18"; }
    string up() { return L17::who(); }
}
class L19 inherits L18 {
    public { int m19 = 190; }
    constructor(int x) : L18(x + 1) { events += sprintf("L19(%d)", x); }
    destructor() { events += "~L19"; }
    int depth() { return 19; }
    string up() { return L18::who(); }
}
class L20 inherits L19 {
    public { int m20 = 200; }
    constructor(int x) : L19(x + 1) { events += sprintf("L20(%d)", x); }
    destructor() { events += "~L20"; }
    int depth() { return 20; }
    string who() { return "L20"; }
    string up() { return L19::who(); }
}
class L21 inherits L20 {
    public { int m21 = 210; }
    constructor(int x) : L20(x + 1) { events += sprintf("L21(%d)", x); }
    destructor() { events += "~L21"; }
    int depth() { return 21; }
    string up() { return L20::who(); }
}
class L22 inherits L21 {
    public { int m22 = 220; }
    constructor(int x) : L21(x + 1) { events += sprintf("L22(%d)", x); }
    destructor() { events += "~L22"; }
    int depth() { return 22; }
    string who() { return "L22"; }
    string up() { return L21::who(); }
}
class L23 inherits L22 {
    public { int m23 = 230; }
    constructor(int x) : L22(x + 1) { events += sprintf("L23(%d)", x); }
    destructor() { events += "~L23"; }
    int depth() { return 23; }
    string up() { return L22::who(); }
}

# a chain of classes whose abstract method is implemented by the next class
class A0 {
    abstract int f0();
}
class A1 inherits A0 {
    int f0() { return 1; }
    abstract int f1();
}
class A2 inherits A1 {
    int f1() { return 2; }
    abstract int f2();
}
class A3 inherits A2 {
    int f2() { return 3; }
    abstract int f3();
}
class A4 inherits A3 {
    int f3() { return 4; }
    abstract int f4();
}
class A5 inherits A4 {
    int f4() { return 5; }
    abstract int f5();
}
class A6 inherits A5 {
    int f5() { return 6; }
    abstract int f6();
}
class A7 inherits A6 {
    int f6() { return 7; }
    abstract int f7();
}
class A8 inherits A7 {
    int f7() { return 8; }
    abstract int f8();
}
class A9 inherits A8 {
    int f8() { return 9; }
    abstract int f9();
}
class A10 inherits A9 {
    int f9() { return 10; }
    abstract int f10();
}
class A11 inherits A10 {
    int f10() { return 11; }
    abstract int f11();
}
class A12 inherits A11 {
    int f11() { return 12; }
    abstract int f12();
}
class A13 inherits A12 {
    int f12() { return 13; }
    abstract int f13();
}
class A14 inherits A13 {
    int f13() { return 14; }
    abstract int f14();
}
class A15 inherits A14 {
    int f14() { return 15; }
    abstract int f15();
}
class A16 inherits A15 {
    int f15() { return 16; }
    abstract int f16();
}
class A17 inherits A16 {
    int f16() { return 17; }
    abstract int f17();
}
class A18 inherits A17 {
    int f17() { return 18; }
    abstract int f18();
}
class A19 inherits A18 {
    int f18() { return 19; }
    abstract int f19();
}
class A20 inherits A19 {
    int f19() { return 20; }
    abstract int f20();
}
class A21 inherits A20 {
    int f20() { return 21; }
    abstract int f21();
}
class A22 inherits A21 {
    int f21() { return 22; }
    abstract int f22();
}
class A23 inherits A22 {
    int f22() { return 23; }
}

# a chain of classes that also inherit a class of their own, all of which inherit XBase: XBase is constructed once
class XBase {
    constructor() { events += "XBase"; }
    destructor() { events += "~XBase"; }
}
class P0 {
    constructor() { events += "P0"; }
}
class X1 inherits XBase {
    constructor() : XBase() { events += "X1"; }
    int x1() { return 1; }
}
class P1 inherits P0, X1 {
    constructor() : P0(), X1() { events += "P1"; }
}
class X2 inherits XBase {
    constructor() : XBase() { events += "X2"; }
    int x2() { return 2; }
}
class P2 inherits P1, X2 {
    constructor() : P1(), X2() { events += "P2"; }
}
class X3 inherits XBase {
    constructor() : XBase() { events += "X3"; }
    int x3() { return 3; }
}
class P3 inherits P2, X3 {
    constructor() : P2(), X3() { events += "P3"; }
}
class X4 inherits XBase {
    constructor() : XBase() { events += "X4"; }
    int x4() { return 4; }
}
class P4 inherits P3, X4 {
    constructor() : P3(), X4() { events += "P4"; }
}
class X5 inherits XBase {
    constructor() : XBase() { events += "X5"; }
    int x5() { return 5; }
}
class P5 inherits P4, X5 {
    constructor() : P4(), X5() { events += "P5"; }
}
class X6 inherits XBase {
    constructor() : XBase() { events += "X6"; }
    int x6() { return 6; }
}
class P6 inherits P5, X6 {
    constructor() : P5(), X6() { events += "P6"; }
}
class X7 inherits XBase {
    constructor() : XBase() { events += "X7"; }
    int x7() { return 7; }
}
class P7 inherits P6, X7 {
    constructor() : P6(), X7() { events += "P7"; }
}
class X8 inherits XBase {
    constructor() : XBase() { events += "X8"; }
    int x8() { return 8; }
}
class P8 inherits P7, X8 {
    constructor() : P7(), X8() { events += "P8"; }
}
class X9 inherits XBase {
    constructor() : XBase() { events += "X9"; }
    int x9() { return 9; }
}
class P9 inherits P8, X9 {
    constructor() : P8(), X9() { events += "P9"; }
}
class X10 inherits XBase {
    constructor() : XBase() { events += "X10"; }
    int x10() { return 10; }
}
class P10 inherits P9, X10 {
    constructor() : P9(), X10() { events += "P10"; }
}
class X11 inherits XBase {
    constructor() : XBase() { events += "X11"; }
    int x11() { return 11; }
}
class P11 inherits P10, X11 {
    constructor() : P10(), X11() { events += "P11"; }
}
class X12 inherits XBase {
    constructor() : XBase() { events += "X12"; }
    int x12() { return 12; }
}
class P12 inherits P11, X12 {
    constructor() : P11(), X12() { events += "P12"; }
}
class X13 inherits XBase {
    constructor() : XBase() { events += "X13"; }
    int x13() { return 13; }
}
class P13 inherits P12, X13 {
    constructor() : P12(), X13() { events += "P13"; }
}
class X14 inherits XBase {
    constructor() : XBase() { events += "X14"; }
    int x14() { return 14; }
}
class P14 inherits P13, X14 {
    constructor() : P13(), X14() { events += "P14"; }
}
class X15 inherits XBase {
    constructor() : XBase() { events += "X15"; }
    int x15() { return 15; }
}
class P15 inherits P14, X15 {
    constructor() : P14(), X15() { events += "P15"; }
}
class X16 inherits XBase {
    constructor() : XBase() { events += "X16"; }
    int x16() { return 16; }
}
class P16 inherits P15, X16 {
    constructor() : P15(), X16() { events += "P16"; }
}
class X17 inherits XBase {
    constructor() : XBase() { events += "X17"; }
    int x17() { return 17; }
}
class P17 inherits P16, X17 {
    constructor() : P16(), X17() { events += "P17"; }
}
class X18 inherits XBase {
    constructor() : XBase() { events += "X18"; }
    int x18() { return 18; }
}
class P18 inherits P17, X18 {
    constructor() : P17(), X18() { events += "P18"; }
}
class X19 inherits XBase {
    constructor() : XBase() { events += "X19"; }
    int x19() { return 19; }
}
class P19 inherits P18, X19 {
    constructor() : P18(), X19() { events += "P19"; }
}
class X20 inherits XBase {
    constructor() : XBase() { events += "X20"; }
    int x20() { return 20; }
}
class P20 inherits P19, X20 {
    constructor() : P19(), X20() { events += "P20"; }
}
class X21 inherits XBase {
    constructor() : XBase() { events += "X21"; }
    int x21() { return 21; }
}
class P21 inherits P20, X21 {
    constructor() : P20(), X21() { events += "P21"; }
}
class X22 inherits XBase {
    constructor() : XBase() { events += "X22"; }
    int x22() { return 22; }
}
class P22 inherits P21, X22 {
    constructor() : P21(), X22() { events += "P22"; }
}
class X23 inherits XBase {
    constructor() : XBase() { events += "X23"; }
    int x23() { return 23; }
}
class P23 inherits P22, X23 {
    constructor() : P22(), X23() { events += "P23"; }
}

sub show(string label, auto value) {
    printf("%s: %y\n", label, value);
}

# the static hierarchies
{
    L23 l = new L23(0);
    show("L construction", events);
    events = ();
    show("L members", (map l{"m" + $1}, xrange(24)));
    show("L depth", l.depth());
    show("L who", l.who());
    show("L up", l.up());
    L23 c = l.copy();
    show("L copy who", c.who());
    delete c;
    events = ();
    delete l;
    show("L destruction", events);
    events = ();

    A23 a = new A23();
    show("A results", (map call_object_method(a, "f" + $1), xrange(23)));

    P23 p = new P23();
    show("P construction", events);
    events = ();
    show("P mixins", (map call_object_method(p, "x" + $1), xrange(1, 23)));
    delete p;
    show("P destruction", events);
    events = ();
}

# returns the source of a hierarchy of n classes like the static ones above: each class D<i> inherits D<i-1> and
# M<i>, all M<i> inherit MBase, and D<i> implements the abstract method of D<i-1>
string sub deep_source(int n) {
    string src = "our list<string> events = ();\n"
        "class MBase { constructor() { events += \"MBase\"; } }\n"
        "class D0 { public { int m0 = 0; } constructor(int x) { events += \"D0:\" + x; } int depth() { return 0; } "
        "abstract int f0(); }\n";
    for (int i = 1; i < n; ++i) {
        src += sprintf("class M%d inherits MBase { constructor() : MBase() {} int mix%d() { return %d; } }\n",
            i, i, i);
        src += sprintf("class D%d inherits D%d, M%d { public { int m%d = %d; } constructor(int x) : D%d(x + 1), "
            "M%d() { events += \"D%d:\" + x; } int depth() { return %d; } int f%d() { return %d; } %s}\n",
            i, i - 1, i, i, i * 10, i - 1, i, i, i, i - 1, i, i < (n - 1) ? sprintf("abstract int f%d(); ", i) : "");
    }
    src += sprintf("hash<auto> sub run() { D%d d = new D%d(0); return {"
        "\"events\": events, "
        "\"members\": (map d{\"m\" + $1}, xrange(%d)), "
        "\"depth\": d.depth(), "
        "\"f\": (map call_object_method(d, \"f\" + $1), xrange(%d)), "
        "\"mix\": (map call_object_method(d, \"mix\" + $1), xrange(1, %d)), "
        "}; }\n", n - 1, n - 1, n, n - 1, n - 1);
    return src;
}

# the generated hierarchy
{
    const int N = 300;
    Program pgm(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    pgm.parse(deep_source(N), "deep");
    hash<auto> h = pgm.callFunction("run");
    list<string> expected_events = ("D0:" + (N - 1), "MBase");
    for (int i = 1; i < N; ++i) {
        expected_events += sprintf("D%d:%d", i, N - 1 - i);
    }
    show("deep events", h.events == expected_events);
    show("deep members", h.members == (map $1 * 10, xrange(N)));
    show("deep depth", h.depth);
    show("deep f", h.f == (map $1 + 1, xrange(N - 1)));
    show("deep mix", h.mix == (map $1, xrange(1, N - 1)));
}
