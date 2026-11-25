#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# -= with a key or a list of keys on an lvalue that holds a weak reference to an object removes the members from the
# object, and the lvalue keeps the weak reference, in every mode (qore#7368); see
# ../weak-reference-object-minus-assign.qtest

%modern
%allow-weak-references
%no-child-restrictions

class O {
    public {
        auto a = 1;
        auto b = 2;
        auto c = 3;
        *O w;
    }

    private {
        auto p = 4;
    }

    auto getP() {
        return p;
    }

    # removes a private member through a weak reference to the object itself, whose lock the lvalue holds
    removeOwnPrivate() {
        w := self;
        w -= "p";
    }
}

class U {
}

class G {
    public {
        auto a = 1;
    }

    auto memberGate(string m) {
        return "gate " + m;
    }
}

class Holder {
    public {
        *O w;
        auto peer;
        auto strong;
    }
}

class Peer {
    public {
        *Holder holder;
    }

    string name() {
        return "peer";
    }
}

our object gw;

# an object whose destructor releases the last reference to the object holding it
class Releaser {
    destructor() {
        printf("Releaser destructor: releasing gl\n");
        remove gl;
    }
}

class L {
    public {
        auto a;
        auto w;
    }

    destructor() {
        printf("L destructor\n");
    }
}

our *L gl;

sub err(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s\n", label, ex.err);
}

sub test() {
    # the case of the issue: auto and object variables
    O o();
    auto aw;
    aw := o;
    aw -= "b";
    printf("auto -= b: o.b %y aw.a %y\n", o.b, aw.a);
    object w;
    w := o;
    w -= "c";
    printf("object -= c: o.c %y keys %y\n", o.c, keys o);
    # the weak reference is kept: a change of the object is seen through it
    o.a = 11;
    printf("still weak: w.a %y\n", w.a);

    # a list of keys, the value of the operator
    O o2();
    O w2;
    w2 := o2;
    auto rv = (w2 -= ("a", "b"));
    printf("-= list: o2 %y rv is o2 %y\n", (o2.a, o2.b, o2.c), rv == o2);

    # an undeclared member is removed with its key
    U u();
    object ou = u;
    object wu;
    wu := u;
    ou.x = 1;
    ou.y = 2;
    wu -= "x";
    printf("undeclared: keys %y\n", keys ou);

    # access checks
    O o3();
    object wo3;
    wo3 := o3;
    try {
        wo3 -= "p";
    } catch (hash<ExceptionInfo> ex) {
        err("private", ex);
    }
    printf("private kept: %y\n", o3.getP());
    o3.removeOwnPrivate();
    printf("own private removed: %y\n", o3.getP());

    # a class with a memberGate() method
    G g();
    G wg;
    wg := g;
    wg -= "a";
    printf("gate class: g.a %y\n", g.a);

    # weak references held by a member, a closure and a global
    O o4();
    Holder h();
    h.w := o4;
    h.w -= "a";
    printf("member: o4.a %y\n", o4.a);
    O w4;
    w4 := o4;
    code c = sub () { w4 -= "b"; };
    c();
    printf("closure: o4.b %y\n", o4.b);
    gw := o4;
    gw -= "c";
    printf("global: o4.c %y\n", o4.c);

    # a deleted object
    O o5();
    object w5;
    w5 := o5;
    delete o5;
    try {
        w5 -= "a";
        printf("deleted: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        err("deleted", ex);
    }

    # the same without a weak reference
    O o6();
    o6 -= "a";
    object ob = o6;
    ob -= ("b", "c");
    printf("plain: o6 %y\n", (o6.a, o6.b, o6.c));
}

# -= on a member that holds the object at the start of the lvalue's path (a cycle) deadlocked or failed an assertion,
# as the members were removed with a scan made while the lvalue held the lock of that object (qore#7401)
sub cycles() {
    {
        Holder h();
        {
            Peer p();
            h.peer = p;
            p.holder = h;
        }
        h.peer -= "holder";
        printf("cycle strong: %y holder %y\n", h.peer.name(), h.peer.holder);
    }
    {
        Holder h();
        {
            Peer p();
            h.strong = p;
            h.peer := p;
            p.holder = h;
        }
        h.peer -= "holder";
        printf("cycle weak: %y holder %y\n", h.peer.name(), h.strong.holder);
    }
}

# the value of the operator, and an object that its last reference released
sub values() {
    O o();
    auto aw;
    aw := o;
    auto rv = (aw -= NOTHING);
    printf("-= NOTHING: rv is o %y\n", rv == o);
    rv = (aw -= {"x": 1});
    printf("-= hash: rv is o %y\n", rv == o);
    auto dw;
    {
        O o2();
        dw := o2;
    }
    printf("released: %y\n", dw);
    printf("released -= hash: %y\n", (dw -= {}));
    try {
        dw -= "a";
        printf("released -= key: no exception\n");
    } catch (hash<ExceptionInfo> ex) {
        err("released -= key", ex);
    }
}

# the members are removed with the object's own lock released (qore#7107); removing a member whose destructor
# releases the last reference to the object leaves the object alive until the operator has its value
sub last_reference() {
    gl = new L();
    gl.a = new Releaser();
    gl.w := gl;
    gl.w -= "a";
    printf("last reference: done, gl %y\n", gl);
}

test();
cycles();
values();
last_reference();
printf("end\n");
