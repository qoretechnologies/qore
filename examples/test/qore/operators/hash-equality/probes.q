%modern
%allow-weak-references
# the probes of hash-equality.qtest: the soft (==, !=) and hard (===, !==) comparisons of hashes (qore#7266); every
# execution mode and qcc-compiled executable must print expected.txt

class C {
}

hashdecl HashEqualityPair {
    int a;
    int b;
}

enum HashEqualityInt { A = 1, B = 2 }
enum HashEqualityFloat { F = 1.5, G = 2.5 }

const NaN = @nan@;

#! the results of ==, !=, === and !== of two untyped values
string sub cmp_auto(auto l, auto r) {
    return (l == r ? "T" : "F") + (l != r ? "T" : "F") + (l === r ? "T" : "F") + (l !== r ? "T" : "F");
}

#! the results of ==, !=, === and !== of two hash values
string sub cmp_hash(hash<auto> l, hash<auto> r) {
    return (l == r ? "T" : "F") + (l != r ? "T" : "F") + (l === r ? "T" : "F") + (l !== r ? "T" : "F");
}

#! compares both ways, untyped and as hashes; the results must be the same in each order
string sub cmp(hash<auto> l, hash<auto> r) {
    string rv = cmp_auto(l, r);
    string other = cmp_auto(r, l);
    if (other != rv) {
        rv += " reversed: " + other;
    }
    string typed = cmp_hash(l, r);
    if (typed != rv) {
        rv += " typed: " + typed;
    }
    return rv;
}

