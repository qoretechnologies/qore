# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of union-enum-member.qtest: the values of an enum assigned to unions with the enum as a member type in any
# position (qore#6277)

%modern

enum Color : int { Red = 1, Green = 2 }
enum Name : string { A = "a", B = "b" }
enum Ratio : float { Half = 0.5 }

class C {
    public {
        union<string, Color> m;
    }
}

hashdecl H {
    union<string, Color> m;
}

#! members of soft types before the enum: the values keep their enum type
class CM {
    public {
        union<softstring, Color> m = Color::Red;
        union<softint, Name> n = Name::A;
    }
}

hashdecl HM {
    union<softint, Name> n;
}

#! a softlist value typed at run time
auto sub softlistTyped(auto v) {
    union<int, softlist<int>, Color> u = v;
    return u;
}

union<string, Color> sub ret(Color c) {
    return c;
}

auto sub runtimeTyped(auto v) {
    union<int, Color> u = v;
    return u;
}

sub show(string label, code c) {
    try {
        auto v = c();
        printf("%s: %y %s\n", label, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

sub main() {
    show("union<string, Color>", auto sub () { union<string, Color> u = Color::Red; return u; });
    show("union<Color, string>", auto sub () { union<Color, string> u = Color::Red; return u; });
    show("union<int, Name>", auto sub () { union<int, Name> u = Name::A; return u; });
    show("union<int, Color>", auto sub () { union<int, Color> u = Color::Red; return u; });
    show("union<Color, int>", auto sub () { union<Color, int> u = Color::Red; return u; });
    show("union<int, Color> int", auto sub () { union<int, Color> u = 5; return u; });
    show("union<string, Color> string", auto sub () { union<string, Color> u = "x"; return u; });
    show("union<string, int, Color>", auto sub () { union<string, int, Color> u = Color::Green; return u; });
    show("union<Name, Color> Color", auto sub () { union<Name, Color> u = Color::Green; return u; });
    show("union<Name, Color> Name", auto sub () { union<Name, Color> u = Name::B; return u; });
    show("union<string, Ratio>", auto sub () { union<string, Ratio> u = Ratio::Half; return u; });
    show("union<float, Ratio>", auto sub () { union<float, Ratio> u = Ratio::Half; return u; });
    show("union<string, int> Color", auto sub () { union<string, int> u = Color::Red; return u; });
    show("reassign", auto sub () { union<string, Color> u = "x"; u = Color::Green; return u; });
    show("*union<string, Color>", auto sub () { *union<string, Color> u = Color::Red; return u; });
    show("list<union<string, Color>>", auto sub () {
        list<union<string, Color>> l = (Color::Red, "x");
        return map $1.fullType(), l;
    });
    show("param", auto sub () { code f = auto sub (union<string, Color> p) { return p; }; return f(Color::Red); });
    show("hash value", auto sub () { hash<string, union<string, Color>> h = {"a": Color::Red}; return h.a; });
    show("object member", auto sub () { C c(); c.m = Color::Green; return c.m; });
    show("hashdecl member", auto sub () { hash<H> h = <H>{"m": Color::Green}; return h.m; });
    show("return", auto sub () { return ret(Color::Red); });
    show("union<softstring, Color>", auto sub () { union<softstring, Color> u = Color::Red; return u; });
    show("union<softint, Name>", auto sub () { union<softint, Name> u = Name::A; return u; });
    show("value typed at run time", auto sub () { return runtimeTyped(Color::Green); });
    show("int typed at run time", auto sub () { return runtimeTyped(7); });
    show("another enum", auto sub () { return runtimeTyped(Name::A); });
    show("union<list<int>, nothing, int, Color>", auto sub () {
        union<list<int>, nothing, int, Color> u = Color::Red;
        return u;
    });
    show("union<hash<string, int>, nothing, int, Color>", auto sub () {
        union<hash<string, int>, nothing, int, Color> u = Color::Red;
        return u;
    });

    # hash keys and members of soft types before the enum
    Name n = Name::A;
    show("member union<softstring, Color>", auto sub () { CM c(); return c.m; });
    show("member union<softint, Name>", auto sub () { CM c(); return c.n; });
    show("hashdecl member union<softint, Name>", auto sub () { hash<HM> h = <HM>{"n": Name::A}; return h.n; });
    show("hash literal value union<softint, Name>", auto sub () {
        hash<string, union<softint, Name>> h = {"a": n};
        return h.a;
    });
    show("hash key assignment union<softint, Name>", auto sub () {
        hash<string, union<softint, Name>> h();
        h.a = n;
        return h.a;
    });
    show("hash value typed at run time union<softstring, Color>", auto sub () {
        auto v = Color::Green;
        hash<string, union<softstring, Color>> h = {"a": v};
        return h.a;
    });

    # soft lists before the enum
    show("union<softlist<string>, Name>", auto sub () { union<softlist<string>, Name> u = Name::A; return u; });
    show("union<softlist, Color> typed at run time", auto sub () {
        auto v = Color::Red;
        union<softlist, Color> u = v;
        return u;
    });
    show("union<int, softlist<int>, Color> typed at run time", auto sub () { return softlistTyped(Color::Red); });
    show("union<softlist<int>, nothing, Color>", auto sub () {
        union<softlist<int>, nothing, Color> u = Color::Red;
        return u;
    });

    # a softlist with no enum type takes the base value, as before (qore#6277 review)
    show("softlist", auto sub () { softlist l = Color::Red; return l; });
    show("*softlist", auto sub () { *softlist l = Color::Red; return l; });
}

main();
