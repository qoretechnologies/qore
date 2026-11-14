# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# removes and deletes integers outside the inline range from lists, hashes, objects and local variables and prints
# the results and how many heap integers it left behind (dbg_get_live_bigint_count()), which
# remove-boxed-integer-leak.qtest checks
%modern

class Holder {
    public {
        auto a;
        auto b;
        auto l;
    }
}

#! returns the values that the removals returned, as strings, so that no heap integer outlives the call
list<string> sub removals(int big) {
    list<auto> rv;

    # a list element: one index, a negative index, a list of indices, two selectors and a range
    list<auto> l = (big, big + 1, big + 2, big + 3);
    push rv, remove l[1];
    push rv, remove l[-1];
    l = (big, big + 1, big + 2, big + 3);
    push rv, remove l[(0, 2)];
    l = (big, big + 1, big + 2, big + 3);
    push rv, remove l[0, 2];
    l = (big, big + 1, big + 2, big + 3);
    push rv, remove l[1..2];
    l = (big, big + 1);
    delete l[0];
    delete l[0..1];
    push rv, l;

    # a hash member, a hash slice and a list element in a hash
    hash<auto> h = {"a": big, "b": big + 1, "l": (big, big + 1)};
    push rv, remove h.a;
    push rv, remove h.l[1];
    h.a = big;
    push rv, remove h{"a", "b"};
    h = {"a": big, "l": (big, big + 1)};
    delete h.a;
    delete h.l[0];
    push rv, h;

    # an object member, an object slice and a list element in an object
    Holder o();
    o.a = big;
    o.b = big + 1;
    o.l = (big, big + 1);
    push rv, remove o.a;
    push rv, remove o.l[0];
    o.a = big;
    push rv, remove o{"a", "b"};
    o.a = big;
    o.l = (big, big + 1);
    delete o.a;
    delete o.l[1];
    push rv, o.l;

    # a local variable
    auto x = big;
    push rv, remove x;
    x = big;
    delete x;
    push rv, x;

    return map sprintf("%y", $1), rv;
}

# no loop: a top-level loop keeps the last values of its temporary integers in the IR tier
*int live_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigint_count") : NOTHING;
list<string> result = removals((1 << 60) - 2);
result = removals((1 << 60) - 1);
result = removals(1 << 60);
printf("%s\n", result.join(" "));
if (exists live_before) {
    printf("live heap integers: %d\n", call_function("dbg_get_live_bigint_count") - live_before);
}
