#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of compound-assignment-nothing-union.qtest (generated for the types, operators and operands below): each
# prints the value that += or -= gives an lvalue with no value, or the exception that it raises; the output of every
# execution mode must be that of the AST interpreter

%modern

# operands whose types the parser does not know
const Runtime = (2, 2.5, 2.5n, "3");

sub show(string label, code<auto()> c) {
    try {
        auto v = c();
        printf("%s => %y %s\n", label, v, v.type());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

our *union<int, float> g1_p;
our *union<int, float> g1_m;
class C1 {
    public { *union<int, float> p; *union<int, float> m; }
}
our *union<float, int> g2_p;
our *union<float, int> g2_m;
class C2 {
    public { *union<float, int> p; *union<float, int> m; }
}
our *union<int, number> g3_p;
our *union<int, number> g3_m;
class C3 {
    public { *union<int, number> p; *union<int, number> m; }
}
our *union<number, float> g4_p;
our *union<number, float> g4_m;
class C4 {
    public { *union<number, float> p; *union<number, float> m; }
}
our *union<string, int> g5_p;
our *union<string, int> g5_m;
class C5 {
    public { *union<string, int> p; *union<string, int> m; }
}
our *union<int, bool> g6_p;
our *union<int, bool> g6_m;
class C6 {
    public { *union<int, bool> p; *union<int, bool> m; }
}
our *union<list<int>, int> g7_p;
our *union<list<int>, int> g7_m;
class C7 {
    public { *union<list<int>, int> p; *union<list<int>, int> m; }
}
our *union<int, list<int>> g8_p;
our *union<int, list<int>> g8_m;
class C8 {
    public { *union<int, list<int>> p; *union<int, list<int>> m; }
}
our *union<hash<auto>, int> g9_p;
our *union<hash<auto>, int> g9_m;
class C9 {
    public { *union<hash<auto>, int> p; *union<hash<auto>, int> m; }
}
our *union<int, hash<auto>> g10_p;
our *union<int, hash<auto>> g10_m;
class C10 {
    public { *union<int, hash<auto>> p; *union<int, hash<auto>> m; }
}
our union<int, float> g11_p;
our union<int, float> g11_m;
class C11 {
    public { union<int, float> p; union<int, float> m; }
}
our union<list<int>, int> g12_p;
our union<list<int>, int> g12_m;
class C12 {
    public { union<list<int>, int> p; union<list<int>, int> m; }
}
our union<int, list<int>> g13_p;
our union<int, list<int>> g13_m;
class C13 {
    public { union<int, list<int>> p; union<int, list<int>> m; }
}
our union<hash<auto>, int> g14_p;
our union<hash<auto>, int> g14_m;
class C14 {
    public { union<hash<auto>, int> p; union<hash<auto>, int> m; }
}
our union<int, hash<auto>> g15_p;
our union<int, hash<auto>> g15_m;
class C15 {
    public { union<int, hash<auto>> p; union<int, hash<auto>> m; }
}
our *int g16_p;
our *int g16_m;
class C16 {
    public { *int p; *int m; }
}
our *float g17_p;
our *float g17_m;
class C17 {
    public { *float p; *float m; }
}

sub main() {
    show("local *union<int, float> += 2", auto sub () { *union<int, float> u; u += 2; return u; });
    show("global *union<int, float> += 2", auto sub () { remove g1_p; g1_p += 2; return g1_p; });
    show("member *union<int, float> += 2", auto sub () { C1 o(); o.p += 2; return o.p; });
    show("local *union<int, float> += 2.5", auto sub () { *union<int, float> u; u += 2.5; return u; });
    show("global *union<int, float> += 2.5", auto sub () { remove g1_p; g1_p += 2.5; return g1_p; });
    show("member *union<int, float> += 2.5", auto sub () { C1 o(); o.p += 2.5; return o.p; });
    show("local *union<int, float> += 2.5n", auto sub () { *union<int, float> u; u += 2.5n; return u; });
    show("global *union<int, float> += 2.5n", auto sub () { remove g1_p; g1_p += 2.5n; return g1_p; });
    show("member *union<int, float> += 2.5n", auto sub () { C1 o(); o.p += 2.5n; return o.p; });
    show("local *union<int, float> += '3'", auto sub () { *union<int, float> u; u += "3"; return u; });
    show("global *union<int, float> += '3'", auto sub () { remove g1_p; g1_p += "3"; return g1_p; });
    show("member *union<int, float> += '3'", auto sub () { C1 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, float> += %y %s", v, v.type()),
            auto sub () { *union<int, float> u; u += v; return u; });
    }
    show("local *union<int, float> -= 2", auto sub () { *union<int, float> u; u -= 2; return u; });
    show("global *union<int, float> -= 2", auto sub () { remove g1_m; g1_m -= 2; return g1_m; });
    show("member *union<int, float> -= 2", auto sub () { C1 o(); o.m -= 2; return o.m; });
    show("local *union<int, float> -= 2.5", auto sub () { *union<int, float> u; u -= 2.5; return u; });
    show("global *union<int, float> -= 2.5", auto sub () { remove g1_m; g1_m -= 2.5; return g1_m; });
    show("member *union<int, float> -= 2.5", auto sub () { C1 o(); o.m -= 2.5; return o.m; });
    show("local *union<int, float> -= 2.5n", auto sub () { *union<int, float> u; u -= 2.5n; return u; });
    show("global *union<int, float> -= 2.5n", auto sub () { remove g1_m; g1_m -= 2.5n; return g1_m; });
    show("member *union<int, float> -= 2.5n", auto sub () { C1 o(); o.m -= 2.5n; return o.m; });
    show("local *union<int, float> -= '3'", auto sub () { *union<int, float> u; u -= "3"; return u; });
    show("global *union<int, float> -= '3'", auto sub () { remove g1_m; g1_m -= "3"; return g1_m; });
    show("member *union<int, float> -= '3'", auto sub () { C1 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, float> -= %y %s", v, v.type()),
            auto sub () { *union<int, float> u; u -= v; return u; });
    }
    show("local *union<float, int> += 2", auto sub () { *union<float, int> u; u += 2; return u; });
    show("global *union<float, int> += 2", auto sub () { remove g2_p; g2_p += 2; return g2_p; });
    show("member *union<float, int> += 2", auto sub () { C2 o(); o.p += 2; return o.p; });
    show("local *union<float, int> += 2.5", auto sub () { *union<float, int> u; u += 2.5; return u; });
    show("global *union<float, int> += 2.5", auto sub () { remove g2_p; g2_p += 2.5; return g2_p; });
    show("member *union<float, int> += 2.5", auto sub () { C2 o(); o.p += 2.5; return o.p; });
    show("local *union<float, int> += 2.5n", auto sub () { *union<float, int> u; u += 2.5n; return u; });
    show("global *union<float, int> += 2.5n", auto sub () { remove g2_p; g2_p += 2.5n; return g2_p; });
    show("member *union<float, int> += 2.5n", auto sub () { C2 o(); o.p += 2.5n; return o.p; });
    show("local *union<float, int> += '3'", auto sub () { *union<float, int> u; u += "3"; return u; });
    show("global *union<float, int> += '3'", auto sub () { remove g2_p; g2_p += "3"; return g2_p; });
    show("member *union<float, int> += '3'", auto sub () { C2 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<float, int> += %y %s", v, v.type()),
            auto sub () { *union<float, int> u; u += v; return u; });
    }
    show("local *union<float, int> -= 2", auto sub () { *union<float, int> u; u -= 2; return u; });
    show("global *union<float, int> -= 2", auto sub () { remove g2_m; g2_m -= 2; return g2_m; });
    show("member *union<float, int> -= 2", auto sub () { C2 o(); o.m -= 2; return o.m; });
    show("local *union<float, int> -= 2.5", auto sub () { *union<float, int> u; u -= 2.5; return u; });
    show("global *union<float, int> -= 2.5", auto sub () { remove g2_m; g2_m -= 2.5; return g2_m; });
    show("member *union<float, int> -= 2.5", auto sub () { C2 o(); o.m -= 2.5; return o.m; });
    show("local *union<float, int> -= 2.5n", auto sub () { *union<float, int> u; u -= 2.5n; return u; });
    show("global *union<float, int> -= 2.5n", auto sub () { remove g2_m; g2_m -= 2.5n; return g2_m; });
    show("member *union<float, int> -= 2.5n", auto sub () { C2 o(); o.m -= 2.5n; return o.m; });
    show("local *union<float, int> -= '3'", auto sub () { *union<float, int> u; u -= "3"; return u; });
    show("global *union<float, int> -= '3'", auto sub () { remove g2_m; g2_m -= "3"; return g2_m; });
    show("member *union<float, int> -= '3'", auto sub () { C2 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<float, int> -= %y %s", v, v.type()),
            auto sub () { *union<float, int> u; u -= v; return u; });
    }
    show("local *union<int, number> += 2", auto sub () { *union<int, number> u; u += 2; return u; });
    show("global *union<int, number> += 2", auto sub () { remove g3_p; g3_p += 2; return g3_p; });
    show("member *union<int, number> += 2", auto sub () { C3 o(); o.p += 2; return o.p; });
    show("local *union<int, number> += 2.5", auto sub () { *union<int, number> u; u += 2.5; return u; });
    show("global *union<int, number> += 2.5", auto sub () { remove g3_p; g3_p += 2.5; return g3_p; });
    show("member *union<int, number> += 2.5", auto sub () { C3 o(); o.p += 2.5; return o.p; });
    show("local *union<int, number> += 2.5n", auto sub () { *union<int, number> u; u += 2.5n; return u; });
    show("global *union<int, number> += 2.5n", auto sub () { remove g3_p; g3_p += 2.5n; return g3_p; });
    show("member *union<int, number> += 2.5n", auto sub () { C3 o(); o.p += 2.5n; return o.p; });
    show("local *union<int, number> += '3'", auto sub () { *union<int, number> u; u += "3"; return u; });
    show("global *union<int, number> += '3'", auto sub () { remove g3_p; g3_p += "3"; return g3_p; });
    show("member *union<int, number> += '3'", auto sub () { C3 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, number> += %y %s", v, v.type()),
            auto sub () { *union<int, number> u; u += v; return u; });
    }
    show("local *union<int, number> -= 2", auto sub () { *union<int, number> u; u -= 2; return u; });
    show("global *union<int, number> -= 2", auto sub () { remove g3_m; g3_m -= 2; return g3_m; });
    show("member *union<int, number> -= 2", auto sub () { C3 o(); o.m -= 2; return o.m; });
    show("local *union<int, number> -= 2.5", auto sub () { *union<int, number> u; u -= 2.5; return u; });
    show("global *union<int, number> -= 2.5", auto sub () { remove g3_m; g3_m -= 2.5; return g3_m; });
    show("member *union<int, number> -= 2.5", auto sub () { C3 o(); o.m -= 2.5; return o.m; });
    show("local *union<int, number> -= 2.5n", auto sub () { *union<int, number> u; u -= 2.5n; return u; });
    show("global *union<int, number> -= 2.5n", auto sub () { remove g3_m; g3_m -= 2.5n; return g3_m; });
    show("member *union<int, number> -= 2.5n", auto sub () { C3 o(); o.m -= 2.5n; return o.m; });
    show("local *union<int, number> -= '3'", auto sub () { *union<int, number> u; u -= "3"; return u; });
    show("global *union<int, number> -= '3'", auto sub () { remove g3_m; g3_m -= "3"; return g3_m; });
    show("member *union<int, number> -= '3'", auto sub () { C3 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, number> -= %y %s", v, v.type()),
            auto sub () { *union<int, number> u; u -= v; return u; });
    }
    show("local *union<number, float> += 2", auto sub () { *union<number, float> u; u += 2; return u; });
    show("global *union<number, float> += 2", auto sub () { remove g4_p; g4_p += 2; return g4_p; });
    show("member *union<number, float> += 2", auto sub () { C4 o(); o.p += 2; return o.p; });
    show("local *union<number, float> += 2.5", auto sub () { *union<number, float> u; u += 2.5; return u; });
    show("global *union<number, float> += 2.5", auto sub () { remove g4_p; g4_p += 2.5; return g4_p; });
    show("member *union<number, float> += 2.5", auto sub () { C4 o(); o.p += 2.5; return o.p; });
    show("local *union<number, float> += 2.5n", auto sub () { *union<number, float> u; u += 2.5n; return u; });
    show("global *union<number, float> += 2.5n", auto sub () { remove g4_p; g4_p += 2.5n; return g4_p; });
    show("member *union<number, float> += 2.5n", auto sub () { C4 o(); o.p += 2.5n; return o.p; });
    show("local *union<number, float> += '3'", auto sub () { *union<number, float> u; u += "3"; return u; });
    show("global *union<number, float> += '3'", auto sub () { remove g4_p; g4_p += "3"; return g4_p; });
    show("member *union<number, float> += '3'", auto sub () { C4 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<number, float> += %y %s", v, v.type()), auto sub () {
        *union<number, float> u; u += v; return u; });
    }
    show("local *union<number, float> -= 2", auto sub () { *union<number, float> u; u -= 2; return u; });
    show("global *union<number, float> -= 2", auto sub () { remove g4_m; g4_m -= 2; return g4_m; });
    show("member *union<number, float> -= 2", auto sub () { C4 o(); o.m -= 2; return o.m; });
    show("local *union<number, float> -= 2.5", auto sub () { *union<number, float> u; u -= 2.5; return u; });
    show("global *union<number, float> -= 2.5", auto sub () { remove g4_m; g4_m -= 2.5; return g4_m; });
    show("member *union<number, float> -= 2.5", auto sub () { C4 o(); o.m -= 2.5; return o.m; });
    show("local *union<number, float> -= 2.5n", auto sub () { *union<number, float> u; u -= 2.5n; return u; });
    show("global *union<number, float> -= 2.5n", auto sub () { remove g4_m; g4_m -= 2.5n; return g4_m; });
    show("member *union<number, float> -= 2.5n", auto sub () { C4 o(); o.m -= 2.5n; return o.m; });
    show("local *union<number, float> -= '3'", auto sub () { *union<number, float> u; u -= "3"; return u; });
    show("global *union<number, float> -= '3'", auto sub () { remove g4_m; g4_m -= "3"; return g4_m; });
    show("member *union<number, float> -= '3'", auto sub () { C4 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<number, float> -= %y %s", v, v.type()), auto sub () {
        *union<number, float> u; u -= v; return u; });
    }
    show("local *union<string, int> += 2", auto sub () { *union<string, int> u; u += 2; return u; });
    show("global *union<string, int> += 2", auto sub () { remove g5_p; g5_p += 2; return g5_p; });
    show("member *union<string, int> += 2", auto sub () { C5 o(); o.p += 2; return o.p; });
    show("local *union<string, int> += 2.5", auto sub () { *union<string, int> u; u += 2.5; return u; });
    show("global *union<string, int> += 2.5", auto sub () { remove g5_p; g5_p += 2.5; return g5_p; });
    show("member *union<string, int> += 2.5", auto sub () { C5 o(); o.p += 2.5; return o.p; });
    show("local *union<string, int> += 2.5n", auto sub () { *union<string, int> u; u += 2.5n; return u; });
    show("global *union<string, int> += 2.5n", auto sub () { remove g5_p; g5_p += 2.5n; return g5_p; });
    show("member *union<string, int> += 2.5n", auto sub () { C5 o(); o.p += 2.5n; return o.p; });
    show("local *union<string, int> += '3'", auto sub () { *union<string, int> u; u += "3"; return u; });
    show("global *union<string, int> += '3'", auto sub () { remove g5_p; g5_p += "3"; return g5_p; });
    show("member *union<string, int> += '3'", auto sub () { C5 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<string, int> += %y %s", v, v.type()),
            auto sub () { *union<string, int> u; u += v; return u; });
    }
    show("local *union<string, int> -= 2", auto sub () { *union<string, int> u; u -= 2; return u; });
    show("global *union<string, int> -= 2", auto sub () { remove g5_m; g5_m -= 2; return g5_m; });
    show("member *union<string, int> -= 2", auto sub () { C5 o(); o.m -= 2; return o.m; });
    show("local *union<string, int> -= 2.5", auto sub () { *union<string, int> u; u -= 2.5; return u; });
    show("global *union<string, int> -= 2.5", auto sub () { remove g5_m; g5_m -= 2.5; return g5_m; });
    show("member *union<string, int> -= 2.5", auto sub () { C5 o(); o.m -= 2.5; return o.m; });
    show("local *union<string, int> -= 2.5n", auto sub () { *union<string, int> u; u -= 2.5n; return u; });
    show("global *union<string, int> -= 2.5n", auto sub () { remove g5_m; g5_m -= 2.5n; return g5_m; });
    show("member *union<string, int> -= 2.5n", auto sub () { C5 o(); o.m -= 2.5n; return o.m; });
    show("local *union<string, int> -= '3'", auto sub () { *union<string, int> u; u -= "3"; return u; });
    show("global *union<string, int> -= '3'", auto sub () { remove g5_m; g5_m -= "3"; return g5_m; });
    show("member *union<string, int> -= '3'", auto sub () { C5 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<string, int> -= %y %s", v, v.type()),
            auto sub () { *union<string, int> u; u -= v; return u; });
    }
    show("local *union<int, bool> += 2", auto sub () { *union<int, bool> u; u += 2; return u; });
    show("global *union<int, bool> += 2", auto sub () { remove g6_p; g6_p += 2; return g6_p; });
    show("member *union<int, bool> += 2", auto sub () { C6 o(); o.p += 2; return o.p; });
    show("local *union<int, bool> += 2.5", auto sub () { *union<int, bool> u; u += 2.5; return u; });
    show("global *union<int, bool> += 2.5", auto sub () { remove g6_p; g6_p += 2.5; return g6_p; });
    show("member *union<int, bool> += 2.5", auto sub () { C6 o(); o.p += 2.5; return o.p; });
    show("local *union<int, bool> += 2.5n", auto sub () { *union<int, bool> u; u += 2.5n; return u; });
    show("global *union<int, bool> += 2.5n", auto sub () { remove g6_p; g6_p += 2.5n; return g6_p; });
    show("member *union<int, bool> += 2.5n", auto sub () { C6 o(); o.p += 2.5n; return o.p; });
    show("local *union<int, bool> += '3'", auto sub () { *union<int, bool> u; u += "3"; return u; });
    show("global *union<int, bool> += '3'", auto sub () { remove g6_p; g6_p += "3"; return g6_p; });
    show("member *union<int, bool> += '3'", auto sub () { C6 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, bool> += %y %s", v, v.type()),
            auto sub () { *union<int, bool> u; u += v; return u; });
    }
    show("local *union<int, bool> -= 2", auto sub () { *union<int, bool> u; u -= 2; return u; });
    show("global *union<int, bool> -= 2", auto sub () { remove g6_m; g6_m -= 2; return g6_m; });
    show("member *union<int, bool> -= 2", auto sub () { C6 o(); o.m -= 2; return o.m; });
    show("local *union<int, bool> -= 2.5", auto sub () { *union<int, bool> u; u -= 2.5; return u; });
    show("global *union<int, bool> -= 2.5", auto sub () { remove g6_m; g6_m -= 2.5; return g6_m; });
    show("member *union<int, bool> -= 2.5", auto sub () { C6 o(); o.m -= 2.5; return o.m; });
    show("local *union<int, bool> -= 2.5n", auto sub () { *union<int, bool> u; u -= 2.5n; return u; });
    show("global *union<int, bool> -= 2.5n", auto sub () { remove g6_m; g6_m -= 2.5n; return g6_m; });
    show("member *union<int, bool> -= 2.5n", auto sub () { C6 o(); o.m -= 2.5n; return o.m; });
    show("local *union<int, bool> -= '3'", auto sub () { *union<int, bool> u; u -= "3"; return u; });
    show("global *union<int, bool> -= '3'", auto sub () { remove g6_m; g6_m -= "3"; return g6_m; });
    show("member *union<int, bool> -= '3'", auto sub () { C6 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, bool> -= %y %s", v, v.type()),
            auto sub () { *union<int, bool> u; u -= v; return u; });
    }
    show("local *union<list<int>, int> += 2", auto sub () { *union<list<int>, int> u; u += 2; return u; });
    show("global *union<list<int>, int> += 2", auto sub () { remove g7_p; g7_p += 2; return g7_p; });
    show("member *union<list<int>, int> += 2", auto sub () { C7 o(); o.p += 2; return o.p; });
    show("local *union<list<int>, int> += 2.5", auto sub () { *union<list<int>, int> u; u += 2.5; return u; });
    show("global *union<list<int>, int> += 2.5", auto sub () { remove g7_p; g7_p += 2.5; return g7_p; });
    show("member *union<list<int>, int> += 2.5", auto sub () { C7 o(); o.p += 2.5; return o.p; });
    show("local *union<list<int>, int> += 2.5n", auto sub () { *union<list<int>, int> u; u += 2.5n; return u; });
    show("global *union<list<int>, int> += 2.5n", auto sub () { remove g7_p; g7_p += 2.5n; return g7_p; });
    show("member *union<list<int>, int> += 2.5n", auto sub () { C7 o(); o.p += 2.5n; return o.p; });
    show("local *union<list<int>, int> += '3'", auto sub () { *union<list<int>, int> u; u += "3"; return u; });
    show("global *union<list<int>, int> += '3'", auto sub () { remove g7_p; g7_p += "3"; return g7_p; });
    show("member *union<list<int>, int> += '3'", auto sub () { C7 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<list<int>, int> += %y %s", v, v.type()), auto sub () {
        *union<list<int>, int> u; u += v; return u; });
    }
    show("local *union<list<int>, int> -= 2", auto sub () { *union<list<int>, int> u; u -= 2; return u; });
    show("global *union<list<int>, int> -= 2", auto sub () { remove g7_m; g7_m -= 2; return g7_m; });
    show("member *union<list<int>, int> -= 2", auto sub () { C7 o(); o.m -= 2; return o.m; });
    show("local *union<list<int>, int> -= 2.5", auto sub () { *union<list<int>, int> u; u -= 2.5; return u; });
    show("global *union<list<int>, int> -= 2.5", auto sub () { remove g7_m; g7_m -= 2.5; return g7_m; });
    show("member *union<list<int>, int> -= 2.5", auto sub () { C7 o(); o.m -= 2.5; return o.m; });
    show("local *union<list<int>, int> -= 2.5n", auto sub () { *union<list<int>, int> u; u -= 2.5n; return u; });
    show("global *union<list<int>, int> -= 2.5n", auto sub () { remove g7_m; g7_m -= 2.5n; return g7_m; });
    show("member *union<list<int>, int> -= 2.5n", auto sub () { C7 o(); o.m -= 2.5n; return o.m; });
    show("local *union<list<int>, int> -= '3'", auto sub () { *union<list<int>, int> u; u -= "3"; return u; });
    show("global *union<list<int>, int> -= '3'", auto sub () { remove g7_m; g7_m -= "3"; return g7_m; });
    show("member *union<list<int>, int> -= '3'", auto sub () { C7 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<list<int>, int> -= %y %s", v, v.type()), auto sub () {
        *union<list<int>, int> u; u -= v; return u; });
    }
    show("local *union<int, list<int>> += 2", auto sub () { *union<int, list<int>> u; u += 2; return u; });
    show("global *union<int, list<int>> += 2", auto sub () { remove g8_p; g8_p += 2; return g8_p; });
    show("member *union<int, list<int>> += 2", auto sub () { C8 o(); o.p += 2; return o.p; });
    show("local *union<int, list<int>> += 2.5", auto sub () { *union<int, list<int>> u; u += 2.5; return u; });
    show("global *union<int, list<int>> += 2.5", auto sub () { remove g8_p; g8_p += 2.5; return g8_p; });
    show("member *union<int, list<int>> += 2.5", auto sub () { C8 o(); o.p += 2.5; return o.p; });
    show("local *union<int, list<int>> += 2.5n", auto sub () { *union<int, list<int>> u; u += 2.5n; return u; });
    show("global *union<int, list<int>> += 2.5n", auto sub () { remove g8_p; g8_p += 2.5n; return g8_p; });
    show("member *union<int, list<int>> += 2.5n", auto sub () { C8 o(); o.p += 2.5n; return o.p; });
    show("local *union<int, list<int>> += '3'", auto sub () { *union<int, list<int>> u; u += "3"; return u; });
    show("global *union<int, list<int>> += '3'", auto sub () { remove g8_p; g8_p += "3"; return g8_p; });
    show("member *union<int, list<int>> += '3'", auto sub () { C8 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, list<int>> += %y %s", v, v.type()), auto sub () {
        *union<int, list<int>> u; u += v; return u; });
    }
    show("local *union<int, list<int>> -= 2", auto sub () { *union<int, list<int>> u; u -= 2; return u; });
    show("global *union<int, list<int>> -= 2", auto sub () { remove g8_m; g8_m -= 2; return g8_m; });
    show("member *union<int, list<int>> -= 2", auto sub () { C8 o(); o.m -= 2; return o.m; });
    show("local *union<int, list<int>> -= 2.5", auto sub () { *union<int, list<int>> u; u -= 2.5; return u; });
    show("global *union<int, list<int>> -= 2.5", auto sub () { remove g8_m; g8_m -= 2.5; return g8_m; });
    show("member *union<int, list<int>> -= 2.5", auto sub () { C8 o(); o.m -= 2.5; return o.m; });
    show("local *union<int, list<int>> -= 2.5n", auto sub () { *union<int, list<int>> u; u -= 2.5n; return u; });
    show("global *union<int, list<int>> -= 2.5n", auto sub () { remove g8_m; g8_m -= 2.5n; return g8_m; });
    show("member *union<int, list<int>> -= 2.5n", auto sub () { C8 o(); o.m -= 2.5n; return o.m; });
    show("local *union<int, list<int>> -= '3'", auto sub () { *union<int, list<int>> u; u -= "3"; return u; });
    show("global *union<int, list<int>> -= '3'", auto sub () { remove g8_m; g8_m -= "3"; return g8_m; });
    show("member *union<int, list<int>> -= '3'", auto sub () { C8 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, list<int>> -= %y %s", v, v.type()), auto sub () {
        *union<int, list<int>> u; u -= v; return u; });
    }
    show("local *union<hash<auto>, int> += 2", auto sub () { *union<hash<auto>, int> u; u += 2; return u; });
    show("global *union<hash<auto>, int> += 2", auto sub () { remove g9_p; g9_p += 2; return g9_p; });
    show("member *union<hash<auto>, int> += 2", auto sub () { C9 o(); o.p += 2; return o.p; });
    show("local *union<hash<auto>, int> += 2.5", auto sub () { *union<hash<auto>, int> u; u += 2.5; return u; });
    show("global *union<hash<auto>, int> += 2.5", auto sub () { remove g9_p; g9_p += 2.5; return g9_p; });
    show("member *union<hash<auto>, int> += 2.5", auto sub () { C9 o(); o.p += 2.5; return o.p; });
    show("local *union<hash<auto>, int> += 2.5n", auto sub () { *union<hash<auto>, int> u; u += 2.5n; return u; });
    show("global *union<hash<auto>, int> += 2.5n", auto sub () { remove g9_p; g9_p += 2.5n; return g9_p; });
    show("member *union<hash<auto>, int> += 2.5n", auto sub () { C9 o(); o.p += 2.5n; return o.p; });
    show("local *union<hash<auto>, int> += '3'", auto sub () { *union<hash<auto>, int> u; u += "3"; return u; });
    show("global *union<hash<auto>, int> += '3'", auto sub () { remove g9_p; g9_p += "3"; return g9_p; });
    show("member *union<hash<auto>, int> += '3'", auto sub () { C9 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<hash<auto>, int> += %y %s", v, v.type()), auto sub () {
        *union<hash<auto>, int> u; u += v; return u; });
    }
    show("local *union<hash<auto>, int> -= 2", auto sub () { *union<hash<auto>, int> u; u -= 2; return u; });
    show("global *union<hash<auto>, int> -= 2", auto sub () { remove g9_m; g9_m -= 2; return g9_m; });
    show("member *union<hash<auto>, int> -= 2", auto sub () { C9 o(); o.m -= 2; return o.m; });
    show("local *union<hash<auto>, int> -= 2.5", auto sub () { *union<hash<auto>, int> u; u -= 2.5; return u; });
    show("global *union<hash<auto>, int> -= 2.5", auto sub () { remove g9_m; g9_m -= 2.5; return g9_m; });
    show("member *union<hash<auto>, int> -= 2.5", auto sub () { C9 o(); o.m -= 2.5; return o.m; });
    show("local *union<hash<auto>, int> -= 2.5n", auto sub () { *union<hash<auto>, int> u; u -= 2.5n; return u; });
    show("global *union<hash<auto>, int> -= 2.5n", auto sub () { remove g9_m; g9_m -= 2.5n; return g9_m; });
    show("member *union<hash<auto>, int> -= 2.5n", auto sub () { C9 o(); o.m -= 2.5n; return o.m; });
    show("local *union<hash<auto>, int> -= '3'", auto sub () { *union<hash<auto>, int> u; u -= "3"; return u; });
    show("global *union<hash<auto>, int> -= '3'", auto sub () { remove g9_m; g9_m -= "3"; return g9_m; });
    show("member *union<hash<auto>, int> -= '3'", auto sub () { C9 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<hash<auto>, int> -= %y %s", v, v.type()), auto sub () {
        *union<hash<auto>, int> u; u -= v; return u; });
    }
    show("local *union<int, hash<auto>> += 2", auto sub () { *union<int, hash<auto>> u; u += 2; return u; });
    show("global *union<int, hash<auto>> += 2", auto sub () { remove g10_p; g10_p += 2; return g10_p; });
    show("member *union<int, hash<auto>> += 2", auto sub () { C10 o(); o.p += 2; return o.p; });
    show("local *union<int, hash<auto>> += 2.5", auto sub () { *union<int, hash<auto>> u; u += 2.5; return u; });
    show("global *union<int, hash<auto>> += 2.5", auto sub () { remove g10_p; g10_p += 2.5; return g10_p; });
    show("member *union<int, hash<auto>> += 2.5", auto sub () { C10 o(); o.p += 2.5; return o.p; });
    show("local *union<int, hash<auto>> += 2.5n", auto sub () { *union<int, hash<auto>> u; u += 2.5n; return u; });
    show("global *union<int, hash<auto>> += 2.5n", auto sub () { remove g10_p; g10_p += 2.5n; return g10_p; });
    show("member *union<int, hash<auto>> += 2.5n", auto sub () { C10 o(); o.p += 2.5n; return o.p; });
    show("local *union<int, hash<auto>> += '3'", auto sub () { *union<int, hash<auto>> u; u += "3"; return u; });
    show("global *union<int, hash<auto>> += '3'", auto sub () { remove g10_p; g10_p += "3"; return g10_p; });
    show("member *union<int, hash<auto>> += '3'", auto sub () { C10 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, hash<auto>> += %y %s", v, v.type()), auto sub () {
        *union<int, hash<auto>> u; u += v; return u; });
    }
    show("local *union<int, hash<auto>> -= 2", auto sub () { *union<int, hash<auto>> u; u -= 2; return u; });
    show("global *union<int, hash<auto>> -= 2", auto sub () { remove g10_m; g10_m -= 2; return g10_m; });
    show("member *union<int, hash<auto>> -= 2", auto sub () { C10 o(); o.m -= 2; return o.m; });
    show("local *union<int, hash<auto>> -= 2.5", auto sub () { *union<int, hash<auto>> u; u -= 2.5; return u; });
    show("global *union<int, hash<auto>> -= 2.5", auto sub () { remove g10_m; g10_m -= 2.5; return g10_m; });
    show("member *union<int, hash<auto>> -= 2.5", auto sub () { C10 o(); o.m -= 2.5; return o.m; });
    show("local *union<int, hash<auto>> -= 2.5n", auto sub () { *union<int, hash<auto>> u; u -= 2.5n; return u; });
    show("global *union<int, hash<auto>> -= 2.5n", auto sub () { remove g10_m; g10_m -= 2.5n; return g10_m; });
    show("member *union<int, hash<auto>> -= 2.5n", auto sub () { C10 o(); o.m -= 2.5n; return o.m; });
    show("local *union<int, hash<auto>> -= '3'", auto sub () { *union<int, hash<auto>> u; u -= "3"; return u; });
    show("global *union<int, hash<auto>> -= '3'", auto sub () { remove g10_m; g10_m -= "3"; return g10_m; });
    show("member *union<int, hash<auto>> -= '3'", auto sub () { C10 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *union<int, hash<auto>> -= %y %s", v, v.type()), auto sub () {
        *union<int, hash<auto>> u; u -= v; return u; });
    }
    show("local union<int, float> += 2", auto sub () { union<int, float> u; u += 2; return u; });
    show("global union<int, float> += 2", auto sub () { remove g11_p; g11_p += 2; return g11_p; });
    show("member union<int, float> += 2", auto sub () { C11 o(); o.p += 2; return o.p; });
    show("local union<int, float> += 2.5", auto sub () { union<int, float> u; u += 2.5; return u; });
    show("global union<int, float> += 2.5", auto sub () { remove g11_p; g11_p += 2.5; return g11_p; });
    show("member union<int, float> += 2.5", auto sub () { C11 o(); o.p += 2.5; return o.p; });
    show("local union<int, float> += 2.5n", auto sub () { union<int, float> u; u += 2.5n; return u; });
    show("global union<int, float> += 2.5n", auto sub () { remove g11_p; g11_p += 2.5n; return g11_p; });
    show("member union<int, float> += 2.5n", auto sub () { C11 o(); o.p += 2.5n; return o.p; });
    show("local union<int, float> += '3'", auto sub () { union<int, float> u; u += "3"; return u; });
    show("global union<int, float> += '3'", auto sub () { remove g11_p; g11_p += "3"; return g11_p; });
    show("member union<int, float> += '3'", auto sub () { C11 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<int, float> += %y %s", v, v.type()),
            auto sub () { union<int, float> u; u += v; return u; });
    }
    show("local union<int, float> -= 2", auto sub () { union<int, float> u; u -= 2; return u; });
    show("global union<int, float> -= 2", auto sub () { remove g11_m; g11_m -= 2; return g11_m; });
    show("member union<int, float> -= 2", auto sub () { C11 o(); o.m -= 2; return o.m; });
    show("local union<int, float> -= 2.5", auto sub () { union<int, float> u; u -= 2.5; return u; });
    show("global union<int, float> -= 2.5", auto sub () { remove g11_m; g11_m -= 2.5; return g11_m; });
    show("member union<int, float> -= 2.5", auto sub () { C11 o(); o.m -= 2.5; return o.m; });
    show("local union<int, float> -= 2.5n", auto sub () { union<int, float> u; u -= 2.5n; return u; });
    show("global union<int, float> -= 2.5n", auto sub () { remove g11_m; g11_m -= 2.5n; return g11_m; });
    show("member union<int, float> -= 2.5n", auto sub () { C11 o(); o.m -= 2.5n; return o.m; });
    show("local union<int, float> -= '3'", auto sub () { union<int, float> u; u -= "3"; return u; });
    show("global union<int, float> -= '3'", auto sub () { remove g11_m; g11_m -= "3"; return g11_m; });
    show("member union<int, float> -= '3'", auto sub () { C11 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<int, float> -= %y %s", v, v.type()),
            auto sub () { union<int, float> u; u -= v; return u; });
    }
    show("local union<list<int>, int> += 2", auto sub () { union<list<int>, int> u; u += 2; return u; });
    show("global union<list<int>, int> += 2", auto sub () { remove g12_p; g12_p += 2; return g12_p; });
    show("member union<list<int>, int> += 2", auto sub () { C12 o(); o.p += 2; return o.p; });
    show("local union<list<int>, int> += 2.5", auto sub () { union<list<int>, int> u; u += 2.5; return u; });
    show("global union<list<int>, int> += 2.5", auto sub () { remove g12_p; g12_p += 2.5; return g12_p; });
    show("member union<list<int>, int> += 2.5", auto sub () { C12 o(); o.p += 2.5; return o.p; });
    show("local union<list<int>, int> += 2.5n", auto sub () { union<list<int>, int> u; u += 2.5n; return u; });
    show("global union<list<int>, int> += 2.5n", auto sub () { remove g12_p; g12_p += 2.5n; return g12_p; });
    show("member union<list<int>, int> += 2.5n", auto sub () { C12 o(); o.p += 2.5n; return o.p; });
    show("local union<list<int>, int> += '3'", auto sub () { union<list<int>, int> u; u += "3"; return u; });
    show("global union<list<int>, int> += '3'", auto sub () { remove g12_p; g12_p += "3"; return g12_p; });
    show("member union<list<int>, int> += '3'", auto sub () { C12 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<list<int>, int> += %y %s", v, v.type()), auto sub () {
        union<list<int>, int> u; u += v; return u; });
    }
    show("local union<list<int>, int> -= 2", auto sub () { union<list<int>, int> u; u -= 2; return u; });
    show("global union<list<int>, int> -= 2", auto sub () { remove g12_m; g12_m -= 2; return g12_m; });
    show("member union<list<int>, int> -= 2", auto sub () { C12 o(); o.m -= 2; return o.m; });
    show("local union<list<int>, int> -= 2.5", auto sub () { union<list<int>, int> u; u -= 2.5; return u; });
    show("global union<list<int>, int> -= 2.5", auto sub () { remove g12_m; g12_m -= 2.5; return g12_m; });
    show("member union<list<int>, int> -= 2.5", auto sub () { C12 o(); o.m -= 2.5; return o.m; });
    show("local union<list<int>, int> -= 2.5n", auto sub () { union<list<int>, int> u; u -= 2.5n; return u; });
    show("global union<list<int>, int> -= 2.5n", auto sub () { remove g12_m; g12_m -= 2.5n; return g12_m; });
    show("member union<list<int>, int> -= 2.5n", auto sub () { C12 o(); o.m -= 2.5n; return o.m; });
    show("local union<list<int>, int> -= '3'", auto sub () { union<list<int>, int> u; u -= "3"; return u; });
    show("global union<list<int>, int> -= '3'", auto sub () { remove g12_m; g12_m -= "3"; return g12_m; });
    show("member union<list<int>, int> -= '3'", auto sub () { C12 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<list<int>, int> -= %y %s", v, v.type()), auto sub () {
        union<list<int>, int> u; u -= v; return u; });
    }
    show("local union<int, list<int>> += 2", auto sub () { union<int, list<int>> u; u += 2; return u; });
    show("global union<int, list<int>> += 2", auto sub () { remove g13_p; g13_p += 2; return g13_p; });
    show("member union<int, list<int>> += 2", auto sub () { C13 o(); o.p += 2; return o.p; });
    show("local union<int, list<int>> += 2.5", auto sub () { union<int, list<int>> u; u += 2.5; return u; });
    show("global union<int, list<int>> += 2.5", auto sub () { remove g13_p; g13_p += 2.5; return g13_p; });
    show("member union<int, list<int>> += 2.5", auto sub () { C13 o(); o.p += 2.5; return o.p; });
    show("local union<int, list<int>> += 2.5n", auto sub () { union<int, list<int>> u; u += 2.5n; return u; });
    show("global union<int, list<int>> += 2.5n", auto sub () { remove g13_p; g13_p += 2.5n; return g13_p; });
    show("member union<int, list<int>> += 2.5n", auto sub () { C13 o(); o.p += 2.5n; return o.p; });
    show("local union<int, list<int>> += '3'", auto sub () { union<int, list<int>> u; u += "3"; return u; });
    show("global union<int, list<int>> += '3'", auto sub () { remove g13_p; g13_p += "3"; return g13_p; });
    show("member union<int, list<int>> += '3'", auto sub () { C13 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<int, list<int>> += %y %s", v, v.type()), auto sub () {
        union<int, list<int>> u; u += v; return u; });
    }
    show("local union<int, list<int>> -= 2", auto sub () { union<int, list<int>> u; u -= 2; return u; });
    show("global union<int, list<int>> -= 2", auto sub () { remove g13_m; g13_m -= 2; return g13_m; });
    show("member union<int, list<int>> -= 2", auto sub () { C13 o(); o.m -= 2; return o.m; });
    show("local union<int, list<int>> -= 2.5", auto sub () { union<int, list<int>> u; u -= 2.5; return u; });
    show("global union<int, list<int>> -= 2.5", auto sub () { remove g13_m; g13_m -= 2.5; return g13_m; });
    show("member union<int, list<int>> -= 2.5", auto sub () { C13 o(); o.m -= 2.5; return o.m; });
    show("local union<int, list<int>> -= 2.5n", auto sub () { union<int, list<int>> u; u -= 2.5n; return u; });
    show("global union<int, list<int>> -= 2.5n", auto sub () { remove g13_m; g13_m -= 2.5n; return g13_m; });
    show("member union<int, list<int>> -= 2.5n", auto sub () { C13 o(); o.m -= 2.5n; return o.m; });
    show("local union<int, list<int>> -= '3'", auto sub () { union<int, list<int>> u; u -= "3"; return u; });
    show("global union<int, list<int>> -= '3'", auto sub () { remove g13_m; g13_m -= "3"; return g13_m; });
    show("member union<int, list<int>> -= '3'", auto sub () { C13 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<int, list<int>> -= %y %s", v, v.type()), auto sub () {
        union<int, list<int>> u; u -= v; return u; });
    }
    show("local union<hash<auto>, int> += 2", auto sub () { union<hash<auto>, int> u; u += 2; return u; });
    show("global union<hash<auto>, int> += 2", auto sub () { remove g14_p; g14_p += 2; return g14_p; });
    show("member union<hash<auto>, int> += 2", auto sub () { C14 o(); o.p += 2; return o.p; });
    show("local union<hash<auto>, int> += 2.5", auto sub () { union<hash<auto>, int> u; u += 2.5; return u; });
    show("global union<hash<auto>, int> += 2.5", auto sub () { remove g14_p; g14_p += 2.5; return g14_p; });
    show("member union<hash<auto>, int> += 2.5", auto sub () { C14 o(); o.p += 2.5; return o.p; });
    show("local union<hash<auto>, int> += 2.5n", auto sub () { union<hash<auto>, int> u; u += 2.5n; return u; });
    show("global union<hash<auto>, int> += 2.5n", auto sub () { remove g14_p; g14_p += 2.5n; return g14_p; });
    show("member union<hash<auto>, int> += 2.5n", auto sub () { C14 o(); o.p += 2.5n; return o.p; });
    show("local union<hash<auto>, int> += '3'", auto sub () { union<hash<auto>, int> u; u += "3"; return u; });
    show("global union<hash<auto>, int> += '3'", auto sub () { remove g14_p; g14_p += "3"; return g14_p; });
    show("member union<hash<auto>, int> += '3'", auto sub () { C14 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<hash<auto>, int> += %y %s", v, v.type()), auto sub () {
        union<hash<auto>, int> u; u += v; return u; });
    }
    show("local union<hash<auto>, int> -= 2", auto sub () { union<hash<auto>, int> u; u -= 2; return u; });
    show("global union<hash<auto>, int> -= 2", auto sub () { remove g14_m; g14_m -= 2; return g14_m; });
    show("member union<hash<auto>, int> -= 2", auto sub () { C14 o(); o.m -= 2; return o.m; });
    show("local union<hash<auto>, int> -= 2.5", auto sub () { union<hash<auto>, int> u; u -= 2.5; return u; });
    show("global union<hash<auto>, int> -= 2.5", auto sub () { remove g14_m; g14_m -= 2.5; return g14_m; });
    show("member union<hash<auto>, int> -= 2.5", auto sub () { C14 o(); o.m -= 2.5; return o.m; });
    show("local union<hash<auto>, int> -= 2.5n", auto sub () { union<hash<auto>, int> u; u -= 2.5n; return u; });
    show("global union<hash<auto>, int> -= 2.5n", auto sub () { remove g14_m; g14_m -= 2.5n; return g14_m; });
    show("member union<hash<auto>, int> -= 2.5n", auto sub () { C14 o(); o.m -= 2.5n; return o.m; });
    show("local union<hash<auto>, int> -= '3'", auto sub () { union<hash<auto>, int> u; u -= "3"; return u; });
    show("global union<hash<auto>, int> -= '3'", auto sub () { remove g14_m; g14_m -= "3"; return g14_m; });
    show("member union<hash<auto>, int> -= '3'", auto sub () { C14 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<hash<auto>, int> -= %y %s", v, v.type()), auto sub () {
        union<hash<auto>, int> u; u -= v; return u; });
    }
    show("local union<int, hash<auto>> += 2", auto sub () { union<int, hash<auto>> u; u += 2; return u; });
    show("global union<int, hash<auto>> += 2", auto sub () { remove g15_p; g15_p += 2; return g15_p; });
    show("member union<int, hash<auto>> += 2", auto sub () { C15 o(); o.p += 2; return o.p; });
    show("local union<int, hash<auto>> += 2.5", auto sub () { union<int, hash<auto>> u; u += 2.5; return u; });
    show("global union<int, hash<auto>> += 2.5", auto sub () { remove g15_p; g15_p += 2.5; return g15_p; });
    show("member union<int, hash<auto>> += 2.5", auto sub () { C15 o(); o.p += 2.5; return o.p; });
    show("local union<int, hash<auto>> += 2.5n", auto sub () { union<int, hash<auto>> u; u += 2.5n; return u; });
    show("global union<int, hash<auto>> += 2.5n", auto sub () { remove g15_p; g15_p += 2.5n; return g15_p; });
    show("member union<int, hash<auto>> += 2.5n", auto sub () { C15 o(); o.p += 2.5n; return o.p; });
    show("local union<int, hash<auto>> += '3'", auto sub () { union<int, hash<auto>> u; u += "3"; return u; });
    show("global union<int, hash<auto>> += '3'", auto sub () { remove g15_p; g15_p += "3"; return g15_p; });
    show("member union<int, hash<auto>> += '3'", auto sub () { C15 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<int, hash<auto>> += %y %s", v, v.type()), auto sub () {
        union<int, hash<auto>> u; u += v; return u; });
    }
    show("local union<int, hash<auto>> -= 2", auto sub () { union<int, hash<auto>> u; u -= 2; return u; });
    show("global union<int, hash<auto>> -= 2", auto sub () { remove g15_m; g15_m -= 2; return g15_m; });
    show("member union<int, hash<auto>> -= 2", auto sub () { C15 o(); o.m -= 2; return o.m; });
    show("local union<int, hash<auto>> -= 2.5", auto sub () { union<int, hash<auto>> u; u -= 2.5; return u; });
    show("global union<int, hash<auto>> -= 2.5", auto sub () { remove g15_m; g15_m -= 2.5; return g15_m; });
    show("member union<int, hash<auto>> -= 2.5", auto sub () { C15 o(); o.m -= 2.5; return o.m; });
    show("local union<int, hash<auto>> -= 2.5n", auto sub () { union<int, hash<auto>> u; u -= 2.5n; return u; });
    show("global union<int, hash<auto>> -= 2.5n", auto sub () { remove g15_m; g15_m -= 2.5n; return g15_m; });
    show("member union<int, hash<auto>> -= 2.5n", auto sub () { C15 o(); o.m -= 2.5n; return o.m; });
    show("local union<int, hash<auto>> -= '3'", auto sub () { union<int, hash<auto>> u; u -= "3"; return u; });
    show("global union<int, hash<auto>> -= '3'", auto sub () { remove g15_m; g15_m -= "3"; return g15_m; });
    show("member union<int, hash<auto>> -= '3'", auto sub () { C15 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime union<int, hash<auto>> -= %y %s", v, v.type()), auto sub () {
        union<int, hash<auto>> u; u -= v; return u; });
    }
    show("local *int += 2", auto sub () { *int u; u += 2; return u; });
    show("global *int += 2", auto sub () { remove g16_p; g16_p += 2; return g16_p; });
    show("member *int += 2", auto sub () { C16 o(); o.p += 2; return o.p; });
    show("local *int += 2.5", auto sub () { *int u; u += 2.5; return u; });
    show("global *int += 2.5", auto sub () { remove g16_p; g16_p += 2.5; return g16_p; });
    show("member *int += 2.5", auto sub () { C16 o(); o.p += 2.5; return o.p; });
    show("local *int += 2.5n", auto sub () { *int u; u += 2.5n; return u; });
    show("global *int += 2.5n", auto sub () { remove g16_p; g16_p += 2.5n; return g16_p; });
    show("member *int += 2.5n", auto sub () { C16 o(); o.p += 2.5n; return o.p; });
    show("local *int += '3'", auto sub () { *int u; u += "3"; return u; });
    show("global *int += '3'", auto sub () { remove g16_p; g16_p += "3"; return g16_p; });
    show("member *int += '3'", auto sub () { C16 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *int += %y %s", v, v.type()), auto sub () { *int u; u += v; return u; });
    }
    show("local *int -= 2", auto sub () { *int u; u -= 2; return u; });
    show("global *int -= 2", auto sub () { remove g16_m; g16_m -= 2; return g16_m; });
    show("member *int -= 2", auto sub () { C16 o(); o.m -= 2; return o.m; });
    show("local *int -= 2.5", auto sub () { *int u; u -= 2.5; return u; });
    show("global *int -= 2.5", auto sub () { remove g16_m; g16_m -= 2.5; return g16_m; });
    show("member *int -= 2.5", auto sub () { C16 o(); o.m -= 2.5; return o.m; });
    show("local *int -= 2.5n", auto sub () { *int u; u -= 2.5n; return u; });
    show("global *int -= 2.5n", auto sub () { remove g16_m; g16_m -= 2.5n; return g16_m; });
    show("member *int -= 2.5n", auto sub () { C16 o(); o.m -= 2.5n; return o.m; });
    show("local *int -= '3'", auto sub () { *int u; u -= "3"; return u; });
    show("global *int -= '3'", auto sub () { remove g16_m; g16_m -= "3"; return g16_m; });
    show("member *int -= '3'", auto sub () { C16 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *int -= %y %s", v, v.type()), auto sub () { *int u; u -= v; return u; });
    }
    show("local *float += 2", auto sub () { *float u; u += 2; return u; });
    show("global *float += 2", auto sub () { remove g17_p; g17_p += 2; return g17_p; });
    show("member *float += 2", auto sub () { C17 o(); o.p += 2; return o.p; });
    show("local *float += 2.5", auto sub () { *float u; u += 2.5; return u; });
    show("global *float += 2.5", auto sub () { remove g17_p; g17_p += 2.5; return g17_p; });
    show("member *float += 2.5", auto sub () { C17 o(); o.p += 2.5; return o.p; });
    show("local *float += 2.5n", auto sub () { *float u; u += 2.5n; return u; });
    show("global *float += 2.5n", auto sub () { remove g17_p; g17_p += 2.5n; return g17_p; });
    show("member *float += 2.5n", auto sub () { C17 o(); o.p += 2.5n; return o.p; });
    show("local *float += '3'", auto sub () { *float u; u += "3"; return u; });
    show("global *float += '3'", auto sub () { remove g17_p; g17_p += "3"; return g17_p; });
    show("member *float += '3'", auto sub () { C17 o(); o.p += "3"; return o.p; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *float += %y %s", v, v.type()), auto sub () { *float u; u += v; return u; });
    }
    show("local *float -= 2", auto sub () { *float u; u -= 2; return u; });
    show("global *float -= 2", auto sub () { remove g17_m; g17_m -= 2; return g17_m; });
    show("member *float -= 2", auto sub () { C17 o(); o.m -= 2; return o.m; });
    show("local *float -= 2.5", auto sub () { *float u; u -= 2.5; return u; });
    show("global *float -= 2.5", auto sub () { remove g17_m; g17_m -= 2.5; return g17_m; });
    show("member *float -= 2.5", auto sub () { C17 o(); o.m -= 2.5; return o.m; });
    show("local *float -= 2.5n", auto sub () { *float u; u -= 2.5n; return u; });
    show("global *float -= 2.5n", auto sub () { remove g17_m; g17_m -= 2.5n; return g17_m; });
    show("member *float -= 2.5n", auto sub () { C17 o(); o.m -= 2.5n; return o.m; });
    show("local *float -= '3'", auto sub () { *float u; u -= "3"; return u; });
    show("global *float -= '3'", auto sub () { remove g17_m; g17_m -= "3"; return g17_m; });
    show("member *float -= '3'", auto sub () { C17 o(); o.m -= "3"; return o.m; });
    foreach auto v in (Runtime) {
        show(sprintf("runtime *float -= %y %s", v, v.type()), auto sub () { *float u; u -= v; return u; });
    }
    # through a reference
    show("reference *union<int, float> -= 2.5", auto sub () {
        *union<int, float> u;
        reference<*union<int, float>> r = \u;
        r -= 2.5;
        return u;
    });
    show("reference *union<int, float> += 2.5n", auto sub () {
        *union<int, float> u;
        reference<*union<int, float>> r = \u;
        r += 2.5n;
        return u;
    });
    show("reference *union<hash<auto>, int> += 2", auto sub () {
        *union<hash<auto>, int> u;
        reference<*union<hash<auto>, int>> r = \u;
        r += 2;
        return u;
    });
    show("reference union<int, list<int>> -= 2", auto sub () {
        union<int, list<int>> u;
        reference<union<int, list<int>>> r = \u;
        r -= 2;
        return u;
    });
}

main();
