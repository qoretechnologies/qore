#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the order of the members of hashes, iteration while hashes change, and large hashes; the same output in every
# execution mode (qore#7272)

%modern
%enable-all-warnings

# a class without a member list: objects get members at run time
class Bag {
}

# every key of the hash is found with its value, a key that it does not have is not, and the iteration visits as many
# members as the hash has
string sub verify(hash<auto> h) {
    list<string> errs = ();
    int n = 0;
    foreach hash<auto> p in (h.pairIterator()) {
        if (!h.hasKey(p.key)) {
            push errs, sprintf("no key %y", p.key);
        } else if (h{p.key} !== p.value) {
            push errs, sprintf("value of %y", p.key);
        }
        ++n;
    }
    if (n != h.size()) {
        push errs, sprintf("size %d keys %d", h.size(), n);
    }
    foreach string k in ("absent", "k-1", "naïve-absent", "abcdefghijklmnopqrstuvwxyz-absent") {
        if (h.hasKey(k)) {
            push errs, sprintf("has %y", k);
        }
    }
    return errs ? errs.join(", ") : "ok";
}

# the keys in the order of each way of iterating a hash, which must agree
string sub orders(hash<auto> h) {
    list<string> fwd = keys h;
    list<string> errs = ();
    list<string> it = ();
    HashIterator i(h);
    while (i.next()) {
        push it, i.getKey();
    }
    if (it != fwd) {
        push errs, sprintf("HashIterator %y", it);
    }
    list<string> rev = ();
    HashReverseIterator ri(h);
    while (ri.next()) {
        push rev, ri.getKey();
    }
    if (rev != reverse(fwd)) {
        push errs, sprintf("HashReverseIterator %y", rev);
    }
    list<string> pairs = map $1.key, h.pairIterator();
    if (pairs != fwd) {
        push errs, sprintf("pairIterator %y", pairs);
    }
    list<string> ctx = ();
    foreach string k in (h.keyIterator()) {
        push ctx, k;
    }
    if (ctx != fwd) {
        push errs, sprintf("keyIterator %y", ctx);
    }
    list<auto> vals = h.values();
    list<auto> fvals = map h{$1}, fwd;
    if (vals !== fvals) {
        push errs, sprintf("values %y", vals);
    }
    if (h.firstKey() !== fwd[0] || h.lastKey() !== fwd.last()) {
        push errs, sprintf("first %y last %y", h.firstKey(), h.lastKey());
    }
    return errs ? errs.join(", ") : "ok";
}

string sub summary(hash<auto> h) {
    return sprintf("%d %s %s [%s]", h.size(), verify(h), orders(h), (keys h).join(","));
}

