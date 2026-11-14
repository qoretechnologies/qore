#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the operators of select-constant-predicate.qtest: each constant predicate is written literally in a function of its
# own and in the top-level code (qore#6071), with the same operators for each constant

%modern

const L = (1, 2, 3);

sub show(string scope, string pred, hash<auto> h) {
    foreach hash<auto> i in (h.pairIterator()) {
        printf("%s %s %s: %y\n", scope, pred, i.key, i.value);
    }
}

# predicate: NOTHING
hash<auto> sub t_nothing() {
    hash<auto> h = {};
    h."select" = (select L, NOTHING);
    h."fold select" = (foldl $1 + $2, (select L, NOTHING));
    h."select map" = (select (map $1 * 2, L), NOTHING);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), NOTHING));
    h."map select" = (map $1 * 2, (select L, NOTHING));
    h."map-select" = (map $1 * 2, L, NOTHING);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, NOTHING));
    h."hash map-select" = (map {$1: $1 * 2}, L, NOTHING);
    h."select iterator" = (select new ListIterator(L), NOTHING);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), NOTHING));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), NOTHING);
    h."first select" = (first (select L, NOTHING));
    h."any select" = (any (select L, NOTHING));
    h."count select" = (count (select L, NOTHING));
    h."first select map" = (first (select (map $1 * 2, L), NOTHING));
    h."first pred" = (first (NOTHING), L);
    h."first pred select" = (first (NOTHING), (select L, True));
    h."any pred" = (any (NOTHING), L);
    h."all pred" = (all (NOTHING), L);
    h."count pred" = (count (NOTHING), L);
    h."count pred map" = (count (NOTHING), (map $1 * 2, L));
    h."takewhile" = (takewhile (NOTHING), L);
    h."takeuntil" = (takeuntil (NOTHING), L);
    h."takewhile map" = (takewhile (NOTHING), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (NOTHING), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (NOTHING), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (NOTHING), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (NOTHING), L));
    h."takewhile iterator" = (takewhile (NOTHING), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, NOTHING) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), NOTHING) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, NOTHING) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (NOTHING), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (NOTHING), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: 0
hash<auto> sub t_zero() {
    hash<auto> h = {};
    h."select" = (select L, 0);
    h."fold select" = (foldl $1 + $2, (select L, 0));
    h."select map" = (select (map $1 * 2, L), 0);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), 0));
    h."map select" = (map $1 * 2, (select L, 0));
    h."map-select" = (map $1 * 2, L, 0);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, 0));
    h."hash map-select" = (map {$1: $1 * 2}, L, 0);
    h."select iterator" = (select new ListIterator(L), 0);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), 0));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), 0);
    h."first select" = (first (select L, 0));
    h."any select" = (any (select L, 0));
    h."count select" = (count (select L, 0));
    h."first select map" = (first (select (map $1 * 2, L), 0));
    h."first pred" = (first (0), L);
    h."first pred select" = (first (0), (select L, True));
    h."any pred" = (any (0), L);
    h."all pred" = (all (0), L);
    h."count pred" = (count (0), L);
    h."count pred map" = (count (0), (map $1 * 2, L));
    h."takewhile" = (takewhile (0), L);
    h."takeuntil" = (takeuntil (0), L);
    h."takewhile map" = (takewhile (0), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (0), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (0), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (0), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (0), L));
    h."takewhile iterator" = (takewhile (0), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, 0) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), 0) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, 0) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (0), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (0), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: ""
