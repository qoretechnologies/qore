#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# a reference element is bound while the assignment holds the lock of the referenced lvalue (qore#7380)

%modern
%allow-weak-references

enum BindEnum {
    BindEnumOne = 1,
}

class BaseValue {
}

class ChildValue inherits BaseValue {
}

class TypedHolder {
    public {
        auto b;
        list<reference<int>> i;
        list<reference<BaseValue>> o;
        list<reference<list<int>>> l;
        list<reference<date<absolute>>> d;
        list<reference<list<date<absolute>>>> ld;
        list<reference<softlist<object>>> sl;
        list<reference<object>> obj;
        list<reference<list<softint>>> lsi;
        list<reference<list>> plain_list;
        list<reference<*hash>> plain_hash;
    }
}

class HoldsReference {
    public {
        *reference<int> b;
        list<reference<*reference<int>>> a;
    }
}

class C {
    public {
        auto b = 1;
        list<reference<int>> a;
        list<reference<softint>> s;
    }
}

class Other {
    public {
        auto b = 2;
    }
}

class Notified {
    public {
        auto b = 1;
        list<reference<softint>> s;
        int notifications = 0;
    }

    memberNotification(string member) {
        # reads the object: this must not run under the lock of the assignment
        auto v = self.b;
        ++notifications;
    }
}

class OtherNotified {
    public {
        auto b = "3";
        int notifications = 0;
    }

    memberNotification(string member) {
        ++notifications;
    }
}

class Back {
    public {
        auto back;
    }
}

class ObjectHolder {
    public {
        auto b;
        list<reference<object>> a;
    }
}

class ChainInner {
    public {
        hash<auto> h;
        auto m;
        auto back;
    }
}

class ChainOuter {
    public {
        ChainInner o();
        list<reference<softint>> s;
        list<reference<list<object>>> l;
    }
}

hashdecl H {
    auto b;
    list<reference<softint>> s;
}

our hash<H> g_h = cast<hash<H>>({"b": 5});
our hash<H> g_conv = cast<hash<H>>({"b": "5"});
our auto g_anc;

sub show(string label, code c) {
    try {
        printf("%s: %y\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# a member of the same object: the value has the type
show("member of the same object", auto sub () {
    C c();
    list<auto> i = (\c.b,);
    auto x = i;
    c.a = x;
    c.a[0] = 3;
    return (c.a.fullType(), c.b);
});

# a value that would have to be converted cannot be written under the lock: the target and the value stay as they were
show("member that needs a conversion", auto sub () {
    C c();
    c.b = "7";
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.s = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.s, c.b.type(), c.b);
    }
    return "no exception";
});

# a value that the element type rejects leaves the target as it was
show("member rejected", auto sub () {
    C c();
    c.b = "x";
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.a = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.a, c.b);
    }
    return "no exception";
});

# several elements, one of which is converted
show("two elements", auto sub () {
    C c();
    c.b = 9;
    auto extra = "4";
    list<auto> i = (\c.b, \extra);
    auto x = i;
    c.s = x;
    return (c.b, extra);
});

# a member of another object takes that object's own lock
show("member of another object", auto sub () {
    C c();
    Other o();
    list<auto> i = (\o.b,);
    auto x = i;
    c.a = x;
    c.a[0] = 8;
    return o.b;
});

# the object can be used after the assignment: its lock was released once
show("object after the assignment", auto sub () {
    C c();
    c.b = 6;
    list<auto> i = (\c.b,);
    auto x = i;
    c.s = x;
    c.b = 11;
    return (c.s.size(), c.b);
});

# an element of the same global variable
show("global variable", auto sub () {
    list<auto> i = (\g_h.b,);
    auto x = i;
    g_h.s = x;
    return (g_h.b.type(), g_h.b);
});

show("global variable that needs a conversion", auto sub () {
    list<auto> i = (\g_conv.b,);
    auto x = i;
    try {
        g_conv.s = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, g_conv.s, g_conv.b.type(), g_conv.b);
    }
    return "no exception";
});

# the referenced lvalue contains the target: a conversion would replace the container that the assignment writes to
show("reference to an ancestor of the target", auto sub () {
    hash<string, *list<reference<hash<string, *list<auto>>>>> t = {"k": NOTHING};
    g_anc = t;
    list<auto> i = (\g_anc,);
    auto x = i;
    try {
        g_anc.k = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, g_anc.k, g_anc.fullType());
    }
    return g_anc.k.fullType();
});

# an element of the same closure-bound variable
show("closure-bound variable", auto sub () {
    hash<H> h = cast<hash<H>>({"b": 2});
    code f = auto sub () {
        list<auto> i = (\h.b,);
        auto x = i;
        h.s = x;
        return (h.b.type(), h.b);
    };
    return f();
});

# a referenced member holding an object: no scan of the object runs under the lock
show("member holding an object", auto sub () {
    ObjectHolder c();
    c.b = new Back();
    list<auto> i = (\c.b,);
    auto x = i;
    c.a = x;
    return c.a.fullType();
});

