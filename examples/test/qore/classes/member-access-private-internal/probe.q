#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the probe of member-access-private-internal.qtest: for each class context and object, prints the result of a call
# of a method of class A on the object and of a read and a write of a member of A, when the object's class inherits A
# privately, with private:internal, or through a private:internal and a public base class

%modern
%no-child-restrictions

#! the classes; BODY runs one operation in the class context of each function
const Classes = "
class A {
    public { int pub = 1; }
    int m() { return 2; }
    # A's own methods use the member on objects of every class that inherits A
    int getPub() { return pub; }
    static auto fromA(string obj, string op) { BODY }
}
class B inherits private A {
    static auto fromB(string obj, string op) { BODY }
}
class C inherits B {
    static auto fromC(string obj, string op) { BODY }
}
class BI inherits private:internal A {
    static auto fromBI(string obj, string op) { BODY }
}
class CI inherits BI {
    static auto fromCI(string obj, string op) { BODY }
}
class B1 inherits A {}
class B2 inherits A {}
# A through a private:internal base and a public one, in both orders
class D1 inherits private:internal B1, B2 {
    static auto fromD1(string obj, string op) { BODY }
}
class D2 inherits B2, private:internal B1 {
}
# A through a private and a public base class
class BP inherits private A {}
class DP inherits BP, B2 {}
# another base class of the class that inherits A with private:internal
class P {
    static auto fromP(string obj, string op) { BODY }
}
class BIP inherits private:internal A, P {
}
class X {
    static auto fromX(string obj, string op) { BODY }
}
auto sub outside(string obj, string op) { BODY }
auto sub in_class(string cls, string obj, string op) {
    switch (cls) {
        case 'A': return A::fromA(obj, op);
        case 'B': return B::fromB(obj, op);
        case 'C': return C::fromC(obj, op);
        case 'BI': return BI::fromBI(obj, op);
        case 'CI': return CI::fromCI(obj, op);
        case 'D1': return D1::fromD1(obj, op);
        case 'X': return X::fromX(obj, op);
        case 'P': return P::fromP(obj, op);
    }
}
";

#! one operation on an object of the class named
const Body = "
    object o;
    switch (obj) {
        case 'B': o = new B(); break;
        case 'C': o = new C(); break;
        case 'BI': o = new BI(); break;
        case 'CI': o = new CI(); break;
        case 'D1': o = new D1(); break;
        case 'D2': o = new D2(); break;
        case 'DP': o = new DP(); break;
        case 'BIP': o = new BIP(); break;
    }
    switch (op) {
        case 'm': return o.m();
        case 'pub': return o.pub;
        case 'set': o.pub = 5; return o.pub;
        case 'self': return o.getPub();
        case 'keys': if (keys o) { throw 'KEYS', (keys o).join(','); } return;
    }
";

Program p(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
p.parse("%modern\n%disable-warning non-existent-method-call\n" + replace(Classes, "BODY", Body), "member");
foreach string ctx in ("outside", "A", "B", "C", "BI", "CI", "D1", "X", "P") {
    foreach string obj in ("B", "C", "BI", "CI", "D1", "D2", "DP", "BIP") {
        list<string> res = ();
        foreach string op in ("m", "pub", "set", "self", "keys") {
            string r;
            try {
                if (ctx == "outside") {
                    p.callFunction("outside", obj, op);
                } else {
                    p.callFunction("in_class", ctx, obj, op);
                }
                r = "ok";
            } catch (hash<ExceptionInfo> ex) {
                r = ex.err == "KEYS" ? ex.desc : ex.err;
            }
            res += sprintf("%s=%s", op, r);
        }
        printf("%s %s: %s\n", ctx, obj, foldl $1 + " " + $2, res);
    }
}

# member access resolved when parsing (a typed object)
const Prefix = "class A { public { int pub = 1; } }\nclass BI inherits private:internal A {}\n"
    "class B1 inherits A {}\nclass B2 inherits A {}\nclass D1 inherits private:internal B1, B2 {}\n";
const Typed = (
    "subclass of the inheriting class": Prefix + "class CI2 inherits BI { static int f(BI o) { return o.pub; } }",
    "unrelated subclass of the base class": Prefix + "class A2 inherits A { static int f(BI o) { return o.pub; } }",
    "the inheriting class": Prefix + "class BI2 inherits private:internal A { static int f(BI2 o) { return o.pub; } }",
    "the base class itself": "class A { public { int pub = 1; } static int f(BI o) { return o.pub; } }\n"
        "class BI inherits private:internal A {}\n",
    "outside, through a public path": Prefix + "int sub f(D1 o) { return o.pub; }",
);
foreach hash<auto> i in (Typed.pairIterator()) {
    Program t(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
    string r;
    try {
        t.parse("%modern\n" + i.value, "typed");
        r = "parses";
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err + ": " + ex.desc;
    }
    printf("typed, %s: %s\n", i.key, r);
}

# a name in a class (issue #6667): a member reached only through a base class that a base class inherits with
# private:internal is not a candidate; the name resolves to the constant of the class, and a private:internal member
# of the base class is used by its own methods
const Names = "class G { public { int v = 1; } }
class P inherits private:internal G {
    private:internal { int w = 3; }
    int getW() { return w; }
}
class C inherits P {
    const v = 2;
    int get() { return v; }
}
auto sub run(string what) {
    switch (what) {
        case 'constant': return (new C()).get();
        case 'own internal member': return (new C()).getW();
    }
}
";
{
    Program n(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
    n.parse("%modern\n" + Names, "names");
    foreach string w in ("constant", "own internal member") {
        printf("name, %s: %y\n", w, n.callFunction("run", w));
    }
}
# a private:internal member of a base class is not a candidate for a name in a subclass
{
    Program n(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
    string r;
    try {
        n.parse("%modern\n" + Names + "class NoCandidate inherits P { auto get() { return w; } }\n", "names");
        r = "parses";
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    printf("name, private:internal member of the base class in a subclass: %s\n", r);
}
# a member that a subclass declares with the name of a member hidden behind a private:internal base class of its base
# class is still a conflicting declaration, as before (the name resolution does not change the declaration checks)
{
    Program n(PO_NEW_STYLE | PO_NO_CHILD_PO_RESTRICTIONS, True);
    string r;
    try {
        n.parse("%modern\n" + Names + "class R inherits P { public { string v = 'r'; } }\n", "names");
        r = "parses";
    } catch (hash<ExceptionInfo> ex) {
        r = ex.err;
    }
    printf("name, a member redeclared in a subclass: %s\n", r);
}
