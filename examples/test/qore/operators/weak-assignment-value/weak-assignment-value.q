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
    # a container that the variable's type converts to a copy of its own: the copy is a temporary of the statement,
    # and the value of the assignment (qore#7085)
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

values();
conversions();
store_errors();
temporaries();
printf("done destroyed=%d\n", destroyed);
