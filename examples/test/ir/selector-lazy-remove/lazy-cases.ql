# -*- mode: qore; indent-tabs-mode: nil -*-
# the cases of selector-lazy-remove.qtest: lists of selectors with ranges that a foreach statement or a functional
# or streaming operator iterates lazily, and remove and delete with a list of selectors whose later selector raises;
# included by lazy-plain.q

# Copyright (C) 2026 Qore Technologies, s.r.o.

list<int> l = (10, 11, 12, 13, 14, 15);
string s = "abcdef";
binary b = binary("abcdef");
hash<auto> h = {"a": 1, "b": 2, "c": 3};
*int k;

#! records that an expression was evaluated
int sub f(string tag, int rv = 1) {
    print("  sel " + tag + "\n");
    return rv;
}

#! records a value that the body or an operator's expression uses
auto sub use(string tag, auto v) {
    printf("  %s %y\n", tag, v);
    return v;
}

#! an object whose creation and destruction are shown
class Obj {
    public {
        string name;
    }

    constructor(string n) {
        name = n;
    }

    destructor() {
        print("  destroy " + name + "\n");
    }

    string toString() {
        return name;
    }
}

#! returns a list of new objects
list<Obj> sub objs(string prefix, int n) {
    list<Obj> rv = ();
    for (int i = 0; i < n; ++i) {
        rv += new Obj(prefix + i);
    }
    return rv;
}

#! runs a case and prints its result or its exception
sub t(string name, code<auto()> c) {
    printf("%s:\n", name);
    try {
        printf("  = %y\n", c());
    } catch (hash<ExceptionInfo> ex) {
        printf("  ! %s: %s\n", ex.err, ex.desc);
    }
}

# --- foreach ---

auto sub foreach_list() {
    foreach int x in (l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)]) {
        use("body " + $#, x);
    }
}

auto sub foreach_break() {
    foreach int x in (l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)]) {
        use("body", x);
        if (x == 12) {
            break;
        }
    }
    return "after";
}

auto sub foreach_continue() {
    foreach int x in (l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)]) {
        if (x == 11) {
            continue;
        }
        use("body", x);
    }
}

auto sub foreach_body_throws() {
    foreach int x in (l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)]) {
        use("body", x);
        if (x == 11) {
            throw "BODY-ERROR", sprintf("at %d", x);
        }
    }
}

auto sub foreach_selector_throws() {
    foreach int x in (l[f("a", 0), k..f("c", 3), f("d", 5)]) {
        use("body", x);
    }
}

auto sub foreach_end_throws() {
    foreach int x in (l[f("a", 0), f("b", 2)..k, f("d", 5)]) {
        use("body", x);
    }
}

# the exception leaves the function, which has no handler
auto sub foreach_selector_throws_unhandled() {
    foreach int x in (l[f("a", 0), f("b", 1), k..2]) {
        use("body", x);
    }
}