hash<auto> sub t_empty_string() {
    hash<auto> h = {};
    h."select" = (select L, "");
    h."fold select" = (foldl $1 + $2, (select L, ""));
    h."select map" = (select (map $1 * 2, L), "");
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), ""));
    h."map select" = (map $1 * 2, (select L, ""));
    h."map-select" = (map $1 * 2, L, "");
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, ""));
    h."hash map-select" = (map {$1: $1 * 2}, L, "");
    h."select iterator" = (select new ListIterator(L), "");
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), ""));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), "");
    h."first select" = (first (select L, ""));
    h."any select" = (any (select L, ""));
    h."count select" = (count (select L, ""));
    h."first select map" = (first (select (map $1 * 2, L), ""));
    h."first pred" = (first (""), L);
    h."first pred select" = (first (""), (select L, True));
    h."any pred" = (any (""), L);
    h."all pred" = (all (""), L);
    h."count pred" = (count (""), L);
    h."count pred map" = (count (""), (map $1 * 2, L));
    h."takewhile" = (takewhile (""), L);
    h."takeuntil" = (takeuntil (""), L);
    h."takewhile map" = (takewhile (""), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (""), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (""), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (""), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (""), L));
    h."takewhile iterator" = (takewhile (""), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, "") {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), "") {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, "") {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (""), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (""), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: ()
hash<auto> sub t_empty_list() {
    hash<auto> h = {};
    h."select" = (select L, ());
    h."fold select" = (foldl $1 + $2, (select L, ()));
    h."select map" = (select (map $1 * 2, L), ());
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), ()));
    h."map select" = (map $1 * 2, (select L, ()));
    h."map-select" = (map $1 * 2, L, ());
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, ()));
    h."hash map-select" = (map {$1: $1 * 2}, L, ());
    h."select iterator" = (select new ListIterator(L), ());
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), ()));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), ());
    h."first select" = (first (select L, ()));
    h."any select" = (any (select L, ()));
    h."count select" = (count (select L, ()));
    h."first select map" = (first (select (map $1 * 2, L), ()));
    h."first pred" = (first (()), L);
    h."first pred select" = (first (()), (select L, True));
    h."any pred" = (any (()), L);
    h."all pred" = (all (()), L);
    h."count pred" = (count (()), L);
    h."count pred map" = (count (()), (map $1 * 2, L));
    h."takewhile" = (takewhile (()), L);
    h."takeuntil" = (takeuntil (()), L);
    h."takewhile map" = (takewhile (()), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (()), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (()), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (()), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (()), L));
    h."takewhile iterator" = (takewhile (()), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, ()) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), ()) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, ()) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (()), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (()), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: {}
hash<auto> sub t_empty_hash() {
    hash<auto> h = {};
    h."select" = (select L, {});
    h."fold select" = (foldl $1 + $2, (select L, {}));
    h."select map" = (select (map $1 * 2, L), {});
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), {}));
    h."map select" = (map $1 * 2, (select L, {}));
    h."map-select" = (map $1 * 2, L, {});
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, {}));
    h."hash map-select" = (map {$1: $1 * 2}, L, {});
    h."select iterator" = (select new ListIterator(L), {});
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), {}));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), {});
    h."first select" = (first (select L, {}));
    h."any select" = (any (select L, {}));
    h."count select" = (count (select L, {}));
    h."first select map" = (first (select (map $1 * 2, L), {}));
    h."first pred" = (first ({}), L);
    h."first pred select" = (first ({}), (select L, True));
    h."any pred" = (any ({}), L);
    h."all pred" = (all ({}), L);
    h."count pred" = (count ({}), L);
    h."count pred map" = (count ({}), (map $1 * 2, L));
    h."takewhile" = (takewhile ({}), L);
    h."takeuntil" = (takeuntil ({}), L);
    h."takewhile map" = (takewhile ({}), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile ({}), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile ({}), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil ({}), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil ({}), L));
    h."takewhile iterator" = (takewhile ({}), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, {}) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), {}) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, {}) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile ({}), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil ({}), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: False
hash<auto> sub t_false() {
    hash<auto> h = {};
    h."select" = (select L, False);
    h."fold select" = (foldl $1 + $2, (select L, False));
    h."select map" = (select (map $1 * 2, L), False);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), False));
    h."map select" = (map $1 * 2, (select L, False));
    h."map-select" = (map $1 * 2, L, False);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, False));
    h."hash map-select" = (map {$1: $1 * 2}, L, False);
    h."select iterator" = (select new ListIterator(L), False);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), False));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), False);
    h."first select" = (first (select L, False));
    h."any select" = (any (select L, False));
    h."count select" = (count (select L, False));
    h."first select map" = (first (select (map $1 * 2, L), False));
    h."first pred" = (first (False), L);
    h."first pred select" = (first (False), (select L, True));
    h."any pred" = (any (False), L);
    h."all pred" = (all (False), L);
    h."count pred" = (count (False), L);
    h."count pred map" = (count (False), (map $1 * 2, L));
    h."takewhile" = (takewhile (False), L);
    h."takeuntil" = (takeuntil (False), L);
    h."takewhile map" = (takewhile (False), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (False), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (False), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (False), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (False), L));
    h."takewhile iterator" = (takewhile (False), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, False) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), False) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, False) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (False), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (False), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: True
