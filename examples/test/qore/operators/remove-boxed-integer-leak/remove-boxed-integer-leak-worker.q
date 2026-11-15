# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# removes and deletes integers outside the inline range and floats that a value holds in a node from lists, hashes,
# objects and local variables, reads and replaces variables that hold them, checks the results and prints how many
# heap integers and floats it left behind (dbg_get_live_bigint_count(), dbg_get_live_bigfloat_count()), which
# remove-boxed-integer-leak.qtest checks
%modern
%allow-weak-references

class Holder {
    public {
        auto a;
        auto b;
        auto l;
    }
}

#! the checks that failed
our list<string> mismatches = ();

sub check(string label, auto expected, auto actual) {
    if (expected !== actual) {
        push mismatches, sprintf("%s: expected %y, got %y", label, expected, actual);
    }
}

#! removes and deletes the four values of \a v, which a value holds in a node, from containers and variables
sub removals(string kind, list<auto> v) {
    # a list element: one index, a negative index, a list of indices, two selectors and a range
    list<auto> l = v;
    check(kind + " l[1]", v[1], remove l[1]);
    check(kind + " l[-1]", v[3], remove l[-1]);
    l = v;
    check(kind + " l[(0, 2)]", (v[0], v[2]), remove l[(0, 2)]);
    l = v;
    check(kind + " l[0, 2]", (v[0], v[2]), remove l[0, 2]);
    l = v;
    check(kind + " l[1..2]", (v[1], v[2]), remove l[1..2]);
    l = (v[0], v[1]);
    delete l[0];
    delete l[0..1];
    check(kind + " delete l", (), l);

    # a hash member, a hash slice and a list element in a hash
    hash<auto> h = {"a": v[0], "b": v[1], "l": (v[0], v[1])};
    check(kind + " h.a", v[0], remove h.a);
    check(kind + " h.l[1]", v[1], remove h.l[1]);
    h.a = v[0];
    check(kind + " h{a, b}", {"a": v[0], "b": v[1]}, remove h{"a", "b"});
    h = {"a": v[0], "l": (v[0], v[1])};
    delete h.a;
    delete h.l[0];
    check(kind + " delete h", {"l": (NOTHING, v[1])}, h);

    # an object member, an object slice and a list element in an object
    Holder o();
    o.a = v[0];
    o.b = v[1];
    o.l = (v[0], v[1]);
    check(kind + " o.a", v[0], remove o.a);
    check(kind + " o.l[0]", v[0], remove o.l[0]);
    o.a = v[0];
    check(kind + " o{a, b}", {"a": v[0], "b": v[1]}, remove o{"a", "b"});
    o.a = v[0];
    o.l = (v[0], v[1]);
    delete o.a;
    delete o.l[1];
    check(kind + " delete o", (v[0], NOTHING), o.l);

    # a local variable
    auto x = v[0];
    check(kind + " x", v[0], remove x);
    x = v[0];
    delete x;
    check(kind + " delete x", NOTHING, x);

    # an element, a member or a range of a local variable that holds the value: an index removes the value itself
    x = v[0];
    check(kind + " x[0]", v[0], remove x[0]);
    check(kind + " x[0] removed", NOTHING, x);
    x = v[0];
    delete x[0];
    check(kind + " delete x[0]", NOTHING, x);
    x = v[0];
    check(kind + " x.a", NOTHING, remove x.a);
    check(kind + " x.a kept", v[0], x);
    x = v[0];
    try {
        remove x[0..1];
        push mismatches, kind + " x[0..1]: no exception";
    } catch (hash<ExceptionInfo> ex) {
        check(kind + " x[0..1]", "ILLEGAL-EXPRESSION", ex.err);
    }
    check(kind + " x[0..1] kept", v[0], x);

    # an element of a value that a hash member, a list element or an object member holds
    h = {"a": v[0]};
    check(kind + " h.a[0]", v[0], remove h.a[0]);
    check(kind + " h.a[0] removed", {"a": NOTHING}, h);
    l = (v[0],);
    check(kind + " l[0][0]", v[0], remove l[0][0]);
    o.a = v[0];
    check(kind + " o.a[0]", v[0], remove o.a[0]);
    check(kind + " o.a[0] removed", NOTHING, o.a);
}

#! reads and replaces a variable that holds \a v, which a value holds in a node, through lvalue helpers
sub writes(string kind, auto v) {
    # a member or an element of the variable replaces the value with a hash or a list
    auto x = v;
    x.a = 1;
    check(kind + " x.a = 1", {"a": 1}, x);
    x = v;
    x[0] = 1;
    check(kind + " x[0] = 1", (1,), x);
    # a cast in an lvalue checks the value
    x = v;
    if (kind == "int") {
        cast<int>(x) += 0;
    } else {
        cast<float>(x) += 0.0;
    }
    check(kind + " cast", v, x);
    # a pseudo-method called through a reference
    x = v;
    reference<auto> r = \x;
    check(kind + " r.typeCode()", kind == "int" ? NT_INT : NT_FLOAT, r.typeCode());
}

#! weak assignments from locals that hold such values: a qcc-compiled program makes the container that an untyped
#! local holds unique first (qore_rt_unique_local_aot()), which read the value in a new node
sub weakAssignments(int big, float f) {
    int x = big;
    auto w := x;
    check("int x; w := x", big, w);
    float y = f;
    auto wf := y;
    check("float y; wf := y", f, wf);
    auto ax = big;
    auto aw := ax;
    check("auto ax = int; aw := ax", big, aw);
    auto ay = f;
    auto awf := ay;
    check("auto ay = float; awf := ay", f, awf);
}

sub run(int offset) {
    int big = (1 << 60) + offset;
    removals("int", (big, big + 1, big + 2, big + 3));
    writes("int", big);
    # a number result converted to the type of the lvalue
    softint si = big;
    si += 1n;
    check("softint += number", big + 1, si);
    # -2^1021 times these factors: finite negative doubles that a value holds in a node
    float f = -pow(2.0, 1021) * (1.0 + offset / 8.0);
    removals("float", (f, f * 1.25, f * 1.5, f * 1.75));
    writes("float", f);
    weakAssignments(big, f);
}

# no loop: a top-level loop keeps the last values of its temporary integers in the IR tier
*int ints_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigint_count") : NOTHING;
*int floats_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigfloat_count") : NOTHING;
run(0);
run(1);
run(2);
printf("mismatches: %y\n", mismatches);
if (exists ints_before) {
    printf("live heap integers: %d\n", call_function("dbg_get_live_bigint_count") - ints_before);
    printf("live heap floats: %d\n", call_function("dbg_get_live_bigfloat_count") - floats_before);
}
