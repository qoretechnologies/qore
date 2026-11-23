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
