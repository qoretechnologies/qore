%modern
%no-child-restrictions

# issue #5838: a call typed against a parent class trusts its return type in the compiled modes, so a child class
# variant that it can select must return a compatible type; the output is the same in every execution mode

class Item {
    string name() { return "item"; }
}
class SpecialItem inherits Item {
    string name() { return "special"; }
}

class Base {
    int m(int i) { return i; }
    *int n() { return NOTHING; }
    Item item() { return new Item(); }
    softstring s(int i) { return i; }
}

class Derived inherits Base {
    # the same type
    int m(int i) { return i * 10; }
    # a narrower type
    int n() { return 7; }
    # a subclass (a covariant return type)
    SpecialItem item() { return new SpecialItem(); }
    # a type that the parent class method's accepts without a conversion
    string s(int i) { return sprintf("<%d>", i); }
}

Base b = new Derived();
int z = b.m(1) + 1;
printf("m: %d\n", z);
*int n = b.n();
printf("n: %y\n", n);
printf("item: %s\n", b.item().name());
printf("s: %s\n", b.s(3));

# the issue's example is refused when it is parsed, so no execution mode can trust a type that the call does not
# return
Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES);
try {
    p.parse("class B { int m(int i) { return i; } }
class D inherits B { string m(softint i) { return \"derived\"; } }
int sub f() { B b = new D(); int z = b.m(1) + 1; return z; }", "issue-5838");
    printf("issue example: parsed\n");
} catch (hash<ExceptionInfo> ex) {
    printf("issue example: %s: %s\n", ex.err, (ex.desc =~ x/^(child class method D::m\(softint i\) returning string cannot overload parent class method B::m\(int i\) returning int)/)[0] ?? ex.desc);
}
