# -*- mode: qore; indent-tabs-mode: nil -*-
# the code that member-double-init.qtest parses into a Program: a member declared with an initializer in more than
# one class of a hierarchy, in the same storage (qore#6411); old-style code, as only code without %require-types can
# declare a member again in a class of the same hierarchy

%old-style
%strict-args

our $destroyed = ();
our $out = ();
our $p_count = 0;

class D {
    public { $.name; }
    constructor($n) { $.name = $n; }
    destructor() { push $destroyed, $.name; }
}

# a parent class's member that the class declares again with an initializer that reads the parent's value
class A { public { $.a = 1; } }
class B inherits A { public { $.a = $.a + 1; } }
# and again in the next class
class C inherits B { public { $.a = $.a * 10; } }

# a member declared again without an initializer keeps the parent's value
class E inherits A { public { $.a; } }

# two parent classes that declare the member, in the order of the inheritance list
class X { public { $.a = 1; } }
class Y { public { $.a = 2; } }
class Z inherits X, Y {}
# the second parent's declaration has no initializer
class W { public { $.a; } }
class Z2 inherits X, W {}

# a class inherited through two private:internal paths: its initializer runs once
class P { public { $.a = count_p(); } }
class B1 inherits private:internal P { public { $.a; } }
class B2 inherits private:internal P { public { $.a; } }
class N inherits B1, B2 {}

# values with references, whose replaced values are released: containers, and objects, whose destructors run when
# replaced; a container that holds an object needs a scan
class H {
    public {
        $.h = ("x": 1);
        $.l = (1,);
        $.o = new D("parent");
        $.lo = (new D("list"),);
        $.ln = (1, 2);
    }
}
class I inherits H {
    public {
        $.h = $.h + ("y": 2);
        $.l = $.l + 2;
        $.o = new D("child");
        $.lo = (1, 2);
        $.ln = (new D("new list"),);
    }
}

# the replaced value is one that the initializer assigned itself
class J inherits A { public { $.a = ($.a = new D("assigned"), new D("given"))[1]; } }

# an initializer that throws after an earlier one has set the member
class T { public { $.a = new D("t-parent"); } }
class U inherits T { public { $.a = throw_now(); } }

sub count_p() { return sprintf("p%d", ++$p_count); }

sub throw_now() { throw "INIT-ERROR", "initializer"; }

sub check($label, $v) {
    push $out, sprintf("%s: %y", $label, $v);
}

sub run() {
    $out = ();
    $destroyed = ();
    check("parent and class", (new B()).a);
    check("next class", (new C()).a);
    check("no initializer", (new E()).a);
    check("two parents", (new Z()).a);
    check("second parent without initializer", (new Z2()).a);
    {
        $p_count = 0;
        my $n = new N();
        check("private:internal paths", ($n.a, $p_count));
    }
    {
        my $i = new I();
        check("containers", ($i.h, $i.l, $i.lo, $i.ln[0].name));
        check("object", ($i.o.name, $destroyed));
    }
    check("object released", $destroyed);
    $destroyed = ();
    {
        my $j = new J();
        check("assigned in the initializer", ($j.a.name, $destroyed));
    }
    $destroyed = ();
    try {
        new U();
    } catch ($ex) {
        check("exception", ($ex.err, $destroyed));
    }
}

sub get_output() {
    run();
    return $out;
}