#! the exception raised by a comparison, or its result
string sub cmp_ex(auto l, auto r) {
    try {
        return l == r ? "T" : "F";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

#! a hash with \a n keys whose values are their indexes, in order
hash<auto> sub make_hash(int n) {
    hash<auto> h;
    for (int i = 0; i < n; ++i) {
        h{"key" + i} = i;
    }
    return h;
}

#! the hash with the keys of \a h in reverse order
hash<auto> sub reverse_hash(hash<auto> h) {
    hash<auto> rv;
    foreach string k in (reverse(keys h)) {
        rv{k} = h{k};
    }
    return rv;
}

#! the hash with the value of \a key of \a h replaced by \a v (same place)
hash<auto> sub set_value(hash<auto> h, string key, auto v) {
    h{key} = v;
    return h;
}

#! the hash with the key \a old of \a h renamed to \a new at the same place
hash<auto> sub rename_key(hash<auto> h, string old_key, string new_key) {
    hash<auto> rv;
    foreach hash<auto> i in (h.pairIterator()) {
        rv{i.key == old_key ? new_key : i.key} = i.value;
    }
    return rv;
}

string sub probes() {
    list<string> out;
    hash<auto> h26 = make_hash(26);
    hash<auto> h2 = make_hash(2);
    hash<auto> h1 = make_hash(1);

    # the order of the keys
    out += "empty: " + cmp({}, {});
    out += "same order: " + cmp(h26, make_hash(26));
    out += "copy: " + cmp(h26, h26 + {});
    out += "reverse order: " + cmp(h26, reverse_hash(h26));
    out += "reverse order 2: " + cmp(h2, reverse_hash(h2));
    out += "first two swapped: " + cmp(h26, {"key1": 1, "key0": 0} + (h26 - ("key0", "key1")));
    out += "last two swapped: " + cmp(h26, (h26 - ("key24", "key25")) + {"key25": 25, "key24": 24});
    out += "middle moved to the end: " + cmp(h26, (h26 - "key13") + {"key13": 13});
    out += "end moved to the start: " + cmp(h26, {"key25": 25} + (h26 - "key25"));

    # a value that differs
    out += "value differs first: " + cmp(h26, set_value(h26, "key0", -1));
    out += "value differs middle: " + cmp(h26, set_value(h26, "key13", -1));
    out += "value differs last: " + cmp(h26, set_value(h26, "key25", -1));
    out += "value differs last, reverse order: " + cmp(h26, reverse_hash(set_value(h26, "key25", -1)));
    out += "value differs first, reverse order: " + cmp(h26, reverse_hash(set_value(h26, "key0", -1)));

    # a key that differs
    out += "key differs first: " + cmp(h26, rename_key(h26, "key0", "keyX"));
    out += "key differs middle: " + cmp(h26, rename_key(h26, "key13", "keyX"));
    out += "key differs last: " + cmp(h26, rename_key(h26, "key25", "keyX"));
    out += "key differs last byte: " + cmp(h26, rename_key(h26, "key25", "key2X"));
    out += "key differs in length: " + cmp(h26, rename_key(h26, "key25", "key250"));
    out += "key prefix: " + cmp({"ab": 1}, {"abc": 1});
    out += "long key last byte: " + cmp({"abcdefghijklmnopq1": 1}, {"abcdefghijklmnopq2": 1});
    out += "long key same: " + cmp({"abcdefghijklmnopq1": 1, "x": 2}, {"x": 2, "abcdefghijklmnopq1": 1});
    out += "empty key: " + cmp({"": 1, "a": 2}, {"a": 2, "": 1});
    out += "empty key differs: " + cmp({"": 1}, {"a": 1});
    out += "key differs, values swapped: " + cmp({"a": 1, "b": 2}, {"b": 1, "a": 2});

    # sizes
    out += "subset: " + cmp(h26, h26 - "key25");
    out += "subset first: " + cmp(h26, h26 - "key0");
    out += "empty and one: " + cmp({}, h1);
    out += "nothing value and no key: " + cmp({"a": NOTHING}, {});

    # value types: soft and hard comparisons differ
    out += "int and float: " + cmp({"a": 1, "b": 2}, {"a": 1, "b": 2.0});
    out += "float and int first: " + cmp({"a": 1.0, "b": 2}, {"a": 1, "b": 2});
    out += "int and float differ: " + cmp({"a": 1}, {"a": 1.5});
    out += "int and string: " + cmp({"a": 1}, {"a": "1"});
    out += "int and string differ: " + cmp({"a": 1}, {"a": "2"});
    out += "float and string: " + cmp({"a": 1.5}, {"a": "1.5"});
    out += "int and bool: " + cmp({"a": 1}, {"a": True});
    out += "zero and false: " + cmp({"a": 0}, {"a": False});
    out += "int and number: " + cmp({"a": 1}, {"a": 1n});
    out += "float and number: " + cmp({"a": 1.5}, {"a": 1.5n});
    out += "number and string: " + cmp({"a": 1.5n}, {"a": "1.5"});
    out += "ints: " + cmp({"a": 1, "b": -1}, {"a": 1, "b": -1});
    out += "ints differ in sign: " + cmp({"a": 1}, {"a": -1});
    out += "big ints: " + cmp({"a": 1 << 50, "b": -(1 << 60)}, {"a": 1 << 50, "b": -(1 << 60)});
    out += "big ints differ: " + cmp({"a": 1 << 50}, {"a": (1 << 50) + 1});
    out += "big int and int: " + cmp({"a": 1 << 50}, {"a": 1});
    out += "big int and float: " + cmp({"a": 1 << 50}, {"a": float(1 << 50)});
    out += "floats: " + cmp({"a": 1.5, "b": -2.25}, {"a": 1.5, "b": -2.25});
    out += "floats differ: " + cmp({"a": 1.5}, {"a": 1.25});
    out += "zero and negative zero: " + cmp({"a": 0.0}, {"a": -0.0});
    out += "infinities: " + cmp({"a": 1e308 * 10}, {"a": 1e308 * 10});
    out += "infinities differ: " + cmp({"a": 1e308 * 10}, {"a": -1e308 * 10});
    out += "numbers: " + cmp({"a": 1.5n, "b": 12345678901234567890123n}, {"a": 1.5n, "b": 12345678901234567890123n});
    out += "numbers differ: " + cmp({"a": 12345678901234567890123n}, {"a": 12345678901234567890124n});
    out += "numbers and floats: " + cmp({"a": 1.5n}, {"a": 1.5});

    # NaN is not equal to anything, itself included
    hash<auto> nan_h = {"a": 1, "b": NaN};
    out += "NaN: " + cmp(nan_h, {"a": 1, "b": NaN});
    out += "NaN same hash: " + cmp(nan_h, nan_h);
    out += "NaN copy: " + cmp(nan_h, nan_h + {});
    out += "NaN reverse order: " + cmp(nan_h, reverse_hash(nan_h));
    out += "NaN and int: " + cmp({"a": NaN}, {"a": 1});
    out += "NaN and string: " + cmp({"a": NaN}, {"a": "nan"});
    out += "NaN number: " + cmp({"a": number(NaN)}, {"a": number(NaN)});
    hash<auto> nested_nan = {"x": 1, "y": {"z": NaN}};
    out += "nested NaN same hash: " + cmp(nested_nan, nested_nan);
    out += "nested NaN: " + cmp(nested_nan, {"x": 1, "y": {"z": NaN}});
    hash<auto> list_nan = {"x": (1, NaN)};
    out += "NaN in list same hash: " + cmp(list_nan, list_nan);
    {
        hash<auto> h = nan_h;
        out += "NaN same variable: " + ((h == h ? "T" : "F") + (h != h ? "T" : "F") + (h === h ? "T" : "F")
            + (h !== h ? "T" : "F"));
        auto a = nan_h;
        out += "NaN same untyped variable: " + ((a == a ? "T" : "F") + (a != a ? "T" : "F") + (a === a ? "T" : "F")
            + (a !== a ? "T" : "F"));
    }

    # NOTHING and NULL
    out += "nothing values: " + cmp({"a": NOTHING}, {"a": NOTHING});
    out += "null values: " + cmp({"a": NULL}, {"a": NULL});
    out += "nothing and null: " + cmp({"a": NOTHING}, {"a": NULL});
    out += "nothing and zero: " + cmp({"a": NOTHING}, {"a": 0});
    out += "null and zero: " + cmp({"a": NULL}, {"a": 0});
    out += "nothing and empty string: " + cmp({"a": NOTHING}, {"a": ""});

    # strings
    out += "short strings: " + cmp({"a": "ab", "b": "abcdef"}, {"a": "ab", "b": "abcdef"});
    out += "short strings differ: " + cmp({"a": "ab"}, {"a": "ac"});
    out += "short and long strings: " + cmp({"a": "abcdef"}, {"a": "abcdefg"});
    out += "long strings: " + cmp({"a": strmul("x", 40)}, {"a": strmul("x", 40)});
    out += "long strings differ: " + cmp({"a": strmul("x", 40)}, {"a": strmul("x", 39) + "y"});
    out += "empty strings: " + cmp({"a": ""}, {"a": ""});
    out += "strings in other encodings: " + cmp({"a": convert_encoding("abc", "ISO-8859-1")}, {"a": "abc"});
    out += "non-ascii strings in other encodings: " + cmp({"a": convert_encoding("äb", "ISO-8859-1")}, {"a": "äb"});
    out += "string and date: " + cmp({"a": "2026-01-01"}, {"a": 2026-01-01});

    # binary, date
    out += "binaries: " + cmp({"a": binary("abc")}, {"a": binary("abc")});
    out += "binaries differ: " + cmp({"a": binary("abc")}, {"a": binary("abd")});
    out += "binary and string: " + cmp({"a": binary("abc")}, {"a": "abc"});
    out += "dates: " + cmp({"a": 2026-01-01T10:00:00Z}, {"a": 2026-01-01T10:00:00Z});
    out += "dates in other zones: " + cmp({"a": 2026-01-01T10:00:00Z}, {"a": 2026-01-01T11:00:00+01:00});
    out += "dates differ: " + cmp({"a": 2026-01-01}, {"a": 2026-01-02});
    out += "relative dates: " + cmp({"a": 1D}, {"a": 24h});
    out += "date and int: " + cmp({"a": 1970-01-01T00:00:01Z}, {"a": 1});

    # nested containers
    out += "nested reverse order: " + cmp({"a": {"x": 1, "y": 2}, "b": 1}, {"b": 1, "a": {"y": 2, "x": 1}});
    out += "nested soft: " + cmp({"a": {"x": 1}}, {"a": {"x": 1.0}});
    out += "nested differs: " + cmp({"a": {"x": 1}}, {"a": {"x": 2}});
    out += "nested key differs: " + cmp({"a": {"x": 1}}, {"a": {"y": 1}});
    out += "lists: " + cmp({"a": (1, 2.0, "3")}, {"a": (1.0, "2", 3)});
    out += "lists of ints: " + cmp({"a": (1, 2, 3)}, {"a": (1, 2, 3)});
    out += "lists of floats: " + cmp({"a": (1.5, 2.5)}, {"a": (1.5, 2.5)});
    out += "lists of short strings: " + cmp({"a": ("a", "b")}, {"a": ("a", "b")});
    out += "lists differ: " + cmp({"a": (1, 2, 3)}, {"a": (1, 2, 4)});
    out += "lists differ in size: " + cmp({"a": (1, 2)}, {"a": (1, 2, 3)});
    out += "list and hash: " + cmp({"a": (1,)}, {"a": {"0": 1}});
    out += "hashes in lists: " + cmp({"a": ({"x": 1, "y": 2},)}, {"a": ({"y": 2, "x": 1.0},)});
    out += "hash and nothing: " + cmp({"a": {}}, {"a": NOTHING});

    # objects compare by identity
    C c1();
    C c2();
    out += "same object: " + cmp({"a": c1}, {"a": c1});
    out += "different objects: " + cmp({"a": c1}, {"a": c2});
    out += "object and hash: " + cmp({"a": c1}, {"a": {}});

    # hashdecl hashes and plain hashes
    hash<HashEqualityPair> p1 = <HashEqualityPair>{"a": 1, "b": 2};
    out += "hashdecl and hash: " + cmp(p1, {"a": 1, "b": 2});
    out += "hashdecl and hash in other order: " + cmp(p1, {"b": 2, "a": 1});
    out += "hashdecl and soft hash: " + cmp(p1, {"a": 1.0, "b": 2});
    out += "hashdecls: " + cmp(p1, <HashEqualityPair>{"a": 1, "b": 2});
    out += "hashdecls differ: " + cmp(p1, <HashEqualityPair>{"a": 1, "b": 3});
    hash<string, int> typed = {"a": 1, "b": 2};
    out += "typed hash and hash: " + cmp(typed, {"b": 2, "a": 1});
    out += "typed hash and hashdecl: " + cmp(typed, p1);

    # weak references
    {
        hash<auto> target = {"x": 1, "y": 2};
        hash<auto> w;
        w := target;
        out += "weak reference: " + cmp(w, {"y": 2, "x": 1});
        out += "weak reference untyped: " + cmp_auto(w, {"x": 1, "y": 2}) + " " + cmp_auto({"x": 1, "y": 2}, w);
        hash<auto!> holder = {"a": 1};
        holder.w := target;
        out += "weak reference member: " + cmp(holder, {"a": 1, "w": {"y": 2, "x": 1}});
        out += "weak reference member differs: " + cmp(holder, {"a": 1, "w": {"x": 2, "y": 2}});
    }

    # comparisons that raise exceptions
    out += "invalid date: " + cmp_ex({"a": 1, "b": 2026-01-01}, {"a": 1, "b": 1e300});
    out += "invalid date reverse order: " + cmp_ex({"b": 2026-01-01, "a": 1}, {"a": 1, "b": 1e300});

    # the order of the comparisons: a member that is not equal before one whose comparison raises an exception (a
    # string that cannot be converted to the encoding of the other)
    {
        auto bad = convert_encoding("ä", "ISO-8859-1");
        string euro = "€";
        out += "exception: " + cmp_ex({"a": 1, "b": bad}, {"a": 1, "b": euro});
        out += "exception, reverse order: " + cmp_ex({"b": bad, "a": 1}, {"a": 1, "b": euro});
        out += "order: " + cmp_ex({"b": bad, "a": 1}, {"a": 2, "b": euro});
        out += "order 2: " + cmp_ex({"a": 2, "b": bad}, {"b": euro, "a": 1});
        out += "order, same keys: " + cmp_ex({"a": 2, "b": bad}, {"a": 1, "b": euro});
        out += "order, same keys 2: " + cmp_ex({"b": bad, "a": 2}, {"b": euro, "a": 1});
        out += "order after a key mismatch: " + cmp_ex({"x": 1, "b": bad, "a": 2}, {"x": 1, "a": 1, "b": euro});
        out += "order after a key mismatch 2: " + cmp_ex({"x": 1, "a": 2, "b": bad}, {"x": 1, "b": euro, "a": 1});
        # a list is compared with the elements of the right list on the left
        out += "order in lists: " + cmp_ex({"l": (euro, 1)}, {"l": (bad, 2)}) + " "
            + cmp_ex({"l": (1, euro)}, {"l": (2, bad)});
        out += "hard comparison with an exception: " + (({"a": bad} === {"a": euro}) ? "T" : "F");
    }

    # enum values
    out += "enum and int: " + cmp({"a": HashEqualityInt::A}, {"a": 1});
    out += "enum and float: " + cmp({"a": HashEqualityInt::A}, {"a": 1.0});
    out += "enums: " + cmp({"a": HashEqualityInt::A, "b": HashEqualityFloat::F},
        {"a": HashEqualityInt::A, "b": HashEqualityFloat::F});
    out += "enums differ: " + cmp({"a": HashEqualityInt::A}, {"a": HashEqualityInt::B});
    out += "float enum and float: " + cmp({"a": HashEqualityFloat::F}, {"a": 1.5});
    out += "float enum and string: " + cmp({"a": HashEqualityFloat::F}, {"a": "1.5"});
    out += "enums in lists: " + cmp({"l": (HashEqualityInt::A, HashEqualityFloat::G)}, {"l": (1, 2.5)});

    # char values
    {
        string s = "abca";
        auto ca = s[0];
        auto cb = s[1];
        out += "chars: " + cmp({"a": ca}, {"a": s[3]});
        out += "chars differ: " + cmp({"a": ca}, {"a": cb});
        out += "char and string: " + cmp({"a": ca}, {"a": "a"});
        out += "char and int: " + cmp({"a": ca}, {"a": 97});
        out += "chars in lists: " + cmp({"l": (ca, cb)}, {"l": ("a", s[1])});
    }

    # soft conversions after the walk breaks at a key mismatch
    out += "soft after a key mismatch: " + cmp({"a": 1, "b": 2, "c": 3}, {"a": 1.0, "c": "3", "b": 2});
    out += "soft nested after a key mismatch: " + cmp({"n": {"a": 1, "b": 2, "c": 3}},
        {"n": {"a": 1.0, "c": "3", "b": 2n}});
    out += "big int and float in lists: " + cmp({"l": (1 << 50, 1)}, {"l": (float(1 << 50), 1.0)});

    # the other comparisons of hashes
    out += "inlist: " + (inlist({"a": 1, "b": 2}, ({"b": 2.0, "a": 1},)) ? "T" : "F")
        + (inlist({"a": 1, "b": NaN}, ({"a": 1, "b": NaN},)) ? "T" : "F");
    out += "list equality: " + cmp_auto(({"a": 1, "b": 2},), ({"b": 2, "a": 1.0},));

    return foldl $1 + "\n" + $2, out;
}