hash<auto> sub t_true() {
    hash<auto> h = {};
    h."select" = (select L, True);
    h."fold select" = (foldl $1 + $2, (select L, True));
    h."select map" = (select (map $1 * 2, L), True);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), True));
    h."map select" = (map $1 * 2, (select L, True));
    h."map-select" = (map $1 * 2, L, True);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, True));
    h."hash map-select" = (map {$1: $1 * 2}, L, True);
    h."select iterator" = (select new ListIterator(L), True);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), True));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), True);
    h."first select" = (first (select L, True));
    h."any select" = (any (select L, True));
    h."count select" = (count (select L, True));
    h."first select map" = (first (select (map $1 * 2, L), True));
    h."first pred" = (first (True), L);
    h."first pred select" = (first (True), (select L, True));
    h."any pred" = (any (True), L);
    h."all pred" = (all (True), L);
    h."count pred" = (count (True), L);
    h."count pred map" = (count (True), (map $1 * 2, L));
    h."takewhile" = (takewhile (True), L);
    h."takeuntil" = (takeuntil (True), L);
    h."takewhile map" = (takewhile (True), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (True), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (True), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (True), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (True), L));
    h."takewhile iterator" = (takewhile (True), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, True) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), True) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, True) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (True), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (True), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: 1
hash<auto> sub t_one() {
    hash<auto> h = {};
    h."select" = (select L, 1);
    h."fold select" = (foldl $1 + $2, (select L, 1));
    h."select map" = (select (map $1 * 2, L), 1);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), 1));
    h."map select" = (map $1 * 2, (select L, 1));
    h."map-select" = (map $1 * 2, L, 1);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, 1));
    h."hash map-select" = (map {$1: $1 * 2}, L, 1);
    h."select iterator" = (select new ListIterator(L), 1);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), 1));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), 1);
    h."first select" = (first (select L, 1));
    h."any select" = (any (select L, 1));
    h."count select" = (count (select L, 1));
    h."first select map" = (first (select (map $1 * 2, L), 1));
    h."first pred" = (first (1), L);
    h."first pred select" = (first (1), (select L, True));
    h."any pred" = (any (1), L);
    h."all pred" = (all (1), L);
    h."count pred" = (count (1), L);
    h."count pred map" = (count (1), (map $1 * 2, L));
    h."takewhile" = (takewhile (1), L);
    h."takeuntil" = (takeuntil (1), L);
    h."takewhile map" = (takewhile (1), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (1), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (1), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (1), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (1), L));
    h."takewhile iterator" = (takewhile (1), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, 1) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), 1) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, 1) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (1), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (1), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# predicate: "x"
hash<auto> sub t_string() {
    hash<auto> h = {};
    h."select" = (select L, "x");
    h."fold select" = (foldl $1 + $2, (select L, "x"));
    h."select map" = (select (map $1 * 2, L), "x");
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), "x"));
    h."map select" = (map $1 * 2, (select L, "x"));
    h."map-select" = (map $1 * 2, L, "x");
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, "x"));
    h."hash map-select" = (map {$1: $1 * 2}, L, "x");
    h."select iterator" = (select new ListIterator(L), "x");
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), "x"));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), "x");
    h."first select" = (first (select L, "x"));
    h."any select" = (any (select L, "x"));
    h."count select" = (count (select L, "x"));
    h."first select map" = (first (select (map $1 * 2, L), "x"));
    h."first pred" = (first ("x"), L);
    h."first pred select" = (first ("x"), (select L, True));
    h."any pred" = (any ("x"), L);
    h."all pred" = (all ("x"), L);
    h."count pred" = (count ("x"), L);
    h."count pred map" = (count ("x"), (map $1 * 2, L));
    h."takewhile" = (takewhile ("x"), L);
    h."takeuntil" = (takeuntil ("x"), L);
    h."takewhile map" = (takewhile ("x"), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile ("x"), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile ("x"), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil ("x"), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil ("x"), L));
    h."takewhile iterator" = (takewhile ("x"), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, "x") {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), "x") {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, "x") {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile ("x"), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil ("x"), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    return h;
}

# each function runs several times, so that the tiered execution mode compiles it
for (int i = 0; i < 3; ++i) {
    show("function", "nothing", t_nothing());
    show("function", "zero", t_zero());
    show("function", "empty-string", t_empty_string());
    show("function", "empty-list", t_empty_list());
    show("function", "empty-hash", t_empty_hash());
    show("function", "false", t_false());
    show("function", "true", t_true());
    show("function", "one", t_one());
    show("function", "string", t_string());
}