auto sub foreach_selector_throws_try() {
    try {
        foreach int x in (l[f("a", 0), k..f("c", 3)]) {
            use("body", x);
        }
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return "after";
}

auto sub foreach_descending() {
    foreach int x in (l[f("a", 3)..f("b", 1), f("c", 0)]) {
        use("body", x);
    }
}

auto sub foreach_out_of_range() {
    foreach auto x in (l[f("a", 4)..f("b", 7), f("c", 9)]) {
        use("body", x);
    }
}

auto sub foreach_nested() {
    for (int i = 0; i < 3; ++i) {
        foreach int x in (l[f("a", i), f("b", 1)..f("c", 2)]) {
            use("body " + i, x);
            if (x == 11) {
                break;
            }
        }
    }
}

auto sub foreach_reassign() {
    list<int> m = (1, 2, 3, 4);
    foreach int x in (m[f("a", 0), f("b", 1)..f("c", 3)]) {
        use("body", x);
        m = (7, 8, 9, 10);
    }
    return m;
}

auto sub foreach_string() {
    foreach auto x in (s[f("a", 0), f("b", 2)..f("c", 3)]) {
        use("body", x);
    }
}

auto sub foreach_binary() {
    foreach auto x in (b[f("a", 0), f("b", 2)..f("c", 3)]) {
        use("body", x);
    }
}

auto sub foreach_string_throws() {
    foreach auto x in (s[f("a", 0), k..f("c", 3)]) {
        use("body", x);
    }
}

auto sub foreach_hash_seq() {
    auto hv = h;
    foreach auto x in (hv[f("a", 0), f("b", 1)..2]) {
        use("body", x);
    }
    return "none";
}

auto sub foreach_nothing_seq() {
    auto nv;
    foreach auto x in (nv[f("a", 0), f("b", 1)..2]) {
        use("body", x);
    }
    return "none";
}

auto sub foreach_hash_slice() {
    foreach auto x in (h{use("key", "a"), use("key", "c")}) {
        use("body", x);
    }
}

auto sub foreach_objects() {
    list<Obj> ol = objs("o", 4);
    foreach Obj o in (ol[f("a", 3), f("b", 0)..f("c", 1)]) {
        use("body", o.name);
        ol = ();
        print("  cleared\n");
    }
    return "after";
}

auto sub foreach_objects_break() {
    list<Obj> ol = objs("p", 4);
    foreach Obj o in (ol[f("a", 1), f("b", 2)..f("c", 3)]) {
        use("body", o.name);
        ol = ();
        break;
    }
    return "after";
}

auto sub foreach_objects_selector_throws() {
    try {
        list<Obj> ol = objs("q", 3);
        foreach Obj o in (ol[f("a", 0), k..f("c", 1)]) {
            use("body", o.name);
            ol = ();
        }
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return "after";
}

auto sub foreach_objects_body_throws() {
    try {
        list<Obj> ol = objs("r", 3);
        foreach Obj o in (ol[f("a", 2), f("b", 0)..f("c", 1)]) {
            use("body", o.name);
            ol = ();
            throw "BODY-ERROR";
        }
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return "after";
}

# --- functional and streaming operators ---

auto sub map_list() {
    return map use("map " + $#, $1 * 2), l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub map_select() {
    return map use("map", $1), l[f("a", 0), f("b", 1)..f("c", 3)], use("select", $1) % 2;
}

#! uses a value; raises an exception for 11
auto sub use_raise(auto v) {
    if (v == 11) {
        throw "USE-ERROR", sprintf("at %y", v);
    }
    return use("use", v);
}

auto sub map_throws() {
    return map use_raise($1), l[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub select_throws() {
    return select l[f("a", 0), f("b", 1)..f("c", 3)], use_raise($1);
}

auto sub foldl_throws() {
    return foldl $1 + use_raise($2), l[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub map_selector_throws() {
    return map use("map", $1), l[f("a", 0), k..f("c", 3)];
}

auto sub map_string() {
    return map use("map", $1), s[f("a", 0), f("b", 2)..f("c", 3)];
}

auto sub map_binary() {
    return map use("map", $1), b[f("a", 0), f("b", 2)..f("c", 3)];
}

auto sub map_hash_seq() {
    auto hv = h;
    return map use("map", $1), hv[f("a", 0), f("b", 1)..2];
}

auto sub map_objects() {
    list<Obj> ol = objs("m", 3);
    list<string> rv = map use("map", $1.name), ol[f("a", 2), f("b", 0)..f("c", 1)];
    ol = ();
    print("  cleared\n");
    return rv;
}

auto sub map_map() {
    return map use("outer", $1 + 1), (map use("inner", $1 * 10), l[f("a", 0), f("b", 1)..f("c", 2)]);
}

auto sub select_list() {
    return select l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)], use("select", $1) % 2;
}

auto sub foldl_list() {
    return foldl use("fold", $1 + $2), l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub foldr_list() {
    return foldr use("fold", $1 + $2), l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub hmap_list() {
    return map {use("key", $1.toString()): $1}, l[f("a", 0), f("b", 1)..f("c", 2)];
}

auto sub take_list() {
    return take 2, l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub first_list() {
    return first use("first", $1) > 11, l[f("a", 0), f("b", 1)..f("c", 3), f("d", 5)];
}

auto sub any_list() {
    return any use("any", $1) == 10, l[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub count_list() {
    return count use("count", $1) > 10, l[f("a", 0), f("b", 1)..f("c", 3)];
}

auto sub foreach_map_pipeline() {
    foreach int x in (map use("map", $1 + 100), l[f("a", 0), f("b", 1)..f("c", 2)]) {
        use("body", x);
    }
}

# --- inert selectors: constants and int locals, whose evaluation cannot be observed; a list of selectors that is
# iterated gives the elements of its value ---

auto sub inert_foreach() {
    int z = 0;
    int e = -1;
    list<auto> rv = ();
    foreach int x in (l[z, 2..e]) {
        rv += x;
    }
    foreach int x in (l[0, 4..-2]) {
        rv += x;
    }
    return rv;
}

auto sub inert_map() {
    int z = 0;
    int e = -1;
    return (map $1 * 2, l[z, 2..e]) + (map $1, l[1, -1..-3]);
}

auto sub inert_select() {
    int z = 0;
    int e = -1;
    return select l[z, 2..e], $1 % 2;
}

auto sub inert_foldl() {
    int z = 0;
    int e = -1;
    return foldl $1 + $2, l[z, 2..e];
}

auto sub inert_take() {
    int z = 0;
    int e = -1;
    return take 5, l[z, 2..e];
}

auto sub inert_pipeline() {
    int z = 0;
    int e = -1;
    list<auto> rv = ();
    foreach auto x in (map $1 + 1, l[z, 2..e]) {
        rv += x;
    }
    return rv;
}

auto sub inert_value() {
    int z = 0;
    int e = -1;
    # a list of selectors that is not iterated gives the elements of its ranges
    list<int> v = l[z, 2..e];
    return (v, l[0, 4..-2], s[z, 2..e], b[z, 2..e]);
}

auto sub inert_remove() {
    list<int> m = (0, 1, 2, 3, 4);
    int z = 0;
    int e = -1;
    auto r = remove m[z, 2..e];
    return (r, m);
}

# --- int locals read by the selectors and written by the code that iterates the list: the selectors are evaluated
# lazily, after the writes of the iterations before them ---

auto sub written_foreach() {
    int z = 0;
    list<auto> rv = ();
    foreach int x in (l[0, z..2]) {
        push rv, x;
        z = 4;
    }
    return rv;
}

auto sub written_foreach_inc() {
    int z = 0;
    int e = 1;
    list<auto> rv = ();
    foreach auto x in (l[z, z..e, e]) {
        push rv, x;
        ++z;
        e += 2;
    }
    return rv;
}

auto sub written_foreach_nested_var() {
    int z = 0;
    list<auto> rv = ();
    foreach int x in (l[1, z..2]) {
        foreach z in (3, 4) {
            push rv, x * 100 + z;
        }
    }
    return rv;
}

auto sub written_map() {
    int z = 0;
    return map (z = $1 - 10, $1)[1], l[0, z..2];
}

auto sub written_select() {
    int z = 0;
    return select l[0, z..3], (z = 3, True)[1];
}

auto sub written_foldl() {
    int z = 0;
    return foldl (z = 2, $1 + $2)[1], l[0, 1, z..4];
}

auto sub stable_arithmetic() {
    int z = 1;
    int e = 3;
    list<auto> rv = ();
    foreach int x in (l[z - 1, z + 1..e + 1, -e]) {
        push rv, x;
    }
    return (rv, (map $1 + 1, l[z - 1, z + 1..e + 1, -e]), l[z - 1, z + 1..e + 1, -e]);
}

auto sub written_other_local() {
    int z = 1;
    int other = 0;
    list<auto> rv = ();
    foreach int x in (l[z, z..3]) {
        push rv, x;
        other = x;
    }
    return (rv, other);
}

# --- map, select and foldl expressions that only compute values from numeric elements: the selectors, which have
# side effects, can be evaluated before the iteration, which cannot observe it ---

auto sub pure_map() {
    return map $1 + 1, l[f("a", 0), f("b", 1)..f("c", 3), -f("d", 1)];
}

auto sub pure_map_raises() {
    return map $1 - 1, l[f("a", 0), k..f("c", 3)];
}

auto sub pure_select() {
    return select l[f("a", 0), f("b", 1)..f("c", 4)], $1 > 11;
}

auto sub pure_foldl() {
    return foldl $1 + $2, l[f("a", 0), f("b", 1)..f("c", 9)];
}

auto sub pure_map_mult() {
    int z = 1;
    return (map $1 * 2, l[f("a", 0), f("b", 1)..f("c", 3)]) + (map $1 * 3, l[z * 2, z..z * 3]);
}

auto sub pure_map_strings() {
    list<string> sl = ("a", "b", "c", "d");
    return map $1 + "x", sl[f("a", 0), f("b", 1)..f("c", 2)];
}

# --- a sequence that no selector applies to: a functional operator's value is NOTHING ---

auto sub none_map() {
    *list<int> n;
    return map $1, n[0, f("a", 1)..2];
}

auto sub none_select() {
    *list<int> n;
    return select n[0, f("a", 1)..2], True;
}

auto sub none_take() {
    auto hv = h;
    return take 1, hv[0, f("a", 1)..2];
}

#! removes the variable that \a r refers to, which makes an int local NOTHING
sub clear_ref(reference<int> r) {
    remove r;
}

auto sub remove_cleared_bound() {
    list<int> m = (10, 11, 12, 13, 14);
    int a = 1;
    int e = 2;
    clear_ref(\e);
    try {
        remove m[0, a..e];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

# --- iterated elements are the elements of the value: a list of selectors with a range that a foreach statement or
# an operator iterates gives the elements that it gives as a value, for every bound (qore#5688) ---

#! the bounds of the ranges compared: negative, reversed, out of range and single
const RangeBounds = ((1, -1), (-2, -5), (-1, 1), (1, 9), (-9, 2), (4, -9), (-9, -7), (2, 2), (-3, -3));

#! prints whether the elements iterated are those of the value
sub check_same(string name, auto value, auto iterated) {
    if (value == iterated) {
        printf("  %s: same %y\n", name, value);
    } else {
        printf("  %s: MISMATCH iterated %y value %y\n", name, iterated, value);
    }
}

auto sub same_list() {
    foreach list<int> bounds in (RangeBounds) {
        int z = 0;
        int st = bounds[0];
        int en = bounds[1];
        string tag = sprintf("list [0, %d..%d]", st, en);
        auto value = l[z, st..en];
        list<auto> it = ();
        foreach auto x in (l[z, st..en]) {
            push it, x;
        }
        check_same(tag + " foreach", value, it);
        it = ();
        foreach auto x in (l[f("z", z), f("s", st)..f("e", en)]) {
            push it, x;
        }
        check_same(tag + " foreach lazy", value, it);
        check_same(tag + " map", value, (map $1, l[z, st..en]));
        check_same(tag + " map lazy", value, (map $1, l[f("z", z), f("s", st)..f("e", en)]));
        check_same(tag + " select", value, (select l[z, st..en], True));
        check_same(tag + " select lazy", value, (select l[f("z", z), f("s", st)..f("e", en)], True));
        check_same(tag + " foldl", (foldl $1 + ($2 ?? 0), value), (foldl $1 + ($2 ?? 0), l[z, st..en]));
        check_same(tag + " foldl lazy", (foldl $1 + ($2 ?? 0), value),
            (foldl $1 + ($2 ?? 0), l[f("z", z), f("s", st)..f("e", en)]));
    }
}

auto sub same_string() {
    foreach list<int> bounds in (RangeBounds) {
        int z = 0;
        int st = bounds[0];
        int en = bounds[1];
        string tag = sprintf("string [0, %d..%d]", st, en);
        auto value = s[z, st..en];
        list<auto> it = ();
        foreach auto x in (s[f("z", z), f("s", st)..f("e", en)]) {
            push it, x;
        }
        check_same(tag + " foreach lazy", (value,), it);
        check_same(tag + " map lazy", value, (map $1, s[f("z", z), f("s", st)..f("e", en)]));
        check_same(tag + " map", value, (map $1, s[z, st..en]));
    }
}

auto sub same_binary() {
    foreach list<int> bounds in (RangeBounds) {
        int z = 0;
        int st = bounds[0];
        int en = bounds[1];
        string tag = sprintf("binary [0, %d..%d]", st, en);
        auto value = b[z, st..en];
        list<auto> it = ();
        foreach auto x in (b[f("z", z), f("s", st)..f("e", en)]) {
            push it, x;
        }
        check_same(tag + " foreach lazy", (value,), it);
        check_same(tag + " map lazy", value, (map $1, b[f("z", z), f("s", st)..f("e", en)]));
        check_same(tag + " map", value, (map $1, b[z, st..en]));
    }
}

# --- remove and delete with a raising later selector ---

auto sub remove_partial() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        remove m[0, k..2];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

auto sub remove_partial_two() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        auto r = remove m[f("a", 3), f("b", 1), f("c", 3)..k, f("d", 0)];
        print("  removed " + r + "\n");
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

auto sub remove_partial_range() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        remove m[f("a", 1)..f("b", 2), k, f("c", 4)..k];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

auto sub remove_first_throws() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        remove m[k..f("a", 1), 0];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

auto sub remove_ok() {
    list<int> m = (0, 1, 2, 3, 4);
    auto r = remove m[f("a", 0), f("b", 2)..f("c", 3)];
    return (r, m);
}

auto sub remove_string_partial() {
    string str = "abcdef";
    try {
        remove str[f("a", 1), k..2];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return str;
}

auto sub remove_binary_partial() {
    binary bin = binary("abcdef");
    try {
        remove bin[f("a", 1), f("b", 3), k..2];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return bin;
}

auto sub remove_nested_partial() {
    hash<auto> hv = {"x": (0, 1, 2, 3)};
    try {
        remove hv.x[f("a", 0), k..1];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return hv;
}

auto sub remove_hash_slice_throws() {
    hash<auto> hv = {"a": 1, "b": 2, "c": 3};
    try {
        remove hv{"a", k..2};
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return hv;
}

auto sub delete_hash_slice_throws() {
    hash<auto> hv = {"a": 1, "b": 2, "c": 3};
    try {
        delete hv{"a", "b", k..2};
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return hv;
}

auto sub delete_partial() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        delete m[f("a", 4), f("b", 2), k..2];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

auto sub remove_objects_partial() {
    list<Obj> ol = objs("x", 4);
    try {
        remove ol[f("a", 2), f("b", 0), k..1];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return ol;
}

auto sub delete_objects_partial() {
    list<Obj> ol = objs("y", 4);
    try {
        delete ol[f("a", 3), f("b", 1), k..1];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return ol;
}

auto sub delete_objects_shared() {
    list<Obj> ol = objs("z", 3);
    Obj keep = ol[1];
    try {
        delete ol[f("a", 1), k..1];
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return (ol, keep.name);
}

# the exception leaves the function, which has no handler
sub remove_unhandled(reference<list<int>> m) {
    remove m[f("a", 1), k..2];
}

auto sub remove_partial_unhandled() {
    list<int> m = (0, 1, 2, 3, 4);
    try {
        remove_unhandled(\m);
    } catch (hash<ExceptionInfo> ex) {
        print("  caught " + ex.err + "\n");
    }
    return m;
}

sub main() {
    for (int i = 0; i < 3; ++i) {
        t("foreach list", \foreach_list());
        t("foreach break", \foreach_break());
        t("foreach continue", \foreach_continue());
        t("foreach body throws", \foreach_body_throws());
        t("foreach selector throws", \foreach_selector_throws());
        t("foreach end throws", \foreach_end_throws());
        t("foreach selector throws unhandled", \foreach_selector_throws_unhandled());
        t("foreach selector throws try", \foreach_selector_throws_try());
        t("foreach descending", \foreach_descending());
        t("foreach out of range", \foreach_out_of_range());
        t("foreach nested", \foreach_nested());
        t("foreach reassign", \foreach_reassign());
        t("foreach string", \foreach_string());
        t("foreach binary", \foreach_binary());
        t("foreach string throws", \foreach_string_throws());
        t("foreach hash seq", \foreach_hash_seq());
        t("foreach nothing seq", \foreach_nothing_seq());
        t("foreach hash slice", \foreach_hash_slice());
        t("foreach objects", \foreach_objects());
        t("foreach objects break", \foreach_objects_break());
        t("foreach objects selector throws", \foreach_objects_selector_throws());
        t("foreach objects body throws", \foreach_objects_body_throws());
        t("map list", \map_list());
        t("map select", \map_select());
        t("map throws", \map_throws());
        t("map selector throws", \map_selector_throws());
        t("select throws", \select_throws());
        t("foldl throws", \foldl_throws());
        t("map string", \map_string());
        t("map binary", \map_binary());
        t("map hash seq", \map_hash_seq());
        t("map objects", \map_objects());
        t("map map", \map_map());
        t("select list", \select_list());
        t("foldl list", \foldl_list());
        t("foldr list", \foldr_list());
        t("hash map list", \hmap_list());
        t("take list", \take_list());
        t("first list", \first_list());
        t("any list", \any_list());
        t("count list", \count_list());
        t("foreach map pipeline", \foreach_map_pipeline());
        t("inert foreach", \inert_foreach());
        t("inert map", \inert_map());
        t("inert select", \inert_select());
        t("inert foldl", \inert_foldl());
        t("inert take", \inert_take());
        t("inert pipeline", \inert_pipeline());
        t("inert value", \inert_value());
        t("inert remove", \inert_remove());
        t("none map", \none_map());
        t("none select", \none_select());
        t("none take", \none_take());
        t("remove cleared bound", \remove_cleared_bound());
        t("pure map", \pure_map());
        t("pure map raises", \pure_map_raises());
        t("pure select", \pure_select());
        t("pure foldl", \pure_foldl());
        t("pure map mult", \pure_map_mult());
        t("pure map strings", \pure_map_strings());
        t("written foreach", \written_foreach());
        t("written foreach inc", \written_foreach_inc());
        t("written foreach nested var", \written_foreach_nested_var());
        t("written map", \written_map());
        t("written select", \written_select());
        t("written foldl", \written_foldl());
        t("stable arithmetic", \stable_arithmetic());
        t("written other local", \written_other_local());
        t("same list", \same_list());
        t("same string", \same_string());
        t("same binary", \same_binary());
        t("remove partial", \remove_partial());
        t("remove partial two", \remove_partial_two());
        t("remove partial range", \remove_partial_range());
        t("remove first throws", \remove_first_throws());
        t("remove ok", \remove_ok());
        t("remove string partial", \remove_string_partial());
        t("remove binary partial", \remove_binary_partial());
        t("remove nested partial", \remove_nested_partial());
        t("remove hash slice throws", \remove_hash_slice_throws());
        t("delete hash slice throws", \delete_hash_slice_throws());
        t("delete partial", \delete_partial());
        t("remove objects partial", \remove_objects_partial());
        t("delete objects partial", \delete_objects_partial());
        t("delete objects shared", \delete_objects_shared());
        t("remove partial unhandled", \remove_partial_unhandled());
    }
}
