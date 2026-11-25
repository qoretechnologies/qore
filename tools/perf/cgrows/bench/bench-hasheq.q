%modern
# hash equality loops (qore#7266): L=hes (== of two equal 26-key hashes with their keys in the same order), her (the
# same with the keys of one hash in reverse order), hsw (the same with the first two keys of one hash swapped), hfv (the
# value of the first key differs), hfk (the first key differs), hhe (=== of two equal hashes, keys in the same order),
# hhr (=== with the keys in reverse order); N iterations; 25 int values and a nested hash with an int, a string and a
# list of ints
hash<auto> sub make_hash() {
    hash<auto> h;
    for (int i = 0; i < 25; ++i) {
        h{"k" + i} = i;
    }
    h.nested = {"a": 1, "b": "two", "c": (1, 2, 3)};
    return h;
}
hash<auto> sub reverse_hash(hash<auto> h) {
    hash<auto> rv;
    foreach string k in (reverse(keys h)) {
        rv{k} = h{k};
    }
    return rv;
}
int sub f(int a) { return a + 1; }
int sub run_eq(int n, hash<auto> a, hash<auto> b) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (a == b ? 1 : 0);
    }
    return t;
}
int sub run_heq(int n, hash<auto> a, hash<auto> b) {
    int t = 0;
    for (int i = 0; i < n; ++i) {
        t += (a === b ? 1 : 0);
    }
    return t;
}
int n = ENV.N.toInt();
hash<auto> h = make_hash();
# a copy with new member lists and the same values
hash<auto> same = h + {};
switch (ENV.L) {
    case "hes": run_eq(n, h, same); break;
    case "her": run_eq(n, h, reverse_hash(h)); break;
    case "hsw": run_eq(n, h, {"k1": 1, "k0": 0} + (h - ("k0", "k1"))); break;
    case "hfv": run_eq(n, h, h + {"k0": -1}); break;
    case "hfk": run_eq(n, h, {"kX": 0} + (h - "k0")); break;
    case "hhe": run_heq(n, h, same); break;
    case "hhr": run_heq(n, h, reverse_hash(h)); break;
}
