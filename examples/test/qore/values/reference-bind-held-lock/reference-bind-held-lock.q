#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.

# a reference element of a value stored into a locked lvalue is bound after the store has released the lock, also
# when the referenced lvalue is guarded by the same lock (qore#7380, qore#7391)

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
        list<string> notified = ();
    }

    memberNotification(string member) {
        # reads the object: this must not run under the lock of the assignment
        auto v = self.b;
        notified += member;
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

class LocalTarget {
    public {
        auto b = "6";
        list<reference<softint>> s;
    }
}

hashdecl H {
    auto b;
    list<reference<softint>> s;
}

our hash<H> g_h = cast<hash<H>>({"b": 5});
our hash<H> g_conv = cast<hash<H>>({"b": "5"});
our auto g_anc;
our hash<H> g_lref = cast<hash<H>>({"b": "9"});

# stores a value through a typed reference held by a local variable (a parameter)
sub store_typed_ref(reference<list<reference<softint>>> r, list<auto> v) {
    r = v;
}

# stores a value through a reference held by an untyped local variable (a parameter)
sub store_untyped_ref(auto r, list<auto> v) {
    r = v;
}

# stores a value whose element the referenced lvalue refuses through a reference held by a local variable
sub store_int_ref(reference<list<reference<int>>> r, list<auto> v) {
    r = v;
}

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

# a value that has to be converted is converted and written back after the lock has been released
show("member that needs a conversion", auto sub () {
    C c();
    c.b = "7";
    list<auto> i = (\c.b,);
    auto x = i;
    c.s = x;
    return (c.s.fullType(), c.b.type(), c.b);
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
    g_conv.s = x;
    return (g_conv.s.fullType(), g_conv.b.type(), g_conv.b);
});

# the referenced lvalue contains the target: the bind converts the container that the assignment writes to, and the
# store resolves its target again in the converted container
show("reference to an ancestor of the target", auto sub () {
    hash<string, *list<reference<hash<string, *list<auto>>>>> t = {"k": NOTHING};
    g_anc = t;
    list<auto> i = (\g_anc,);
    auto x = i;
    g_anc.k = x;
    return (g_anc.k.fullType(), g_anc.fullType());
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

# a path through another object from the object being assigned: the converted value is written to a copy of the
# shared container, which stays as it is
show("path through another object with a shared container", auto sub () {
    ChainOuter c();
    c.o.h = {"k": "5"};
    hash<auto> saved = c.o.h;
    list<auto> i = (\c.o.h.k,);
    auto x = i;
    c.s = x;
    return (c.s.fullType(), saved.k.type(), saved.k, c.o.h.k.type(), c.o.h.k);
});

# the same path in a recursive set, with a value that is copied to be converted
show("path through another object in a recursive set", auto sub () {
    ChainOuter c();
    ChainInner o = c.o;
    o.back = c;
    list<auto> objs = (c,);
    o.m = objs;
    list<auto> i = (\c.o.m,);
    auto x = i;
    list<auto> rv;
    try {
        c.l = x;
        rv = (c.l.fullType(), o.m.fullType(), o.m[0] == c);
    } catch (hash<ExceptionInfo> ex) {
        rv = (ex.err,);
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

# an enum value is converted to its base value
show("enum value", auto sub () {
    TypedHolder c();
    c.b = BindEnumOne;
    list<auto> i = (\c.b,);
    auto x = i;
    c.i = x;
    return (c.i.fullType(), c.b.type(), c.b);
});

# a list whose element type is wider than the type of the reference, holding a value that the reference does not accept:
# the target and the referenced value stay as they were
show("list with a wider element type", auto sub () {
    TypedHolder c();
    list<*int> v = (1, NOTHING);
    c.b = v;
    list<auto> i = (\c.b,);
    auto x = i;
    try {
        c.l = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.l, c.b.fullType(), c.b);
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
        return (ex.err, c.d, c.b.isRelative());
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
        return (ex.err, c.ld, c.b.fullType(), c.b[0].isRelative());
    }
    return c.ld.fullType();
});

show("object for a soft list", auto sub () {
    TypedHolder c();
    c.b = new BaseValue();
    list<auto> i = (\c.b,);
    auto x = i;
    c.sl = x;
    # the object is converted to a list holding it
    return (c.sl.fullType(), c.b.fullType(), c.b[0] instanceof BaseValue);
});

show("deleted object", auto sub () {
    TypedHolder c();
    BaseValue d();
    c.b = d;
    delete d;
    list<auto> i = (\c.b,);
    auto x = i;
    # a deleted object is no value, which the object type does not accept
    try {
        c.obj = x;
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.obj, type(c.b), c.b instanceof BaseValue);
    }
    return c.obj.fullType();
});

show("weak reference to a list", auto sub () {
    TypedHolder c();
    list<string> v = ("1",);
    c.b := v;
    list<auto> i = (\c.b,);
    auto x = i;
    c.lsi = x;
    # the list is converted to a new list, which replaces the weak reference; the list it refers to stays as it is
    return (c.lsi.fullType(), c.b.fullType(), c.b, v.fullType(), v);
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
        return (ex.err, c.plain_list, c.b[0]);
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
        return (ex.err, c.plain_hash, c.b.a);
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

# the member notifications of the same object run after the lock has been released: one for the member written back by
# the bind and one for the target; the round of the store that was not made adds none ("b" is first notified by the
# assignment of 4)
show("member notification of the same object", auto sub () {
    Notified n();
    n.b = 4;
    list<auto> i = (\n.b,);
    auto x = i;
    n.s = x;
    return (n.s.fullType(), n.b.type(), n.b, n.notified);
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

# a local variable that holds a reference to a member of an object stores a value whose element refers to another
# member of the same object (qore#7391)
show("local reference to a member of the same object", auto sub () {
    LocalTarget t();
    list<auto> v = (\t.b,);
    store_typed_ref(\t.s, v);
    return (t.s.fullType(), t.b.type(), t.b);
});

show("untyped local reference to a member of the same object", auto sub () {
    LocalTarget t();
    list<auto> v = (\t.b,);
    store_untyped_ref(\t.s, v);
    return (t.s.fullType(), t.b.type(), t.b);
});

show("local reference to a member of another object", auto sub () {
    LocalTarget t();
    Other o();
    o.b = "3";
    list<auto> v = (\o.b,);
    store_typed_ref(\t.s, v);
    return (t.s.fullType(), o.b.type(), o.b, t.b);
});

show("local reference to a global variable", auto sub () {
    list<auto> v = (\g_lref.b,);
    store_typed_ref(\g_lref.s, v);
    return (g_lref.s.fullType(), g_lref.b.type(), g_lref.b);
});

show("local reference to a closure-bound variable", auto sub () {
    hash<H> h = cast<hash<H>>({"b": "4"});
    code get = auto sub () { return h.b; };
    list<auto> v = (\h.b,);
    store_typed_ref(\h.s, v);
    return (h.s.fullType(), h.b.type(), get());
});

# the referenced value is refused: the target and the referenced value stay as they were
show("local reference with a refused element", auto sub () {
    C c();
    c.b = "x";
    list<auto> v = (\c.b,);
    try {
        store_int_ref(\c.a, v);
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.a, c.b);
    }
    return "no exception";
});

# the variable of a foreach loop is given a reference by the first element; the second element is assigned through it
show("foreach variable holding a reference", auto sub () {
    LocalTarget t();
    t.b = "5";
    list<auto> elems = (\t.s, (\t.b,));
    int n = 0;
    foreach auto e in (elems) {
        ++n;
    }
    return (n, t.s.fullType(), t.b.type(), t.b);
});

# a cast binds the reference elements of the list it converts: the referenced value is converted (qore#7377, qore#7391)
show("cast to a list of references to int", auto sub () {
    C c();
    c.b = BindEnumOne;
    list<auto> x = (\c.b,);
    auto l = cast<list<reference<int>>>(x);
    return (l.fullType(), c.b.type(), c.b);
});

show("cast to a list of references to int refused", auto sub () {
    C c();
    c.b = "x";
    list<auto> x = (\c.b,);
    try {
        auto l = cast<list<reference<int>>>(x);
        return l.fullType();
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.b);
    }
});

show("nested cast to references to softint", auto sub () {
    C c();
    c.b = "7";
    list<auto> inner = (\c.b,);
    list<list<auto>> nested = (inner,);
    auto l = cast<list<list<reference<softint>>>>(nested);
    return (l.fullType(), c.b.type(), c.b);
});

show("nested cast to references to int refused", auto sub () {
    C c();
    c.b = "x";
    list<auto> inner = (\c.b,);
    list<list<auto>> nested = (inner,);
    try {
        auto l = cast<list<list<reference<int>>>>(nested);
        return l.fullType();
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.b);
    }
});

# a reference element given to a soft list element type is bound before it is wrapped in a list
show("soft list element", auto sub () {
    C c();
    c.b = "8";
    list<auto> outer = (\c.b,);
    auto l = cast<list<softlist<reference<softint>>>>(outer);
    return (l.fullType(), l[0].fullType(), c.b.type(), c.b);
});

show("soft list element refused", auto sub () {
    C c();
    c.b = "x";
    list<auto> outer = (\c.b,);
    try {
        auto l = cast<list<softlist<reference<int>>>>(outer);
        return l.fullType();
    } catch (hash<ExceptionInfo> ex) {
        return (ex.err, c.b);
    }
});
