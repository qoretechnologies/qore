#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of union-member-types.qtest: each prints the value that a union-typed member, element or variable holds,
# or the exception raised; the output of every execution mode must be that of the AST interpreter

%modern

hashdecl HU {
    union<int, float> f = 1;
    *union<int, string> s;
}

class CU {
    public {
        union<int, float> f = 1;
        *union<int, string> s;
    }
}

sub show(string label, code<auto()> c) {
    try {
        auto v = c();
        printf("%s => %y %s\n", label, v, v.type());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", label, ex.err);
    }
}

sub main() {
    # a complex hash whose value type is a union: the value is read as it is, not as the first member type
    show("hash const key float", auto sub () { hash<string, union<int, float>> h = {"x": 1}; h.x = 2.5; return h.x; });
    show("hash const key int", auto sub () { hash<string, union<int, float>> h = {"x": 1}; h.x = 3; return h.x; });
    show("hash init float", auto sub () { hash<string, union<int, float>> h = {"x": 2.5}; return h.x; });
    show("hash or-nothing float", auto sub () { hash<string, *union<int, float>> h = {}; h.x = 2.5; return h.x; });
    show("hash or-nothing missing", auto sub () { hash<string, *union<int, float>> h = {}; return h.x; });
    show("hash string member", auto sub () { hash<string, union<int, string>> h = {"x": "abc"}; return h.x; });
    show("hash bool member", auto sub () { hash<string, union<int, bool>> h = {"x": True}; return h.x; });
    show("hash list member", auto sub () { hash<string, union<int, list<int>>> h = {"x": (1, 2)}; return h.x; });
    show("hash float first", auto sub () { hash<string, union<float, int>> h = {"x": 2}; return h.x; });
    show("hash dynamic key", auto sub () {
        hash<string, union<int, float>> h = {"x": 1};
        string k = "x";
        h{k} = 2.5;
        return h{k};
    });
    show("hash member arithmetic", auto sub () { hash<string, union<int, float>> h = {"x": 2.5}; return h.x + 1; });
    show("hash member compare", auto sub () { hash<string, union<int, float>> h = {"x": 2.5}; return h.x > 2; });
    show("hash member string concat", auto sub () {
        hash<string, union<int, string>> h = {"x": "a"};
        return h.x + "b";
    });
    show("hash or-nothing int missing", auto sub () { hash<string, *int> h = {"a": 1}; return h.x; });
    # a list whose element type is a union
    show("list element float", auto sub () { list<union<int, float>> l = (1,); l[0] = 2.5; return l[0]; });
    show("list element string", auto sub () { list<union<int, string>> l = ("abc",); return l[0]; });
    # members of a hashdecl and a class of union type
    show("hashdecl member float", auto sub () { hash<HU> h(); h.f = 2.5; return h.f; });
    show("hashdecl member string", auto sub () { hash<HU> h(); h.s = "abc"; return h.s; });
    show("class member float", auto sub () { CU o(); o.f = 2.5; return o.f; });
    show("class member string", auto sub () { CU o(); o.s = "abc"; return o.s; });
    # a local of union type
    show("local arithmetic", auto sub () { union<int, float> v = 2.5; return v + 1; });
    show("local string", auto sub () { union<int, string> v = "abc"; return v; });
    show("local compare", auto sub () { union<int, float> v = 2.5; return v > 2; });
    # compound assignments to a union-typed hash member
    show("hash member -=", auto sub () { hash<string, union<int, float>> h = {"x": 2.5}; h.x -= 1; return h.x; });
    show("hash member *=", auto sub () { hash<string, union<int, float>> h = {"x": 2.5}; h.x *= 2; return h.x; });
    show("hash member ++", auto sub () { hash<string, union<int, float>> h = {"x": 2.5}; h.x++; return h.x; });
    show("hash date member", auto sub () {
        hash<string, union<int, date>> h = {"x": 2026-01-01T00:00:00Z};
        return h.x;
    });
    show("hash number member", auto sub () { hash<string, union<int, number>> h = {"x": 2.5n}; return h.x; });
    show("hash nested union member", auto sub () {
        hash<string, union<int, union<float, string>>> h = {"x": "s"};
        return h.x;
    });
    # element types of a list of a union type
    show("map list elements", auto sub () { list<union<string, int>> l = ("a", 1); return map $1 * 2, l; });
    show("foldl list elements", auto sub () { list<union<string, int>> l = ("a", 1, 2); return foldl $1 + $2, l; });
    show("select list elements", auto sub () {
        list<union<int, float>> l = (1, 2.5, 3);
        return select l, $1 > 2;
    });
    # a single value type with no value: still that type
    show("one value type in two members", auto sub () { hash<string, union<int, *int>> h = {"x": 2}; return h.x; });
    show("or-nothing single union", auto sub () { hash<string, *union<int>> h = {"x": 2}; return h.x; });
}

main();
