%modern
# qore#6498: lookups through a class reached through a private and a public path (see most-permissive-access.qtest)

# A is reached through a private path (B) and a public path (C)
class A {
    string m(int i) { return "A"; }
    static string s() { return "As"; }
    public { int pm = 1; }
}
class B inherits private A {}
class C inherits A {}
class BI inherits private:internal A {}
class C2 inherits A { string m(int i) { return "C2"; } }

# diamonds: the private path first, the public path first, the private:internal path first
class D1 inherits B, C {
    list<auto> inside() {
        auto self_any = self;
        code c = \self_any.m();
        return (self_any.m(1), call_object_method(self, "m", 1), self.hasCallableMethod("m"), c(1),
            self_any instanceof A, cast<A>(self_any).m(1));
    }
}
class D2 inherits C, B {}
class D3 inherits BI, C {}
# a different method of the same name through the public path
class D7 inherits B, C2 {
    string inside() { auto self_any = self; return self_any.m(1); }
}
# inherited variants of a method declared by the class
class D5 inherits B, C { string m(string s) { return "D5"; } }

# no diamond: a single public path and a single private path
class P inherits C {}
class Q inherits B {}
class Q5 inherits B { string m(string s) { return "Q5"; } }
class P5 inherits C { string m(string s) { return "P5"; } }
# A only through private paths (one of them private at its first edge only): its variants stay private
class W1 inherits A {}
class Y1 inherits private W1 {}
class D8 inherits B, Y1 { string m(string s) { return "D8"; } }
class D9 inherits Y1, B { string m(string s) { return "D9"; } }
# no diamond: the same private path
class E1 inherits Y1 { string m(string s) { return "E1"; } }
# a method declared by a class whose own hierarchy reaches A through a private path only: its inherited variants are
# those of its own hierarchy, also in a subclass that reaches A publicly
class R inherits Q5, C {}
# a call reference made in a class outside the hierarchy calls the method that a call from outside finds
class U {
    static auto ref(object o) {
        code c = \o.m();
        return c(1);
    }
}

auto sub ex(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<auto> sub results(object o, object o5) {
    auto a = o;
    auto a5 = o5;
    return {
        "method": ex(auto sub () { return a.m(1); }),
        "call_object_method": ex(auto sub () { return call_object_method(a, "m", 1); }),
        "hasCallableMethod": ex(auto sub () { return a.hasCallableMethod("m"); }),
        "call_static_method": ex(auto sub () { return call_static_method(a.className(), "s"); }),
        "call reference": ex(auto sub () { code c = \a.m(); return c(1); }),
        "member": ex(auto sub () { return a.pm; }),
        "instanceof": ex(auto sub () { return a instanceof A; }),
        "cast": ex(auto sub () { return cast<A>(a).m(1); }),
        "parameter": ex(auto sub () { code f = string sub (A x) { return x.m(1); }; return f(a); }),
        "inherited variant": ex(auto sub () { return a5.m(1); }),
    };
}

# the typed lookups, resolved at parse time
hash<auto> typed = {
    "D1": (D1::s(), (new D1()).m(1), (new D1()).pm, (new D5()).m(1), (new D1()) instanceof A,
        cast<A>(new D1()).m(1)),
    "D2": (D2::s(), (new D2()).m(1), (new D2()).pm),
    "D3": (D3::s(), (new D3()).m(1), (new D3()).pm),
    "P": (P::s(), (new P()).m(1), (new P()).pm),
};
printf("typed: %y\n", typed);
printf("D1: %y\n", results(new D1(), new D5()));
printf("D2: %y\n", results(new D2(), new D5()));
printf("D3: %y\n", results(new D3(), new D5()));
printf("P: %y\n", results(new P(), new P5()));
printf("Q: %y\n", results(new Q(), new Q5()));
printf("inside D1: %y\n", (new D1()).inside());
printf("D8: %y %y\n", ex(auto sub () { auto a = new D8(); return a.m(1); }), (new D8()) instanceof A);
printf("D9: %y E1: %y\n", ex(auto sub () { auto a = new D9(); return a.m(1); }),
    ex(auto sub () { auto a = new E1(); return a.m(1); }));
printf("R: %y %y\n", ex(auto sub () { auto a = new R(); return a.m(1); }), (new R()) instanceof A);
printf("U: %y %y\n", U::ref(new D7()), U::ref(new D1()));
printf("D7: %y %y %y\n", (new D7()).m(1), ex(auto sub () { auto a = new D7(); return a.m(1); }), (new D7()).inside());
