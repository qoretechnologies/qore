# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of union-enum-member.qtest: the values of an enum assigned to unions with the enum as a member type in any
# position (qore#6277)

%modern

enum Color : int { Red = 1, Green = 2 }
enum Name : string { A = "a", B = "b" }
enum Ratio : float { Half = 0.5 }

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
}

main();