sub show(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

sub show(string label, code c, hash<auto> h) {
    show(label, auto sub () { return c(h); });
}

hash<auto> sub make(int n, string prefix = "k") {
    hash<auto!> h = {};
    for (int i = 0; i < n; ++i) {
        h{prefix + i} = i;
    }
    return h;
}

# the order of insertion, with keys deleted, removed and added again (to the end), at the start, the middle and the end
{
    hash<auto!> h = {"z": 1, "a": 2, "m": 3, "b": 4, "y": 5};
    show("order literal", \summary(), h);
    h.c = 6;
    h.a = 20;
    show("order assigned", \summary(), h);
    delete h.z;
    show("order first deleted", \summary(), h);
    delete h.c;
    show("order last deleted", \summary(), h);
    remove h.m;
    show("order middle removed", \summary(), h);
    h.z = "z again";
    h.m = "m again";
    show("order re-added", \summary(), h);
    h -= "a";
    h -= ("b", "y");
    show("order minus", \summary(), h);
    delete h.z;
    delete h.m;
    show("order emptied", \summary(), h);
    h.only = 1;
    show("order one", \summary(), h);
}

# the order of each size of hash, around the sizes where the key map grows, after every other key was deleted and the
# keys were added again
foreach int n in (1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129) {
    hash<auto!> h = make(n);
    string label = sprintf("size%d", n);
    show(label, string sub () { return sprintf("%s %s %y %y", verify(h), orders(h), h.firstKey(), h.lastKey()); });
    for (int i = 0; i < n; i += 2) {
        delete h{"k" + i};
    }
    show(label + " halved", string sub () { return sprintf("%d %s %s %y %y", h.size(), verify(h), orders(h),
        h.firstKey(), h.lastKey()); });
    for (int i = 0; i < n; i += 2) {
        h{"k" + i} = -i;
    }
    show(label + " refilled", string sub () { return sprintf("%d %s %s %y %y %y", h.size(), verify(h), orders(h),
        h.firstKey(), h.lastKey(), (keys h)[0..3]); });
}

# a large hash grows through many sizes of its key map, loses most of its keys and grows again; every key is found
{
    hash<auto!> h = make(50000);
    show("large", string sub () { return sprintf("%d %s %s %y %y", h.size(), verify(h), orders(h), h.firstKey(),
        h.lastKey()); });
    int removed = 0;
    for (int i = 0; i < 50000; ++i) {
        if (i % 7) {
            remove h{"k" + i};
            ++removed;
        }
    }
    show("large removed", string sub () { return sprintf("%d %d %s %s %y %y", removed, h.size(), verify(h),
        orders(h), (keys h)[0..4], h.lastKey()); });
    for (int i = 0; i < 20000; ++i) {
        h{"n" + i} = i;
    }
    show("large regrown", string sub () { return sprintf("%d %s %s %y %y", h.size(), verify(h), orders(h),
        (keys h)[7140..7145], h.lastKey()); });
    hash<auto!> c = h;
    c.added = True;
    show("large copy", string sub () { return sprintf("%d %s %y %d", c.size(), verify(c), c.lastKey(),
        h.size()); });
    int found = 0;
    for (int i = 0; i < 50000; ++i) {
        if (c.hasKey("k" + i)) {
            ++found;
        }
    }
    show("large lookups", string sub () { return sprintf("%d", found); });
    show("large equal", string sub () { hash<auto> d = h; return sprintf("%y %y", d == h, c == h); });
}

# keys that differ only in their last bytes, empty, long and not ASCII, deleted and added again
{
    hash<auto!> h = {};
    for (int i = 0; i < 40; ++i) {
        h{sprintf("abcdefghijklmnopqrstuvwxyz-%02d", i)} = i;
    }
    h{""} = "empty";
    h."naïve" = "n";
    h."日本語" = "j";
    show("similar keys", \summary(), h);
    for (int i = 0; i < 40; i += 3) {
        delete h{sprintf("abcdefghijklmnopqrstuvwxyz-%02d", i)};
    }
    delete h{""};
    h{""} = "empty again";
    show("similar keys deleted", \summary(), h);
}

# deletes the key \a k of \a h and adds it again with a suffix, at the end
string sub rename_key(reference<hash<auto>> h, string k) {
    auto v = remove h{k};
    h{k + "x"} = v;
    return k;
}

# iterators of a hash that is changed while they iterate: the change copies the hash, so they iterate over the hash
# as it was when they started
{
    hash<auto!> h = make(10);
    list<string> seen = ();
    HashIterator i(h);
    while (i.next()) {
        push seen, i.getKey();
        if (i.getKey() == "k2") {
            delete h.k3;
            delete h.k0;
            h.added = 1;
        }
    }
    show("iterator snapshot", string sub () { return sprintf("%y %s", seen, summary(h)); });
    seen = ();
    HashReverseIterator ri(h);
    while (ri.next()) {
        push seen, ri.getKey();
        if (ri.getKey() == "k5") {
            remove h.k4;
            h.later = 2;
        }
    }
    show("reverse iterator snapshot", string sub () { return sprintf("%y %s", seen, summary(h)); });
    seen = ();
    foreach string k in (keys h) {
        push seen, k;
        if (k == "k6") {
            delete h.k7;
            h.k6x = 3;
        }
    }
    show("foreach keys snapshot", string sub () { return sprintf("%y %s", seen, summary(h)); });
    seen = ();
    foreach hash<auto> p in (h.pairIterator()) {
        push seen, p.key;
        delete h{p.key};
        h{p.key + "!"} = p.value;
    }
    show("pairIterator snapshot", string sub () { return sprintf("%y %s", seen, summary(h)); });
}

# a member deleted from a hash while another reference to the hash iterates it (copy on write), and a hash changed in
# a map expression over its own keys
{
    hash<auto!> h = make(6);
    hash<auto> shared = h;
    HashKeyIterator ki(shared);
    list<string> seen = ();
    while (ki.next()) {
        push seen, ki.getValue();
        delete h{ki.getValue()};
    }
    show("shared iteration", string sub () { return sprintf("%y %d %s", seen, h.size(), summary(shared)); });
    hash<auto!> m = make(5);
    list<auto> r = map rename_key(\m, $1), keys m;
    show("map over own keys", string sub () { return sprintf("%y %s", r, summary(m)); });
}

# the members of objects keep their order as members are deleted and added; a deleted member is added at the end
{
    Bag b();
    # the member names are given at run time
    list<string> names = ("x", "y", "z", "w", "naïve", "v");
    foreach string n in (names[0..4]) {
        b{n} = n;
    }
    remove b{names[1]};
    b{names[1]} = "y again";
    delete b{names[0]};
    b{names[5]} = "v";
    show("object members", string sub () { return sprintf("%y %y", keys b, b{names}); });
    Bag c = b.copy();
    c{names[2]} = "changed";
    show("object copy members", string sub () { return sprintf("%y %y", keys c, keys b); });
}

# hash expressions that build hashes in order, from lists and other hashes
{
    hash<auto!> h = map {$1: $1.size()}, ("one", "two", "three", "two", "four", "one");
    show("map hash", \summary(), h);
    hash<auto!> s = h{"four", "one", "missing", "two"};
    show("slice order", \summary(), s);
    hash<auto!> p = s + h + {"five": 5, "one": "one again"};
    show("plus order", \summary(), p);
    hash<string, int> t = {"b": 2, "a": 1};
    t.c = 3;
    delete t.b;
    t.b = 22;
    show("typed order", string sub () { return sprintf("%s %s", t.fullType(), summary(t)); });
}
