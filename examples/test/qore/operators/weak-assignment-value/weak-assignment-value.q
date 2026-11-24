%modern
%allow-weak-references
%allow-opaque-references
# the program of weak-assignment-value.qtest: the value of a weak assignment (qore#7084), and the lifetime of the
# temporary that it is given (qore#7085); every execution mode must print the output of the AST interpreter

our int destroyed = 0;

class C {
    destructor() { ++destroyed; }
    string n() { return "C"; }
}

*object sub id(*object o) { return o; }

our *object g;
our auto ga;
thread_local *object tl;

#! describes a value: the class of an object ("deleted" for a destroyed one), or the type of another value
string sub desc(auto v) {
    try {
        return v.typeCode() == NT_OBJECT ? v.className() : v.type();
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == "OBJECT-ALREADY-DELETED") {
            return "deleted";
        }
        rethrow;
    }
}

#! the value of a weak assignment is the value assigned: what keeps it after the statement holds the object
sub values() {
    # at function level: native code gave the consumer the weak reference (qore#7084)
    C c1();
    *object w1;
    code f1 = auto sub () { return w1; };
    auto v1 = (w1 := c1);
    remove c1;
    printf("function level closure value: %s\n", desc(v1));
    remove v1;
    printf("function level closure value released: %s\n", desc(f1()));

    C c2();
    auto v2 = (g := c2);
    remove c2;
    printf("function level global value: %s\n", desc(v2));
    remove v2;

    C c3();
    *object w3;
    auto v3 = (w3 := c3);
    remove c3;
    printf("function level plain value: %s\n", desc(v3));
    remove v3;
    printf("function level plain value released: %s\n", desc(w3));

    C c4();
    auto v4 = (tl := c4);
    remove c4;
    printf("function level thread_local value: %s\n", desc(v4));
    remove v4;
    {
        C c();
        *object w;
        code f = auto sub () { return w; };
        list<auto> l = ((w := c),);
        hash<auto> h = {"a": (w := c)};
        auto a = id(w := c);
        remove c;
        printf("closure list %s hash %s argument %s\n", desc(l[0]), desc(h.a), desc(a));
    }
    {
        C c();
        *object w;
        list<auto> l = ((w := c),);
        hash<auto> h = {"a": (w := c)};
        auto a = id(w := c);
        remove c;
        printf("plain list %s hash %s argument %s\n", desc(l[0]), desc(h.a), desc(a));
    }
    {
        C c();
        list<auto> l = ((g := c), (tl := c));
        remove c;
        printf("global %s thread_local %s\n", desc(l[0]), desc(l[1]));
    }
    {
        # an opaque assignment owns its target: so does its value
        C c();
        *object w;
        auto v = (w @= c);
        remove c;
        remove w;
        printf("plain opaque value: %s\n", desc(v));
        *object cw;
        code f = auto sub () { return cw; };
        C c5();
        auto v5 = (cw @= c5);
        remove c5;
        remove cw;
        printf("closure opaque value: %s\n", desc(v5));
    }
}

our float gf;
our softint gsi;
our hash gh;
thread_local hash tlh;

#! the value of a weak assignment is the value assigned, converted to the variable's type
sub conversions() {
    float f;
    number n;
    softint si;
    hash<auto> h;
    printf("plain float %y number %y softint %y hash %y\n", (f := 1), (n := 2), (si := "3"),
        (h := cast<hash<string, int>>({"a": 1})).fullType());
    float cf;
    softint csi;
    code c = auto sub () { return (cf, csi); };
    auto v1 = (cf := 1);
    auto v2 = (csi := "4");
    printf("closure float %y %s softint %y %s\n", v1, v1.type(), v2, v2.type());
    auto v3 = (gf := 5);
    auto v4 = (gsi := "6");
    printf("global float %y %s softint %y %s\n", v3, v3.type(), v4, v4.type());
    list<auto> l = ((f := 7), (cf := 8), (gf := 9));
    printf("list %y\n", l);
    # a container whose type the variable's type strips: the weak reference designates the container assigned, which
    # is the value of the assignment, and which the variable reads while it exists (qore#7085, qore#7237)
    hash<string, int> th = {"a": 1};
    hash ph;
    auto v5 = (ph := th);
    printf("plain strip: %y %s ph=%y\n", v5, v5.fullType(), ph);
    hash ch;
    code c2 = auto sub () { return ch; };
    auto v6 = (ch := th);
    printf("closure strip: %y %s ch=%y\n", v6, v6.fullType(), c2());
    remove v5;
    remove v6;
    printf("strip released: ph=%y ch=%y\n", ph, c2());
    printf("global strip: %y thread_local strip: %y\n", (gh := th), (tlh := th));
}

#! a class whose destructor raises an exception: releasing the previous value of a variable fails
class Raise {
    destructor() { throw "DESTRUCTOR-ERROR"; }
}

#! a weak assignment whose release of the variable's previous value raises an exception gives no value, and the
#! object assigned is destroyed when its last reference is released
sub store_errors() {
    *object pw;
    *object cw;
    code f = auto sub () { return cw; };
    foreach string what in (("plain", "closure", "global", "thread_local")) {
        switch (what) {
            case "plain": pw = new Raise(); break;
            case "closure": cw = new Raise(); break;
            case "global": g = new Raise(); break;
            case "thread_local": tl = new Raise(); break;
        }
        int before = destroyed;
        C c();
        try {
            switch (what) {
                case "plain": printf("%s value: %s\n", what, (pw := c).n()); break;
                case "closure": printf("%s value: %s\n", what, (cw := c).n()); break;
                case "global": printf("%s value: %s\n", what, (g := c).n()); break;
                case "thread_local": printf("%s value: %s\n", what, (tl := c).n()); break;
            }
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s\n", what, ex.err);
        }
        remove c;
        printf("%s: destroyed=%d\n", what, destroyed - before);
    }
}

