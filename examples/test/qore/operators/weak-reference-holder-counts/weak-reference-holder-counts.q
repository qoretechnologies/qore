#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# A change through a weak reference that adds an object to the list or hash that the weak reference designates keeps
# the object counts of the containers and objects that hold that container, which are not on the lvalue's path
# (qore#7318): a later removal does not subtract from a count of 0 (an assertion failure in a debug build), and a cycle
# made through such a change is collected.

%modern
%allow-weak-references

class C {
    public {
        string name;
        hash<auto> holder;
        auto ref;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        printf("destroyed %s\n", name);
    }
}

# an element write through the weak reference, then the removal of the list from its holder
sub list_element_then_remove() {
    C c("c1");
    c.holder = {"inner": new list<auto!>((NOTHING,))};
    auto w;
    w := c.holder.inner;
    w[1] = new C("x1");
    printf("list element: %y\n", c.holder.inner[1].name);
    remove c.holder.inner;
    printf("list element: removed: %y\n", c.holder.inner);
}

# a key write through the weak reference, then the removal of the hash from its holder
sub hash_key_then_remove() {
    C c("c2");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.o = new C("x2");
    printf("hash key: %y\n", c.holder.inner.o.name);
    remove c.holder.inner;
    printf("hash key: removed: %y\n", c.holder.inner);
}

# += on a list reached through the weak reference (qore#7297), then the removal
sub append_then_remove() {
    C c("c3");
    c.holder = {"inner": {"l": ()}};
    auto w;
    w := c.holder.inner;
    w.l += new C("x3");
    printf("append: %y\n", c.holder.inner.l[0].name);
    remove c.holder.inner;
    printf("append: removed: %y\n", c.holder.inner);
}

# a cycle made through the weak reference is collected when its root goes out of scope
sub cycle_through_weak_write() {
    {
        C c("c4");
        c.holder = {"inner": {}};
        auto w;
        w := c.holder.inner;
        C x("x4");
        x.ref = c;
        w.o = x;
    }
    printf("cycle: scope left\n");
}

# the object that a weak write stores is removed through the weak reference, then the holder is removed
sub weak_write_then_weak_remove() {
    C c("c5");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.o = new C("x5");
    remove w.o;
    printf("weak remove: %y\n", c.holder.inner);
    remove c.holder.inner;
    printf("weak remove: removed: %y\n", c.holder.inner);
}

const Cases = {
    "list_element_then_remove": \list_element_then_remove(),
    "hash_key_then_remove": \hash_key_then_remove(),
    "append_then_remove": \append_then_remove(),
    "cycle_through_weak_write": \cycle_through_weak_write(),
    "weak_write_then_weak_remove": \weak_write_then_weak_remove(),
};

# a case named on the command line runs alone
*string only = ARGV[0];
foreach hash<auto> i in (Cases.pairIterator()) {
    if (!only || only == i.key) {
        try {
            call_function(i.value);
        } catch (hash<ExceptionInfo> ex) {
            printf("%s: %s: %s\n", i.key, ex.err, ex.desc);
        }
    }
}
printf("done\n");