# the object is in a recursive set
show("member holding an object of a recursive set", auto sub () {
    ObjectHolder c();
    {
        Back d();
        c.b = d;
        d.back = c;
    }
    list<auto> i = (\c.b,);
    auto x = i;
    c.a = x;
    string rv = c.a.fullType();
    # breaks the cycle
    remove c.b.back;
    return rv;
});

# a path through another object from the object being assigned: a shared container stays as it is
show("path through another object with a shared container", auto sub () {
    ChainOuter c();
    c.o.h = {"k": "5"};
    hash<auto> saved = c.o.h;
    list<auto> i = (\c.o.h.k,);
    auto x = i;
    try {
        c.s = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.s, saved.k.type(), c.o.h.k.type());
    }
    return "no exception";
});

# the same path in a recursive set, with a value that would be copied: no scan runs under the lock
show("path through another object in a recursive set", auto sub () {
    ChainOuter c();
    ChainInner o = c.o;
    o.back = c;
    list<auto> objs = (c,);
    o.m = objs;
    list<auto> i = (\c.o.m,);
    auto x = i;
    string rv;
    try {
        c.l = x;
        rv = c.l.fullType();
    } catch (hash<ExceptionInfo> ex) {
        rv = ex.err;
    }
    # breaks the cycle
    remove o.back;
    remove o.m;
    return rv;
});

# the object refers to itself and is held only in a container: no reference is released under the lock
show("object in a committed recursive set", auto sub () {
    hash<auto> h = {"c": new ObjectHolder()};
    h.c.b = h.c;
    list<auto> i = (\h.c.b,);
    auto x = i;
    h.c.a = x;
    string rv = h.c.a.fullType();
    # breaks the cycle
    remove h.c.a;
    remove h.c.b;
    return rv;
});

# a subclass object needs no conversion
show("object of a subclass", auto sub () {
    TypedHolder c();
    c.b = new ChildValue();
    list<auto> i = (\c.b,);
    auto x = i;
    c.o = x;
    return c.o.fullType();
});

# an enum value would be converted to its base value
show("enum value", auto sub () {
    TypedHolder c();
    c.b = BindEnumOne;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.i = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.i);
    }
    return "no exception";
});

# a list whose element type is wider than the type of the reference, holding a value that the reference does not accept
show("list with a wider element type", auto sub () {
    TypedHolder c();
    list<*int> v = (1, NOTHING);
    c.b = v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.l = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.l);
    }
    return "no exception";
});

# a list of the type of the reference
show("list of the type", auto sub () {
    TypedHolder c();
    list<int> v = (1, 2);
    c.b = v;
    list<auto> i = (\c.b,);
    auto x = i;
    c.l = x;
    return c.l.fullType();
});

show("relative date for an absolute date", auto sub () {
    TypedHolder c();
    c.b = 1D;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.d = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.d);
    }
    return c.d.fullType();
});

show("list of relative dates for a list of absolute dates", auto sub () {
    TypedHolder c();
    list<date> v = (1D,);
    c.b = v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.ld = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.ld);
    }
    return c.ld.fullType();
});

show("object for a soft list", auto sub () {
    TypedHolder c();
    c.b = new BaseValue();
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.sl = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.sl);
    }
    return c.sl.fullType();
});

show("deleted object", auto sub () {
    TypedHolder c();
    BaseValue d();
    c.b = d;
    delete d;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.obj = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.obj);
    }
    return c.obj.fullType();
});

show("weak reference to a list", auto sub () {
    TypedHolder c();
    list<string> v = ("1",);
    c.b := v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.lsi = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.lsi);
    }
    return c.lsi.fullType();
});

show("weak reference to a plain list", auto sub () {
    TypedHolder c();
    list v = (1,);
    c.b := v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.plain_list = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.plain_list);
    }
    return c.plain_list.fullType();
});

show("weak reference to a plain hash", auto sub () {
    TypedHolder c();
    hash v = {"a": 1};
    c.b := v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.plain_hash = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.plain_hash);
    }
    return c.plain_hash.fullType();
});

# a reference held in the referenced lvalue is followed to its value, as in a normal bind
show("reference held in the referenced lvalue", auto sub () {
    HoldsReference c();
    int v = 1;
    c.b = \v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.a = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.a);
    }
    return c.a.fullType();
});

# a member notification of the same object cannot run under the lock: the assignment is refused and changes nothing
show("member notification of the same object", auto sub () {
    Notified n();
    n.b = 4;
    int before = n.notifications;
    list<auto> i = (\n.b,);
    auto x = i;
    try {
        n.s = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, n.s, n.b, n.notifications - before);
    }
    return "no exception";
});

# a member notification of another object runs as usual
show("member notification of another object", auto sub () {
    C c();
    OtherNotified o();
    list<auto> i = (\o.b,);
    auto x = i;
    c.s = x;
    return (o.b.type(), o.b, o.notifications > 0);
});