#! a temporary that a weak assignment is given lives until the end of the statement, in every chain (qore#7085); the
#! destroyed objects are counted in the statement itself
sub temporaries() {
    *object pl;
    auto al;
    auto ca;
    code fa = auto sub () { return ca; };
    *object ct;
    code ft = auto sub () { return ct; };
    int before = destroyed;
    printf("plain typed: %s destroyed=%d\n", desc(pl := new C()), destroyed - before);
    printf("plain typed after the statement: %s destroyed=%d\n", desc(pl), destroyed - before);
    before = destroyed;
    printf("plain auto: %s destroyed=%d\n", desc(al := new C()), destroyed - before);
    printf("closure auto: %s destroyed=%d\n", desc(ca := new C()), destroyed - before);
    printf("closure typed: %s destroyed=%d\n", desc(ct := new C()), destroyed - before);
    printf("closure typed after the statement: %s\n", desc(ft()));
    before = destroyed;
    printf("global auto: %s destroyed=%d\n", desc(ga := new C()), destroyed - before);
    printf("global typed: %s destroyed=%d\n", desc(g := new C()), destroyed - before);
    printf("thread_local: %s destroyed=%d\n", desc(tl := new C()), destroyed - before);
    # the value is the receiver of a call
    printf("plain receiver: %s\n", (pl := new C()).n());
    printf("global receiver: %s\n", (g := new C()).n());
    printf("closure receiver: %s\n", (ct := new C()).n());
    printf("thread_local receiver: %s\n", (tl := new C()).n());
    # each element of a map: the temporary lives until the end of the element's evaluation
    before = destroyed;
    printf("map plain: %y\n", (map desc(pl := new C()) + ":" + (destroyed - before), (1, 2)));
    before = destroyed;
    printf("map closure: %y\n", (map desc(ct := new C()) + ":" + (destroyed - before), (1, 2)));
}

#! an object whose members are assigned through lvalue paths
class M {
    public {
        *object m;
        float f;
        hash h;
    }

    #! assigns members of self
    run() {
        int before = destroyed;
        printf("self member temp: %s destroyed=%d\n", desc(m := new C()), destroyed - before);
        C c();
        auto v = (m := c);
        remove c;
        printf("self member value: %s\n", desc(v));
        remove v;
        printf("self member receiver: %s\n", (m := new C()).n());
        auto vf = (f := 1);
        printf("self member float: %y %s\n", vf, vf.type());
        hash<string, int> th = {"a": 1};
        printf("self member strip: %y\n", (h := th));
    }
}

hashdecl WeakHolder {
    *object o;
}

#! weak assignments through lvalue paths: members, hash keys, list elements, a softlist variable (qore#7122)
sub paths() {
    new M().run();
    M o();
    int before = destroyed;
    printf("member temp: %s destroyed=%d\n", desc(o.m := new C()), destroyed - before);
    C c();
    auto v = (o.m := c);
    remove c;
    printf("member value: %s\n", desc(v));
    remove v;
    printf("member receiver: %s\n", (o.m := new C()).n());

    hash<auto> h;
    before = destroyed;
    printf("hash key temp: %s destroyed=%d\n", desc(h.k := new C()), destroyed - before);
    C c2();
    auto v2 = (h.k := c2);
    remove c2;
    printf("hash key value: %s\n", desc(v2));
    remove v2;

    hash<WeakHolder> wh();
    before = destroyed;
    printf("hashdecl key temp: %s destroyed=%d\n", desc(wh.o := new C()), destroyed - before);
    C c5();
    auto v6 = (wh.o := c5);
    remove c5;
    printf("hashdecl key value: %s\n", desc(v6));
    remove v6;

    # an opaque assignment through a path: its value owns its target, as the member does
    M om();
    C c6();
    auto v7 = (om.m @= c6);
    remove c6;
    remove om;
    printf("opaque member value: %s\n", desc(v7));
    M om2();
    hash<string, int> oth = {"a": 1};
    printf("opaque member strip: %y\n", (om2.h @= oth));

    list<auto!> l = (NOTHING,);
    before = destroyed;
    printf("list element temp: %s destroyed=%d\n", desc(l[0] := new C()), destroyed - before);
    C c3();
    auto v3 = (l[0] := c3);
    remove c3;
    printf("list element value: %s\n", desc(v3));
    remove v3;

    softlist<auto> sl;
    auto v4 = (sl := 1);
    printf("softlist value: %y %s sl=%y\n", v4, v4.fullType(), sl);
    remove v4;
    # native code kept the list of a plain local softlist variable after the statement (qore#7142)
    printf("softlist released: sl=%y\n", sl);
    softlist<auto> sl2;
    printf("softlist in the statement: %y\n", (sl2 := 2));
    printf("softlist after the statement: sl2=%y\n", sl2);
    softlist<auto> csl;
    code fsl = auto sub () { return csl; };
    auto v5 = (csl := 1);
    printf("closure softlist value: %y %s\n", v5, v5.fullType());
    remove v5;
    printf("closure softlist released: csl=%y\n", fsl());

    # the release of the member's previous value raises an exception: the object assigned is destroyed when its last
    # reference is released
    o.m = new Raise();
    before = destroyed;
    C c4();
    try {
        printf("member value: %s\n", (o.m := c4).n());
    } catch (hash<ExceptionInfo> ex) {
        printf("member store: %s\n", ex.err);
    }
    remove c4;
    printf("member store: destroyed=%d\n", destroyed - before);
}

values();
conversions();
paths();
store_errors();
temporaries();
printf("done destroyed=%d\n", destroyed);
