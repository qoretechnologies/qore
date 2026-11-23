#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# The object counts of the containers on an lvalue path: a container reached twice on the path through a weak
# reference (qore#7321), and an operation that raises after it changed a container (qore#7322).  A debug build checks
# the counts with assertions when a value is removed; the collection of the cycles shows that the scan saw the edges.
# The members are written through self: a write by the bare name of a member raised the object's own count (qore#7348),
# which hid the assertions.  A key holding a weak reference to its own hash is assigned through a weak local: an
# assignment through the path would make the hash unique again and leave the weak reference to the hash replaced.

%modern
%allow-weak-references

our int destroyed = 0;

class C {
    public {
        string name;
        hash<auto> holder;
        auto lst;
    }

    constructor(string n) {
        self.name = n;
    }

    destructor() {
        ++destroyed;
    }
}

hashdecl Decl {
    auto wk;
    *object k;
}

class D {
    public {
        string name;
        *hash<Decl> holder;
        list<object> lst = ();
        hash<string, object> ho = {};
    }

    constructor(string n) {
        self.name = n;
    }

    destructor() {
        ++destroyed;
    }
}

# qore#7321: the path is c, holder, holder (through the weak reference)
sub same_container_twice() {
    C c("c");
    c.holder = {};
    c.holder.k = c;
    auto w;
    w := c.holder;
    w.wk := c.holder;
    c.holder.wk.k = 1;
    remove c.holder;
    printf("same container twice: %y\n", c.holder);
}

# qore#7321: an object added through the weak step to the container already on the path is counted once: once it
# is removed, the hash holds no object, and taking it as an lvalue scans nothing
sub same_container_twice_add() {
    C c("c3");
    c.holder = {};
    auto w;
    w := c.holder;
    w.wk := c.holder;
    c.holder.wk.obj = new C("added");
    printf("added through the weak step: %y\n", c.holder.obj.name);
    remove c.holder.obj;
    # dbg_get_lvalue_scan_count() exists in debug builds only
    *code scan_count = get_lvalue_scan_count_func();
    int start = scan_count ? scan_count() : 0;
    c.holder += {};
    printf("scans after the object was removed: %d\n", scan_count ? scan_count() - start : 0);
    remove c.holder;
}

# qore#7321: the path goes back to an ancestor through a weak reference, then down again through a container already
# on it (c, holder, inner, holder through the weak reference, inner): each change is counted once by each holder
sub ancestor_twice_assign() {
    C c("c4");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.wk := c.holder;
    c.holder.inner.k = new C("x");
    c.holder.inner.wk.inner.k = 1;
    printf("replaced through a weak step to an ancestor: %y\n", c.holder.inner.k);
    remove c.holder;
}

sub ancestor_twice_remove() {
    C c("c5");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.wk := c.holder;
    c.holder.inner.k = new C("x");
    remove c.holder.inner.wk.inner.k;
    printf("removed through a weak step to an ancestor: %y\n", exists c.holder.inner.k);
    remove c.holder;
}

sub ancestor_twice_add() {
    C c("c6");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.wk := c.holder;
    c.holder.inner.wk.inner.k = new C("x");
    remove c.holder.inner.k;
    *code scan_count = get_lvalue_scan_count_func();
    int start = scan_count ? scan_count() : 0;
    c.holder += {};
    printf("scans after the object added through a weak step to an ancestor was removed: %d\n",
        scan_count ? scan_count() - start : 0);
    remove c.holder;
}

# qore#7321: the container on the path is itself replaced through a weak step to an ancestor (a removal through a
# weak step removes nothing yet: qore#7312)
sub ancestor_twice_container() {
    C c("c9");
    c.holder = {"inner": {}};
    auto w;
    w := c.holder.inner;
    w.wk := c.holder;
    c.holder.inner.k = new C("x");
    c.holder.inner.wk.inner = {};
    printf("container replaced through a weak step to an ancestor: %y\n", c.holder.inner);
    remove c.holder;
}

# qore#7321: two weak steps (holder, z, v, holder through w1, z, v2, v through w2, t): the edge from holder to z is on
# the path twice, and is written once the change of z is final (the form that adds an object needs qore#7351 in AST)
sub two_weak_steps_replace() {
    C c("c11");
    c.holder = {"z": {"v": {"t": {}}, "v2": {}}};
    c.holder.z.v.t.k = new C("x");
    auto w;
    w := c.holder.z.v;
    w.w1 := c.holder;
    auto w2;
    w2 := c.holder.z.v2;
    w2.w2 := c.holder.z.v;
    c.holder.z.v.w1.z.v2.w2.t.k = 1;
    *code scan_count = get_lvalue_scan_count_func();
    int start = scan_count ? scan_count() : 0;
    c.holder += {};
    printf("scans after the object was replaced through two weak steps: %d\n",
        scan_count ? scan_count() - start : 0);
    remove c.holder;
}

