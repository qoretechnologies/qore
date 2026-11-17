# probes of private-internal-redeclared-member.qtest (qore#6393): a class two or more levels below a class with a
# private:internal member redeclares the member; the initializer of the private:internal member writes its own
# storage, which the methods of its class use, and the redeclared member keeps its own value.  Members can only be
# declared again without a type, which %modern code (and so IR, JIT and tiered execution and qcc) does not allow, so
# the probes are parsed with old-style and new-style options: each prints one line

%old-style

# the case of the issue: two levels
class A {
    private:internal { $.a = 1; }
    auto getA() { return $.a; }
    setA($v) { $.a = $v; }
}
class B inherits A { public { $.a; } }
class C inherits B { public { $.a; } }

# a further class that declares the member again, and one that does not
class B3 inherits B { public { $.a; } }
class D inherits B3 { public { $.a; } }
class B4 inherits B {}
class E inherits B4 { public { $.a; } }

# the redeclaring class has an initializer of its own
class F inherits B { public { $.a = 5; } }

# private members declared again
class PA {
    private:internal { $.p = "pa"; }
    auto getPA() { return $.p; }
}
class PB inherits PA { private { $.p; } }
class PC inherits PB {
    private { $.p; }
    auto getPC() { return $.p; }
}

# the redeclaring class is inherited with private:internal inheritance by a class whose subclass declares the member
# again
class IC inherits B {
    public { $.a = "c"; }
    auto getIC() { return $.a; }
}
class ID inherits private:internal IC {
    auto getID() { return $.getIC(); }
    auto getIDA() { return $.getA(); }
}
class IE inherits ID { public { $.a = "e"; } }

# a member declared again in two base classes over a member inherited with private:internal inheritance, and
# inherited from both by a class that does not declare it
class X { public { $.x; } }
class XD1 inherits private:internal X { setXD1($v) { $.x = $v; } }
class XD2 inherits private:internal X { auto getXD2() { return $.x; } setXD2($v) { $.x = $v; } }
class XP1 inherits XD1 { public { $.x; } }
class XP2 inherits XD2 { public { $.x; } }
class XR inherits XP1, XP2 {}

my B $b();
printf("old B: a=%y A=%y\n", $b.a, $b.getA());
my C $c();
printf("old C: a=%y A=%y\n", $c.a, $c.getA());
$c.setA(99);
printf("old C after setA(99): a=%y A=%y\n", $c.a, $c.getA());
my D $d();
printf("old D: a=%y A=%y\n", $d.a, $d.getA());
my E $e();
printf("old E: a=%y A=%y\n", $e.a, $e.getA());
my F $f();
printf("old F: a=%y A=%y\n", $f.a, $f.getA());
my PC $pc();
printf("old PC: p=%y PA=%y\n", $pc.getPC(), $pc.getPA());
my IE $ie();
printf("old IE: a=%y IC=%y A=%y\n", $ie.a, $ie.getID(), $ie.getIDA());
my XR $xr();
$xr.setXD2("d2");
printf("old XR: x=%y XD2=%y\n", $xr.x, $xr.getXD2());

%new-style

namespace NS {
# a typed private:internal member
class A {
    private:internal { int a = 1; }
    auto getA() { return a; }
}
class B inherits A { public { a; } }
class C inherits B { public { a; } }

# multiple inheritance: two private:internal members with the name of the redeclared member
class A2 {
    private:internal { string a = "a2"; }
    auto getA2() { return a; }
}
class B2 inherits A2 { public { a; } }
# both base classes declare the member
class M inherits B, B2 { public { a; } }
# the member is inherited from both base classes without being declared again
class N inherits B, B2 {}
class NN inherits N { public { a; } }

# private:internal inheritance
class PI inherits private:internal B {
    public { a; }
    auto getAThroughB() { return getA(); }
}

# the class is created in a method of a base class
class G inherits C {
    static C make() { return new G(); }
}
}

sub new_style() {
    NS::C c();
    printf("new C: a=%y A=%y\n", c.a, c.getA());
    NS::M m();
    printf("new M: a=%y A=%y A2=%y\n", m.a, m.getA(), m.getA2());
    NS::N n();
    printf("new N: a=%y A=%y A2=%y\n", n.a, n.getA(), n.getA2());
    NS::NN nn();
    printf("new NN: a=%y A=%y A2=%y\n", nn.a, nn.getA(), nn.getA2());
    NS::PI pi();
    printf("new PI: a=%y A=%y\n", pi.a, pi.getAThroughB());
    NS::C g = NS::G::make();
    printf("new G: a=%y A=%y\n", g.a, g.getA());
}
new_style();
