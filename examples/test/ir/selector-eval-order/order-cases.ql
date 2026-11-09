# -*- mode: qore; indent-tabs-mode: nil -*-
# the cases of selector-eval-order.qtest: the order in which lists of selectors with ranges and the bounds of ranges
# are evaluated; included by order-plain.q and order-negative-offsets.q

list<int> l = (0, 1, 2, 3, 4);
string s = "abcdef";
binary b = binary("abcdef");
hash<auto> h = {"a": 1, "b": 2};
*int k;

#! records that an expression was evaluated
int sub f(string tag, int rv = 1) {
    print("called " + tag + "\n");
    return rv;
}

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    try {
        printf("%s: %y\n", name, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
}

# a range: the end is evaluated only after the start has been checked
auto sub range_start() { return k..f("range end"); }
auto sub range_end() { return f("range start")..k; }
auto sub range_ok() { return f("range start", 2)..f("range end", 4); }

# a list of selectors: each selector is evaluated after the previous one has been applied
auto sub list_start_end() { return l[k..f("end"), 0]; }
auto sub list_start_later() { return l[k..3, f("later")]; }
auto sub list_end_later() { return l[1..k, f("later")]; }
auto sub list_later_range() { return l[0, f("start")..k, f("later")]; }
auto sub list_ok() { return l[f("a")..f("b", 3), f("c", 0), 4..f("d", 2)]; }
auto sub list_types() { return (l[f("a")..f("b", 2), f("c", 4)]).fullType(); }
auto sub list_mixed_types() {
    list<auto> m = (1, "two", 3.0);
    auto r = m[f("a", 0)..f("b", 1), f("c", 2)];
    return (r.fullType(), r);
}
auto sub list_literal() { return (0, 1, 2, 3)[k..f("end"), 0]; }
# a selector that assigns the variable that holds the sequence: the selectors select from the sequence that the
# variable held before
auto sub list_reassigned() {
    list<auto> m = (1, 2, 3);
    auto r = m[0, f("a", (m = (7, 8, 9)).size() - 2)..2];
    return (r, m);
}
auto sub string_reassigned() {
    string m = "abc";
    auto r = m[0, f("a", (m = "xyz").size() - 2)..2];
    return (r, m);
}
# selectors without side effects, which are evaluated before they are applied
auto sub list_inert_start() { return l[k..3, 0]; }
auto sub list_inert_ok() { int i = 1; return l[i..3, 0, 4..i]; }

# a sequence that no selector applies to: no selector is evaluated
auto sub nothing_seq(auto seq) { return seq[f("start")..2, f("later")]; }

# strings and binary objects
auto sub string_start_later() { return s[k..2, f("later")]; }
auto sub string_ok() { return s[f("a")..f("b", 3), f("c", 0)]; }
auto sub binary_end_later() { return b[0, 1..k, f("later")]; }
auto sub binary_ok() { return b[f("a", 0), f("b", 2)..f("c", 3)]; }

# a hash slice with a range
auto sub hash_slice_start() { return h{"a", k..f("end")}; }
auto sub hash_slice_ok() { return h{f("a") ? "a" : "b", f("b")..f("c", 2)}; }

# remove and delete with a list of selectors
auto sub remove_ok() { list<auto> m = l; auto r = remove m[f("a")..f("b", 2), f("c", 4)]; return (r, m); }
auto sub remove_hash_slice_start() { hash<auto> m = h; auto r = remove m{"a", k..f("end")}; return (r, m); }
auto sub delete_hash_slice_start() { hash<auto> m = h; delete m{"a", k..f("end")}; return m; }

# ranges with negative bounds, which select from the end with %negative-offsets
auto sub negative_later() { return l[f("start", -2)..f("end", -1), f("later", -1)]; }
auto sub negative_descending() { return s[f("start", -1)..f("end", -3), k..f("later")]; }

sub main() {
    # every case runs more than once, so that the functions are compiled with tiered execution
    for (int i = 0; i < 3; ++i) {
        t("range start", \range_start());
        t("range end", \range_end());
        t("range ok", \range_ok());
        t("list start end", \list_start_end());
        t("list start later", \list_start_later());
        t("list end later", \list_end_later());
        t("list later range", \list_later_range());
        t("list ok", \list_ok());
        t("list types", \list_types());
        t("list mixed types", \list_mixed_types());
        t("list literal", \list_literal());
        t("list reassigned", \list_reassigned());
        t("string reassigned", \string_reassigned());
        t("list inert start", \list_inert_start());
        t("list inert ok", \list_inert_ok());
        t("nothing", auto sub () { return nothing_seq(); });
        t("hash", auto sub () { return nothing_seq(h); });
        t("int", auto sub () { return nothing_seq(1); });
        t("string start later", \string_start_later());
        t("string ok", \string_ok());
        t("binary end later", \binary_end_later());
        t("binary ok", \binary_ok());
        t("hash slice start", \hash_slice_start());
        t("hash slice ok", \hash_slice_ok());
        t("remove ok", \remove_ok());
        t("remove hash slice start", \remove_hash_slice_start());
        t("delete hash slice start", \delete_hash_slice_start());
        t("negative later", \negative_later());
        t("negative descending", \negative_descending());
    }
}

