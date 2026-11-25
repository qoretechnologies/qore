#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# remove and delete of a member of an object reached through a weak reference remove the member of the object, in
# every mode (qore#7346); see ../weak-reference-object-member-remove.qtest

%modern
%allow-weak-references
%no-child-restrictions

class D {
    public {
        string n;
    }

    constructor(string n) {
        self.n = n;
    }

    destructor() {
        printf("destroy %s\n", n);
    }
}

class O {
    public {
        auto a = 1;
        auto b = 2;
        auto c = 3;
        int i = 4;
        *int oi = 5;
        auto d;
        auto l = (1, 2);
    }

    private {
        auto p = 6;
    }

    auto getP() {
        return p;
    }

    # removes a private member through a weak reference to the object itself
    auto removeOwnPrivate() {
        O w;
        w := self;
        return remove w.p;
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

class N {
    public {
        auto a = 1;
        auto b = 2;
        hash<auto> h = {"j": 1, "k": 2, "m": 3};
        list<auto> l = (1, 2, 3);
        *N w;
    }
}

class S {
    public {
        static O ws;
    }
}

our O gw;

sub err(string label, hash<ExceptionInfo> ex) {
    printf("%s: %s\n", label, ex.err);
}

sub test() {
    O o();
    O wo;
    wo := o;
    printf("remove wo.a: %y o.a %y\n", remove wo.a, o.a);
    printf("remove wo{\"b\"}: %y o.b %y\n", remove wo{"b"}, o.b);
    printf("remove wo.(c, a): %y o.c %y\n", remove wo.("c", "a"), o.c);
    try {
        printf("remove wo.i: %y\n", remove wo.i);
    } catch (hash<ExceptionInfo> ex) {
        err("remove wo.i", ex);
    }
    printf("o.i %y\n", o.i);
    printf("remove wo.oi: %y o.oi %y\n", remove wo.oi, o.oi);
    printf("remove wo.l[0]: %y o.l %y\n", remove wo.l[0], o.l);
    # an undeclared member of an object of a class with no member list is removed with its key
    U u();
    object ou = u;
    object wu;
    wu := u;
    ou.x = 7;
    ou.y = 8;
    printf("remove wu.x: %y keys %y\n", remove wu.x, keys ou);
    printf("remove wu.none: %y\n", remove wu.none);
    # members checked at run time through a weak reference of type object
    object wob;
    wob := o;
    try {
        remove wob.p;
    } catch (hash<ExceptionInfo> ex) {
        err("remove wob.p (private)", ex);
    }
    try {
        remove wob.none;
    } catch (hash<ExceptionInfo> ex) {
        err("remove wob.none (undeclared)", ex);
    }
    printf("o.p %y\n", o.getP());
    printf("removeOwnPrivate: %y o.p %y\n", o.removeOwnPrivate(), o.getP());

    # delete runs the destructor of the object in the member
    o.d = new D("d");
    delete wo.d;
    printf("after delete wo.d: o.d %y\n", o.d);
    o.d = new D("d2");
    delete wo{"d"};
    printf("after delete wo{d}: o.d %y\n", o.d);

    # weak references held elsewhere
    O o2();
    hash<auto!> h = {"w": NOTHING, "x": 1};
    h.w := o2;
    printf("remove h.w.a: %y o2.a %y\n", remove h.w.a, o2.a);
    code c = auto sub () { return remove wo.b; };
    o.b = 22;
    printf("closure remove wo.b: %y o.b %y\n", c(), o.b);
    gw := o2;
    printf("remove gw.b: %y o2.b %y\n", remove gw.b, o2.b);
    S::ws := o2;
    printf("remove S::ws.c: %y o2.c %y\n", remove S::ws.c, o2.c);

    # a class with a memberGate() method
    G g();
    G wg;
    wg := g;
    printf("remove wg.a: %y g.a %y\n", remove wg.a, g.a);

    # an object that was deleted
    O o3();
    O wo3;
    wo3 := o3;
    delete o3;
    try {
        printf("remove deleted: %y\n", remove wo3.a);
    } catch (hash<ExceptionInfo> ex) {
        err("remove deleted", ex);
    }
    try {
        delete wo3.a;
        printf("delete deleted: done\n");
    } catch (hash<ExceptionInfo> ex) {
        err("delete deleted", ex);
    }
}

# weak references held by a list element, a member and the object itself, and nested paths
sub holders() {
    N n();
    list<auto!> l = (NOTHING, 1);
    l[0] := n;
    printf("remove l[0].a: %y n.a %y\n", remove l[0].a, n.a);
    N n2();
    n2.w := n;
    printf("remove n2.w.b: %y n.b %y\n", remove n2.w.b, n.b);
    # a weak reference to the object itself, whose member is removed with the object's own lock
    N n3();
    n3.w := n3;
    printf("remove n3.w.a: %y n3.a %y\n", remove n3.w.a, n3.a);
    N wn;
    wn := n3;
    printf("remove wn.h.k: %y n3.h %y\n", remove wn.h.k, n3.h);
    printf("remove wn.h.(j): %y n3.h %y\n", remove wn.h.("j"), n3.h);
    printf("remove wn.l[0..1]: %y n3.l %y\n", remove wn.l[0..1], n3.l);
    remove n3.w;
}

test();
holders();
printf("end\n");
