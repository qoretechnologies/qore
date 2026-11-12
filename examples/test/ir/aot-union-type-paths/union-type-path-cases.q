#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the cases of aot-union-type-paths.qtest: union types with typed hash, class and enum members in every position that
# a compiled program records a type; each case prints its result or the exception raised (with the type it names),
# and the output of every execution mode and of AOT executables must be that of the AST interpreter

%modern

# values whose types the parser does not know, so that a type error is raised at runtime
auto Str = "x";
auto Flt = 2.5;

hashdecl H {
    int a = 1;
}

namespace NS {
    public hashdecl NH {
        string s = "x";
    }
    public enum Color {
        Red = "red",
        Blue = "blue",
    }
}

hashdecl P<T> {
    T v;
}

class K {
    public {
        union<hash<H>, int> m = 1;
        *union<int, hash<NS::NH>> om;
    }
}

union<hash<H>, int> sub ret_union(bool h) {
    return h ? cast<hash<H>>({"a": 5}) : 7;
}

string sub param_union(union<hash<NS::NH>, string> v) {
    return v.typeCode() == NT_HASH ? "hash " + v.s : "string " + v;
}

sub show(string label, code<auto()> c) {
    try {
        auto v = c();
        printf("%s => %y %s\n", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        # the type named by the error, whose text differs by the kind of lvalue in some execution modes
        *string type = (ex.desc =~ x/expects type '([^']+)'/)[0];
        printf("%s => %s %s\n", label, ex.err, type ?? "-");
    }
}

sub main() {
    show("local hashdecl first int", auto sub () { union<hash<H>, int> u = 1; return u; });
    show("local hashdecl first hash", auto sub () { union<hash<H>, int> u = cast<hash<H>>({"a": 2}); return u; });
    show("local hashdecl first refuses", auto sub () { union<hash<H>, int> u = 1; u = Str; return u; });
    show("local or-nothing hashdecl first +=", auto sub () { *union<hash<H>, int> u; u += 2; return u; });
    show("local or-nothing hashdecl first -=", auto sub () { *union<hash<H>, int> u; u -= 2; return u; });
    show("local hashdecl last refuses", auto sub () { union<int, hash<H>> u = 1; u = Str; return u; });
    show("local namespace hashdecl", auto sub () {
        union<hash<NS::NH>, int> u = cast<hash<NS::NH>>({"s": "y"});
        return u.s;
    });
    show("local namespace hashdecl refuses", auto sub () { union<hash<NS::NH>, int> u = 1; u = Flt; return u; });
    show("local class first", auto sub () { union<K, int> u = 3; u = new K(); return u.m; });
    show("local class refuses", auto sub () { union<K, int> u = 3; u = Str; return u; });
    show("local enum member", auto sub () { union<NS::Color, int> u = NS::Color::Blue; return u; });
    show("local enum refuses", auto sub () { union<NS::Color, int> u = 1; u = Flt; return u; });
    show("member hashdecl first", auto sub () { K k(); k.m = cast<hash<H>>({"a": 3}); return k.m.a; });
    show("member hashdecl refuses", auto sub () { K k(); k.m = Str; return k.m; });
    show("member or-nothing namespace hashdecl", auto sub () { K k(); k.om = 4; return k.om; });
    show("return hash", auto sub () { return ret_union(True).a; });
    show("return int", auto sub () { return ret_union(False); });
    show("param hash", auto sub () { return param_union(cast<hash<NS::NH>>({"s": "z"})); });
    show("param string", auto sub () { return param_union("w"); });
    show("hash value type refuses", auto sub () { hash<string, union<hash<H>, int>> h = {}; h.x = "s"; return h; });
    show("list element type", auto sub () {
        list<union<hash<H>, int>> l = (1, cast<hash<H>>({"a": 8}));
        return l[1].a;
    });
    # or-nothing and nested members, a parameterized hashdecl, list and hash members
    show("or-nothing member refuses", auto sub () { union<*int, hash<H>> u = 1; u = Flt; return u; });
    show("nested union member", auto sub () {
        union<int, *union<string, hash<H>>> u = 1;
        u = cast<hash<H>>({"a": 9});
        return u.a;
    });
    show("nested union refuses", auto sub () { union<int, *union<string, hash<H>>> u = 1; u = Flt; return u; });
    show("parameterized hashdecl member", auto sub () {
        union<hash<P<int>>, string> u = "s";
        u = cast<hash<P<int>>>({"v": 3});
        return u.v;
    });
    show("parameterized hashdecl refuses", auto sub () { union<hash<P<int>>, string> u = "s"; u = Flt; return u; });
    show("list member", auto sub () { union<list<hash<H>>, int> u = 1; u = (cast<hash<H>>({"a": 1}),); return u; });
    show("list member refuses", auto sub () { union<list<hash<H>>, int> u = 1; u = Str; return u; });
    show("hash member refuses", auto sub () { union<hash<string, hash<H>>, int> u = 1; u = Str; return u; });
    # a member given more than once counts once, also in the name of the type
    show("duplicate member refuses", auto sub () { union<hash<H>, int, hash<H>> u = 1; u = Str; return u; });
    show("duplicate type only refuses", auto sub () { union<int, int> u = 1; u = Flt; return u; });
    show("list element type refuses", auto sub () { list<union<hash<H>, int>> l = (1,); l[0] = Str; return l; });
}

main();
