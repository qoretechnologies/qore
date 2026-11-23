%modern
# remove-object-member.q: the remove and delete operators on object members (qore#7179); prints one line per check,
# which every execution mode and qcc must print identically

#! a class without declared members: every member is added at run time
class Dynamic {
    #! removes members of the object itself through an object variable, as a method does
    list<auto> removeOwn() {
        object me = self;
        me.own = 1;
        auto v = remove me.own;
        remove me.own_never;
        delete me.own_never_del;
        return (v, keys self);
    }
}

#! a class with declared members, which keep their slots
class Declared {
    public {
        auto m;
        *int i = 1;
        hash<auto> h = {"x": 1};
    }

    private {
        auto p = "p";
    }

    #! removes a private member inside the class
    list<auto> removePrivate() {
        auto v = remove p;
        return (v, p, keys self);
    }
}

#! a class with only private declared members: undeclared members can be added from outside the class
class PrivateOnly {
    private {
        auto p = 1;
    }

    list<string> ownKeys() {
        return keys self;
    }
}

#! counts its destructions
class Counted {
    public {
        static int destroyed = 0;
    }

    destructor() {
        ++destroyed;
    }
}

#! the views of an object's members: keys, elements, the iterators and the formatted object
string sub views(object o) {
    list<string> pairs = map sprintf("%s=%y", $1.key, $1.value), new ObjectPairIterator(o);
    list<string> each = ();
    foreach string k in (keys o) {
        each += k;
    }
    return sprintf("keys=%y elements=%d key-iterator=%y pairs=%y foreach=%y formatted=%y", keys o, elements o,
        (map $1, new ObjectKeyIterator(o)), pairs, each, o);
}

sub check(string label, auto v) {
    printf("%s: %y\n", label, v);
}

sub run() {
    # undeclared members
    object o = new Dynamic();
    check("remove never-set returns", remove o.never_set);
    check("remove never-set", views(o));
    delete o.never_del;
    check("delete never-set", views(o));
    o.a = 1;
    o.b = 2;
    check("remove set returns", remove o.a);
    check("remove set", views(o));
    check("removed member", o.a);
    check("removed member exists", exists o.a);
    delete o.b;
    check("delete set", views(o));
    o.c = 1;
    o.d = 2;
    o.e = 3;
    check("remove slice returns", remove o.("c", "x", "d"));
    check("remove slice", views(o));
    o.f = 4;
    delete o.("e", "y");
    check("delete slice", views(o));
    check("remove own members", cast<Dynamic>(o).removeOwn());
    check("own members", views(o));
    o.r = 5;
    reference<auto> r = \o.r;
    check("remove through a reference returns", remove r);
    check("remove through a reference", views(o));
    # a removed member can be added again; it is then the last key
    o.g = 6;
    o.f = 7;
    remove o.f;
    o.f = 8;
    check("added again", views(o));
    # a member that holds an object: delete destroys it, remove returns it
    o.obj = new Counted();
    delete o.obj;
    check("delete of an object member", (Counted::destroyed, views(o)));
    o.obj = new Counted();
    object kept = remove o.obj;
    check("remove of an object member", (Counted::destroyed, kept.className(), views(o)));
    delete kept;
    # a member that refers to the object itself (a cycle) is removed with its key
    o.myself = o;
    remove o.myself;
    check("remove of a cycle", views(o));

    # declared members keep their slots
    object d = new Declared();
    check("declared remove never-set returns", remove d.m);
    check("declared remove never-set", views(d));
    check("declared remove set returns", remove d.i);
    check("declared remove set", views(d));
    delete d.h;
    check("declared delete set", views(d));
    check("declared slice returns", remove d.("m", "i", "h"));
    check("declared slice", views(d));
    d.i = 2;
    check("declared set again", views(d));
    check("private remove", cast<Declared>(d).removePrivate());

    # undeclared members of a class with private members only
    object p = new PrivateOnly();
    p.u = 1;
    p.v = 2;
    check("undeclared beside private: remove returns", remove p.u);
    delete p.v;
    remove p.never;
    check("undeclared beside private", cast<PrivateOnly>(p).ownKeys());

    # the same operations on a hash, for comparison
    hash<auto> h = {"a": 1, "b": 2};
    remove h.never_set;
    remove h.a;
    delete h.b;
    check("hash", h);
}

run();