# qore#7321: holder, x (through wk), y, holder (through back), x, y, t: the only strong entry of the edge from holder to
# x comes before the change of x reaches it through the second entry of the edge from x to y
sub weak_step_down_and_back() {
    C c("c12");
    c.holder = {"x": {"y": {"t": {}}}};
    auto w;
    w := c.holder;
    w.wk := c.holder.x;
    auto wy;
    wy := c.holder.x.y;
    wy.back := c.holder;
    c.holder.wk.y.back.x.y.t.k = new C("o");
    remove c.holder.x.y.t.k;
    *code scan_count = get_lvalue_scan_count_func();
    int start = scan_count ? scan_count() : 0;
    c.holder += {};
    printf("scans after the object added down and back through weak steps was removed: %d\n",
        scan_count ? scan_count() - start : 0);
    remove c.holder;
}

# qore#7321 with a list: an element of the list is a weak reference to the list itself
sub same_list_twice() {
    C c("c7");
    c.lst = new list<auto>((c, NOTHING));
    auto w;
    w := c.lst;
    w[1] := c.lst;
    c.lst[1][0] = 1;
    printf("same list twice: %y\n", c.lst[0]);
    remove c.lst;
}

# qore#7321 with the key operator: c.holder{"wk"}{key}
sub same_container_twice_keys() {
    C c("c8");
    c.holder = {};
    c.holder.k = c;
    auto w;
    w := c.holder;
    w.wk := c.holder;
    string key = "k";
    c.holder{"wk"}{key} = 1;
    printf("same container twice with the key operator: %y\n", c.holder.k);
    remove c.holder;
}

*code sub get_lvalue_scan_count_func() {
    if (!exists_function("dbg_get_lvalue_scan_count")) {
        return;
    }
    return int sub () { return call_function("dbg_get_lvalue_scan_count"); };
}

# qore#7322: k is stored, then zz raises
sub failed_merge() {
    D d("owned");
    d.holder = <Decl>{};
    try {
        d.holder += {"k": new D("y"), "zz": 1};
    } catch (hash<ExceptionInfo> ex) {
        printf("failed merge: %s, k: %y\n", ex.err, exists d.holder.k);
    }
    remove d.holder;
}

# qore#7322: the failed merge stores the object itself, making a cycle, which is collected
sub failed_merge_cycle() {
    int start = destroyed;
    {
        D d("cycle");
        d.holder = <Decl>{};
        try {
            d.holder += {"k": d, "zz": 1};
        } catch (hash<ExceptionInfo> ex) {
            printf("failed merge making a cycle: %s, cycle: %y\n", ex.err, d.holder.k == d);
        }
    }
    printf("the cycle is collected: %y\n", destroyed - start);
}

# qore#7322: the merges into a list<object> and a hash<string, object> store an element, then refuse the next one
sub failed_typed_merges() {
    D d("typed");
    try {
        d.lst += (new D("l"), 1);
    } catch (hash<ExceptionInfo> ex) {
        printf("failed list merge: %s, size: %d\n", ex.err, d.lst.size());
    }
    try {
        d.ho += {"a": new D("h"), "b": 1};
    } catch (hash<ExceptionInfo> ex) {
        printf("failed hash<string, object> merge: %s, size: %d\n", ex.err, d.ho.size());
    }
    remove d.lst;
    remove d.ho;
}

# a merge that fails at its first key changes nothing
sub failed_merge_no_change() {
    D d("unchanged");
    d.holder = <Decl>{"k": new D("kept")};
    try {
        d.holder += {"zz": 1};
    } catch (hash<ExceptionInfo> ex) {
        printf("failed merge without a change: %s, k: %y\n", ex.err, d.holder.k.name);
    }
    remove d.holder;
}

const Cases = {
    "same_container_twice": \same_container_twice(),
    "same_container_twice_add": \same_container_twice_add(),
    "ancestor_twice_assign": \ancestor_twice_assign(),
    "ancestor_twice_remove": \ancestor_twice_remove(),
    "ancestor_twice_add": \ancestor_twice_add(),
    "ancestor_twice_container": \ancestor_twice_container(),
    "two_weak_steps_replace": \two_weak_steps_replace(),
    "weak_step_down_and_back": \weak_step_down_and_back(),
    "same_list_twice": \same_list_twice(),
    "same_container_twice_keys": \same_container_twice_keys(),
    "failed_merge": \failed_merge(),
    "failed_merge_cycle": \failed_merge_cycle(),
    "failed_typed_merges": \failed_typed_merges(),
    "failed_merge_no_change": \failed_merge_no_change(),
};

# a case named on the command line runs alone
*string only = ARGV[0];
foreach hash<auto> i in (Cases.pairIterator()) {
    if (!only || only == i.key) {
        call_function(i.value);
    }
}
printf("done\n");