# the same operators in the top-level code
{
    hash<auto> h = {};
    h."select" = (select L, NOTHING);
    h."fold select" = (foldl $1 + $2, (select L, NOTHING));
    h."select map" = (select (map $1 * 2, L), NOTHING);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), NOTHING));
    h."map select" = (map $1 * 2, (select L, NOTHING));
    h."map-select" = (map $1 * 2, L, NOTHING);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, NOTHING));
    h."hash map-select" = (map {$1: $1 * 2}, L, NOTHING);
    h."select iterator" = (select new ListIterator(L), NOTHING);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), NOTHING));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), NOTHING);
    h."first select" = (first (select L, NOTHING));
    h."any select" = (any (select L, NOTHING));
    h."count select" = (count (select L, NOTHING));
    h."first select map" = (first (select (map $1 * 2, L), NOTHING));
    h."first pred" = (first (NOTHING), L);
    h."first pred select" = (first (NOTHING), (select L, True));
    h."any pred" = (any (NOTHING), L);
    h."all pred" = (all (NOTHING), L);
    h."count pred" = (count (NOTHING), L);
    h."count pred map" = (count (NOTHING), (map $1 * 2, L));
    h."takewhile" = (takewhile (NOTHING), L);
    h."takeuntil" = (takeuntil (NOTHING), L);
    h."takewhile map" = (takewhile (NOTHING), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (NOTHING), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (NOTHING), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (NOTHING), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (NOTHING), L));
    h."takewhile iterator" = (takewhile (NOTHING), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, NOTHING) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), NOTHING) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, NOTHING) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (NOTHING), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (NOTHING), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "nothing", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, 0);
    h."fold select" = (foldl $1 + $2, (select L, 0));
    h."select map" = (select (map $1 * 2, L), 0);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), 0));
    h."map select" = (map $1 * 2, (select L, 0));
    h."map-select" = (map $1 * 2, L, 0);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, 0));
    h."hash map-select" = (map {$1: $1 * 2}, L, 0);
    h."select iterator" = (select new ListIterator(L), 0);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), 0));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), 0);
    h."first select" = (first (select L, 0));
    h."any select" = (any (select L, 0));
    h."count select" = (count (select L, 0));
    h."first select map" = (first (select (map $1 * 2, L), 0));
    h."first pred" = (first (0), L);
    h."first pred select" = (first (0), (select L, True));
    h."any pred" = (any (0), L);
    h."all pred" = (all (0), L);
    h."count pred" = (count (0), L);
    h."count pred map" = (count (0), (map $1 * 2, L));
    h."takewhile" = (takewhile (0), L);
    h."takeuntil" = (takeuntil (0), L);
    h."takewhile map" = (takewhile (0), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (0), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (0), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (0), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (0), L));
    h."takewhile iterator" = (takewhile (0), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, 0) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), 0) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, 0) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (0), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (0), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "zero", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, "");
    h."fold select" = (foldl $1 + $2, (select L, ""));
    h."select map" = (select (map $1 * 2, L), "");
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), ""));
    h."map select" = (map $1 * 2, (select L, ""));
    h."map-select" = (map $1 * 2, L, "");
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, ""));
    h."hash map-select" = (map {$1: $1 * 2}, L, "");
    h."select iterator" = (select new ListIterator(L), "");
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), ""));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), "");
    h."first select" = (first (select L, ""));
    h."any select" = (any (select L, ""));
    h."count select" = (count (select L, ""));
    h."first select map" = (first (select (map $1 * 2, L), ""));
    h."first pred" = (first (""), L);
    h."first pred select" = (first (""), (select L, True));
    h."any pred" = (any (""), L);
    h."all pred" = (all (""), L);
    h."count pred" = (count (""), L);
    h."count pred map" = (count (""), (map $1 * 2, L));
    h."takewhile" = (takewhile (""), L);
    h."takeuntil" = (takeuntil (""), L);
    h."takewhile map" = (takewhile (""), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (""), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (""), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (""), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (""), L));
    h."takewhile iterator" = (takewhile (""), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, "") {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), "") {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, "") {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (""), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (""), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "empty-string", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, ());
    h."fold select" = (foldl $1 + $2, (select L, ()));
    h."select map" = (select (map $1 * 2, L), ());
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), ()));
    h."map select" = (map $1 * 2, (select L, ()));
    h."map-select" = (map $1 * 2, L, ());
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, ()));
    h."hash map-select" = (map {$1: $1 * 2}, L, ());
    h."select iterator" = (select new ListIterator(L), ());
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), ()));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), ());
    h."first select" = (first (select L, ()));
    h."any select" = (any (select L, ()));
    h."count select" = (count (select L, ()));
    h."first select map" = (first (select (map $1 * 2, L), ()));
    h."first pred" = (first (()), L);
    h."first pred select" = (first (()), (select L, True));
    h."any pred" = (any (()), L);
    h."all pred" = (all (()), L);
    h."count pred" = (count (()), L);
    h."count pred map" = (count (()), (map $1 * 2, L));
    h."takewhile" = (takewhile (()), L);
    h."takeuntil" = (takeuntil (()), L);
    h."takewhile map" = (takewhile (()), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (()), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (()), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (()), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (()), L));
    h."takewhile iterator" = (takewhile (()), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, ()) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), ()) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, ()) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (()), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (()), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "empty-list", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, {});
    h."fold select" = (foldl $1 + $2, (select L, {}));
    h."select map" = (select (map $1 * 2, L), {});
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), {}));
    h."map select" = (map $1 * 2, (select L, {}));
    h."map-select" = (map $1 * 2, L, {});
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, {}));
    h."hash map-select" = (map {$1: $1 * 2}, L, {});
    h."select iterator" = (select new ListIterator(L), {});
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), {}));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), {});
    h."first select" = (first (select L, {}));
    h."any select" = (any (select L, {}));
    h."count select" = (count (select L, {}));
    h."first select map" = (first (select (map $1 * 2, L), {}));
    h."first pred" = (first ({}), L);
    h."first pred select" = (first ({}), (select L, True));
    h."any pred" = (any ({}), L);
    h."all pred" = (all ({}), L);
    h."count pred" = (count ({}), L);
    h."count pred map" = (count ({}), (map $1 * 2, L));
    h."takewhile" = (takewhile ({}), L);
    h."takeuntil" = (takeuntil ({}), L);
    h."takewhile map" = (takewhile ({}), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile ({}), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile ({}), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil ({}), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil ({}), L));
    h."takewhile iterator" = (takewhile ({}), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, {}) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), {}) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, {}) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile ({}), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil ({}), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "empty-hash", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, False);
    h."fold select" = (foldl $1 + $2, (select L, False));
    h."select map" = (select (map $1 * 2, L), False);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), False));
    h."map select" = (map $1 * 2, (select L, False));
    h."map-select" = (map $1 * 2, L, False);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, False));
    h."hash map-select" = (map {$1: $1 * 2}, L, False);
    h."select iterator" = (select new ListIterator(L), False);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), False));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), False);
    h."first select" = (first (select L, False));
    h."any select" = (any (select L, False));
    h."count select" = (count (select L, False));
    h."first select map" = (first (select (map $1 * 2, L), False));
    h."first pred" = (first (False), L);
    h."first pred select" = (first (False), (select L, True));
    h."any pred" = (any (False), L);
    h."all pred" = (all (False), L);
    h."count pred" = (count (False), L);
    h."count pred map" = (count (False), (map $1 * 2, L));
    h."takewhile" = (takewhile (False), L);
    h."takeuntil" = (takeuntil (False), L);
    h."takewhile map" = (takewhile (False), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (False), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (False), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (False), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (False), L));
    h."takewhile iterator" = (takewhile (False), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, False) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), False) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, False) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (False), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (False), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "false", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, True);
    h."fold select" = (foldl $1 + $2, (select L, True));
    h."select map" = (select (map $1 * 2, L), True);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), True));
    h."map select" = (map $1 * 2, (select L, True));
    h."map-select" = (map $1 * 2, L, True);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, True));
    h."hash map-select" = (map {$1: $1 * 2}, L, True);
    h."select iterator" = (select new ListIterator(L), True);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), True));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), True);
    h."first select" = (first (select L, True));
    h."any select" = (any (select L, True));
    h."count select" = (count (select L, True));
    h."first select map" = (first (select (map $1 * 2, L), True));
    h."first pred" = (first (True), L);
    h."first pred select" = (first (True), (select L, True));
    h."any pred" = (any (True), L);
    h."all pred" = (all (True), L);
    h."count pred" = (count (True), L);
    h."count pred map" = (count (True), (map $1 * 2, L));
    h."takewhile" = (takewhile (True), L);
    h."takeuntil" = (takeuntil (True), L);
    h."takewhile map" = (takewhile (True), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (True), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (True), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (True), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (True), L));
    h."takewhile iterator" = (takewhile (True), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, True) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), True) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, True) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (True), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (True), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "true", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, 1);
    h."fold select" = (foldl $1 + $2, (select L, 1));
    h."select map" = (select (map $1 * 2, L), 1);
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), 1));
    h."map select" = (map $1 * 2, (select L, 1));
    h."map-select" = (map $1 * 2, L, 1);
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, 1));
    h."hash map-select" = (map {$1: $1 * 2}, L, 1);
    h."select iterator" = (select new ListIterator(L), 1);
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), 1));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), 1);
    h."first select" = (first (select L, 1));
    h."any select" = (any (select L, 1));
    h."count select" = (count (select L, 1));
    h."first select map" = (first (select (map $1 * 2, L), 1));
    h."first pred" = (first (1), L);
    h."first pred select" = (first (1), (select L, True));
    h."any pred" = (any (1), L);
    h."all pred" = (all (1), L);
    h."count pred" = (count (1), L);
    h."count pred map" = (count (1), (map $1 * 2, L));
    h."takewhile" = (takewhile (1), L);
    h."takeuntil" = (takeuntil (1), L);
    h."takewhile map" = (takewhile (1), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile (1), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile (1), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil (1), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil (1), L));
    h."takewhile iterator" = (takewhile (1), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, 1) {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), 1) {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, 1) {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile (1), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil (1), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "one", h);
}
{
    hash<auto> h = {};
    h."select" = (select L, "x");
    h."fold select" = (foldl $1 + $2, (select L, "x"));
    h."select map" = (select (map $1 * 2, L), "x");
    h."fold select map" = (foldl $1 + $2, (select (map $1 * 2, L), "x"));
    h."map select" = (map $1 * 2, (select L, "x"));
    h."map-select" = (map $1 * 2, L, "x");
    h."fold map-select" = (foldl $1 + $2, (map $1 * 2, L, "x"));
    h."hash map-select" = (map {$1: $1 * 2}, L, "x");
    h."select iterator" = (select new ListIterator(L), "x");
    h."fold select iterator" = (foldl $1 + $2, (select new ListIterator(L), "x"));
    h."select map iterator" = (select (map $1 * 2, new ListIterator(L)), "x");
    h."first select" = (first (select L, "x"));
    h."any select" = (any (select L, "x"));
    h."count select" = (count (select L, "x"));
    h."first select map" = (first (select (map $1 * 2, L), "x"));
    h."first pred" = (first ("x"), L);
    h."first pred select" = (first ("x"), (select L, True));
    h."any pred" = (any ("x"), L);
    h."all pred" = (all ("x"), L);
    h."count pred" = (count ("x"), L);
    h."count pred map" = (count ("x"), (map $1 * 2, L));
    h."takewhile" = (takewhile ("x"), L);
    h."takeuntil" = (takeuntil ("x"), L);
    h."takewhile map" = (takewhile ("x"), (map $1 * 2, L));
    h."map takewhile" = (map $1 * 2, (takewhile ("x"), L));
    h."fold takewhile" = (foldl $1 + $2, (takewhile ("x"), L));
    h."fold takeuntil map" = (foldl $1 + $2, (takeuntil ("x"), (map $1 * 2, L)));
    h."first takeuntil" = (first (takeuntil ("x"), L));
    h."takewhile iterator" = (takewhile ("x"), new ListIterator(L));
    {
        list<auto> fl = ();
        foreach auto x in (select L, "x") {
            fl += x;
        }
        h."foreach select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (select (map $1 * 2, L), "x") {
            fl += x;
        }
        h."foreach select map" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (map $1 * 2, L, "x") {
            fl += x;
        }
        h."foreach map-select" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takewhile ("x"), L) {
            fl += x;
        }
        h."foreach takewhile" = fl;
    }
    {
        list<auto> fl = ();
        foreach auto x in (takeuntil ("x"), (map $1 * 2, L)) {
            fl += x;
        }
        h."foreach takeuntil map" = fl;
    }
    show("top-level", "string", h);
}
