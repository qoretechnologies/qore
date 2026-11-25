#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# += on an lvalue that holds a weak reference to an object changes the object as += on the object itself does, and the
# lvalue keeps the weak reference (qore#7403); see ../weak-reference-object-plus-assign.qtest

%modern
%allow-weak-references

class O {
    public {
        auto a = 1;
        auto b = 2;
        int n = 0;
        auto w;
        auto other;
        hash<auto> h = {};
        static int destroyed = 0;
    }

    destructor() {
        ++O::destroyed;
    }
}

# a class with no public member list: a merge adds undeclared members
class U {
    private {
        int p = 3;
    }

    int getP() {
        return p;
    }

    # += through a weak reference to the object itself, from the class: its private members are set
    mergeOwnPrivate() {
        auto sw;
        sw := self;
        sw += {"p": 30};
    }
}

class Src {
    public {
        auto a = 10;
        auto b = 12;
    }
}

class Holder {
    public {
        *O w;
        auto peer;
        auto strong;
        list<auto> l = ();
        hash<auto> h = {};
    }
}

class Peer {
    public {
        *Holder holder;
        auto x;
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

# whether an lvalue still holds a weak reference: it gives the object, which is destroyed when its last strong reference
# is released
sub weak_kept(string label, reference<auto> r, reference<auto> strong) {
    bool is_object = r.typeCode() == NT_OBJECT && r === strong;
    int destroyed = O::destroyed;
    remove strong;
    printf("%s: weak kept %y\n", label, is_object && O::destroyed == destroyed + 1);
}

sub test() {
    # auto lvalue, hash operand
    {
        O o();
        auto aw;
        aw := o;
        auto rv = (aw += {"a": 5, "b": 6});
        printf("auto += hash: o.a %y o.b %y rv is o %y aw is o %y\n", o.a, o.b, rv === o, aw === o);
        remove rv;
        weak_kept("auto += hash", \aw, \o);
    }
    # object and typed lvalues
    {
        O o();
        object ow;
        ow := o;
        ow += {"a": "x"};
        O tw;
        tw := o;
        tw += {"b": "y"};
        *O nw;
        nw := o;
        nw += {"n": 7};
        printf("typed += hash: o.a %y o.b %y o.n %y\n", o.a, o.b, o.n);
        weak_kept("typed += hash", \ow, \o);
    }
    # object operand: its members are merged
    {
        O o();
        Src s();
        auto aw;
        aw := o;
        aw += s;
        printf("+= object: o.a %y o.b %y s.a %y\n", o.a, o.b, s.a);
        weak_kept("+= object", \aw, \o);
    }
    # operands that change nothing: the object is the value, as through a strong reference
    {
        O o();
        auto aw;
        aw := o;
        auto r1 = (aw += 1);
        auto r2 = (aw += "str");
        auto r3 = (aw += NOTHING);
        auto r4 = (aw += (1, 2));
        auto r5 = (aw += {});
        printf("unchanged: o.a %y values %y\n", o.a, (r1 === o, r2 === o, r3 === o, r4 === o, r5 === o));
        auto so = o;
        auto s1 = (so += 1);
        printf("unchanged strong: %y\n", s1 === o);
        remove r1;
        remove r2;
        remove r3;
        remove r4;
        remove r5;
        remove s1;
        remove so;
        weak_kept("unchanged", \aw, \o);
    }
    # a typed member refuses a value of another type, as through a strong reference
    {
        O o();
        auto aw;
        aw := o;
        try {
            aw += {"n": "text"};
            printf("typed member: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            err("typed member weak", ex);
        }
        object so = o;
        try {
            so += {"n": "text"};
            printf("typed member strong: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            err("typed member strong", ex);
        }
        printf("typed member: o.n %y\n", o.n);
        remove so;
        weak_kept("typed member", \aw, \o);
    }
    # an undeclared member is added; a private member is refused outside the class and set inside it
    {
        U o();
        auto aw;
        aw := o;
        aw += {"undeclared": 1};
        auto ao = o;
        printf("undeclared: %y\n", ao.undeclared);
        try {
            aw += {"p": 4};
            printf("private: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            err("private", ex);
        }
        printf("private kept: %y\n", o.getP());
        o.mergeOwnPrivate();
        printf("own private: %y\n", o.getP());
    }
    # a weak reference to the object itself (the lvalue holds the object's own lock, qore#7107), merged into and
    # merged from
    {
        O o();
        o.w := o;
        o.w += {"a": 21};
        printf("self: o.a %y\n", o.a);
        O t();
        t.a = 22;
        t.w := o;
        t.w += t;
        printf("from the lvalue's object: o.a %y o.w is o %y\n", o.a, o.w === o);
        o.a = 23;
        o.other := t;
        o.other += o;
        printf("from the merged object's holder: t.a %y\n", t.a);
    }
    # weak references held by a member, a hash, a list, a closure and a global
    {
        O o();
        Holder h();
        h.w := o;
        h.w += {"a": 31};
        h.h.k := o;
        h.h.k += {"b": 32};
        h.l[0] := o;
        h.l[0] += {"n": 33};
        auto cw;
        cw := o;
        code c = sub () { cw += {"w": 34}; };
        c();
        gw := o;
        gw += {"other": 35};
        printf("holders: %y\n", (o.a, o.b, o.n, o.w, o.other));
        remove c;
        remove gw;
        weak_kept("holders", \cw, \o);
        printf("holders kept: %y\n", (h.w, h.h.k, h.l[0]));
    }
    # a deleted object that is still referenced: a merge raises OBJECT-ALREADY-DELETED, and an operand that changes
    # nothing gives the object, as through a strong reference
    {
        O o();
        object keep = o;
        auto aw;
        aw := o;
        delete o;
        try {
            aw += {"a": 1};
            printf("deleted: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            err("deleted weak", ex);
        }
        try {
            keep += {"a": 1};
            printf("deleted strong: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            err("deleted strong", ex);
        }
        auto rv = (aw += 1);
        printf("deleted += int: rv is keep %y\n", rv === keep);
    }
    # an object that its last reference released: a merge raises OBJECT-ALREADY-DELETED, any other operand gives
    # NOTHING, and the lvalue keeps the weak reference
    {
        auto dw;
        {
            O o();
            dw := o;
        }
        printf("released: %y\n", dw);
        try {
            dw += {"a": 1};
            printf("released += hash: no exception\n");
        } catch (hash<ExceptionInfo> ex) {
            err("released += hash", ex);
        }
        printf("released += int: %y dw %y\n", (dw += 1), dw);
    }
    # the same without a weak reference
    {
        O o();
        auto so = o;
        so += {"a": 41};
        printf("plain: o.a %y so is o %y\n", o.a, so === o);
    }
}

# the other compound operators replace the weak reference, as an assignment does and as they replace an object lvalue
# (the rule of qore#7297 for weak lists and hashes)
sub replacing() {
    O o();
    auto aw;
    aw := o;
    aw *= 2;
    auto so = o;
    so *= 2;
    printf("*=: weak %y strong %y o.a %y\n", aw, so, o.a);
    aw := o;
    aw |= 4;
    printf("|=: weak %y\n", aw);
}

# a merge into an object on a cycle with the start of the lvalue's path, through a strong and a weak reference
sub cycles() {
    {
        Holder h();
        {
            Peer p();
            h.peer = p;
            p.holder = h;
        }
        h.peer += {"holder": NOTHING};
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
        h.peer += {"holder": NOTHING, "x": h};
        printf("cycle weak: %y holder %y x is h %y\n", h.peer.name(), h.strong.holder, h.strong.x === h);
    }
}

# a member replaced by the merge whose destructor releases the last reference to the object: the object stays alive
# until the operator has its value, with the object's own lock released (qore#7107) and held by the lvalue
sub last_reference() {
    gl = new L();
    gl.a = new Releaser();
    gl.w := gl;
    gl.w += {"a": 1};
    printf("last reference own lock: done, gl %y\n", gl);
    gl = new L();
    gl.a = new Releaser();
    auto lw;
    lw := gl;
    lw += {"a": 1};
    printf("last reference: done, gl %y lw %y\n", gl, lw);
}

test();
replacing();
cycles();
last_reference();
printf("end\n");
