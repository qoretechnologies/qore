# probes of default-lvalue-operators.qtest (qore#5947): default values of parameters that modify a variable (splice,
# extract, regex substitution, transliteration, the assignment operators, increment and decrement, push, unshift, pop,
# shift, remove and delete) give their value and have their effect also in a qcc executable; probes() returns one line
# per pass, so that the code runs on every tier with tiered execution

%modern

our list<softbool> values = (False, False);
our int n = 1;
our list<int> l = (1, 2, 3);
our hash<auto> h = {"a": 1, "b": 2};
our *int onv;
our string s = "ab";
our string rs = "abcb";
our string us = "aaa-H\u00e9llo";

auto sub splice_extract(auto spliced = (splice values, 0, 1, "text"), auto extracted = (extract values, 1, 1, "text"))
        {
    return (spliced, extracted, values);
}
auto sub assign_ops(auto a = (n += 2), auto b = (n -= 1), auto c = (n *= 3), auto d = (n |= 8), auto e = (n <<= 1),
        auto f = (n >>= 2), auto g = (n %= 5), auto h2 = (n /= 2), auto i = (n &= 7), auto j = (n ^= 2)) {
    return (a, b, c, d, e, f, g, h2, i, j, n);
}
auto sub incdec(auto a = n++, auto b = ++n, auto c = n--, auto d = --n) {
    return (a, b, c, d, n);
}
auto sub list_ops(auto a = (push l, 4), auto b = (unshift l, 0), auto c = pop l, auto d = shift l) {
    return (a, b, c, d, l);
}
auto sub hash_ops(auto a = remove h.a, auto b = (h.c = 3), auto c = (delete h.b)) {
    return (a, b, c, h);
}
auto sub null_ops(auto c = (s += "c"), auto d = (onv = 7)) {
    return (c, d, onv, s);
}

auto sub regex_ops(auto a = (rs =~ s/(b)/[$1]|/g), auto b = (rs =~ tr/a-c/A-C/), auto c = (rs =~ s/x/y/)) {
    return (a, b, c, rs);
}

auto sub regex_flags(auto a = (us =~ s/a+/X/U), auto b = (us =~ s/\w+/W/gu)) {
    return (a, b, us);
}

string sub probes() {
    string rv;
    for (int i = 0; i < 4; ++i) {
        values = (False, False);
        n = 1;
        l = (1, 2, 3);
        h = {"a": 1, "b": 2};
        onv = NOTHING;
        s = "ab";
        rs = "abcb";
        us = "aaa-H\u00e9llo";
        rv += sprintf("%d: %y %y %y %y %y %y %y %y\n", i, splice_extract(), assign_ops(), incdec(), list_ops(),
            hash_ops(), null_ops(), regex_ops(), regex_flags());
    }
    return rv;
}
