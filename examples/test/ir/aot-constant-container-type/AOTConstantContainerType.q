# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The value types of constants whose values hold lists of closures (#5925); see aot-constant-container-type.qtest

%modern

hashdecl HD {
    int a;
}

# without a declared type: the containers keep their own types
const C1 = {"l": cast<list<code>>((sub () {},))};
const C2 = (sub () {},);
const C3 = {"h": {"l": cast<list<code>>((sub () {},))}};
const C4 = {"l": cast<list<code>>((sub () {}, sub (int i) {}))};
# with a declared type: the value is retyped to it, which a soft container type leaves as it is
const hash<string, list<code>> X1 = {"l": cast<list<code>>((sub () {},))};
const hash<auto> X2 = {"l": cast<list<code>>((sub () {},))};
const hash<string, list<auto>> X3 = {"l": cast<list<code>>((sub () {},))};
const list<auto> X4 = (sub () {},);
const list<code> X5 = (sub () {},);
# constants of plain values, and of a hashdecl, whose types do not change
const P1 = {"a": 1, "l": (1, 2)};
const hash<HD> P2 = <HD>{"a": 2};
const P3 = cast<list<int>>((1, 2));

# without a declared type, of values that functions return: the types that the AST interpreter computed at compile
# time, which are more specific than the parse types of the calls
class Foo {
}
class Bar inherits Foo {
}
Foo sub ff() { return new Bar(); }
object sub fo() { return new Mutex(); }
code sub fc() { return sub (int x) {}; }
*int sub fn() { return 1; }
softint sub fs() { return 1; }
list<auto> sub fl() { return (1, 2); }
const F1 = (ff(), ff());
const F2 = (fo(),);
const F3 = (fc(),);
const F4 = cast<list<code>>((fc(),));
const F5 = (fn(), fn());
const F6 = (fs(),);
const F7 = (fl(), fl());
const F8 = ((sub () {},),);
string sub g(list<object<Bar>> l) { return "Bar variant"; }
string sub g(list<object<Foo>> l) { return "Foo variant"; }
string sub g(auto a) { return "auto variant"; }

class K {
    public {
        const KC = {"l": cast<list<code>>((sub () {},))};
        const hash<string, list<code>> KX = {"l": cast<list<code>>((sub () {},))};
    }
}

printf("C1 %s %s\n", C1.fullType(), C1.l.fullType());
printf("C2 %s\n", C2.fullType());
printf("C3 %s %s\n", C3.h.fullType(), C3.h.l.fullType());
printf("C4 %s\n", C4.l.fullType());
printf("X1 %s %s\n", X1.fullType(), X1.l.fullType());
printf("X2 %s %s\n", X2.fullType(), X2.l.fullType());
printf("X3 %s %s\n", X3.fullType(), X3.l.fullType());
printf("X4 %s\n", X4.fullType());
printf("X5 %s\n", X5.fullType());
printf("P1 %s %s\n", P1.fullType(), P1.l.fullType());
printf("P2 %s %y\n", P2.fullType(), P2);
printf("P3 %s\n", P3.fullType());
printf("F1 %s\nF2 %s\nF3 %s\nF4 %s\nF5 %s\nF6 %s\nF7 %s\nF8 %s\n", F1.fullType(), F2.fullType(), F3.fullType(),
    F4.fullType(), F5.fullType(), F6.fullType(), F7.fullType(), F8.fullType());
# the variant that a call selects by the type of the value
{
    auto a = F1;
    printf("F1 call: %s\n", g(a));
}
printf("KC %s %s\n", K::KC.fullType(), K::KC.l.fullType());
printf("KX %s %s\n", K::KX.fullType(), K::KX.l.fullType());

# a function that the tiered mode compiles to native code
int sub hot(int i) {
    return i + 1;
}
int n = 0;
for (int i = 0; i < 2000; ++i) {
    n = hot(n);
}
