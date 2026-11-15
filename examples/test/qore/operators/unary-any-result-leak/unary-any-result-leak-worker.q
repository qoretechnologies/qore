# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# applies the unary minus and plus operators to untyped values held in heap integers (outside the inline 48-bit
# range), to the smallest inline integer, whose negation needs a heap integer, and to a float that needs a heap
# float, in every position the result can be used in, and prints the results and how many heap integers and floats
# it left behind (dbg_get_live_bigint_count(), dbg_get_live_bigfloat_count()), which unary-any-result-leak.qtest
# checks
%modern

auto sub neg_heap() { auto x = 1 << 60; return -x; }
auto sub pos_heap() { auto x = 1 << 60; return +x; }
auto sub neg_edge() { auto x = -140737488355328; return -x; }
auto sub neg_arg(auto x) { return -x; }
auto sub neg_closure() { auto x = 1 << 60; code c = auto sub () { return -x; }; return c(); }
auto sub neg_assigned() { auto x = 1 << 60; auto y = -x; return y; }
string sub neg_argument() { auto x = 1 << 60; return sprintf("%d", -x); }
int sub neg_discarded() {
    auto x = 1 << 60;
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        @discard(-x);
        @discard(+x);
        ++n;
    }
    return n;
}
# a try block makes the operation an invoke, which released its result already
auto sub neg_try() {
    auto x = 1 << 60;
    try {
        return -x;
    } catch (hash<ExceptionInfo> ex) {
        rethrow;
    }
}
auto sub neg_float(auto f) { return -f; }
# a number and a date, which the runtime negates into new nodes (issue #6132); a memory checker shows a leak of them
auto sub neg_number() { auto n = 7n; code c = auto sub () { return -n; }; return c(); }
auto sub neg_date() { auto d = hours(1); code c = auto sub () { return -d; }; return c(); }

list<auto> sub apply() {
    return (neg_heap(), pos_heap(), neg_edge(), neg_arg(1 << 60), neg_closure(), neg_assigned(), neg_argument(),
        neg_discarded(), neg_try());
}

*int ints_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigint_count") : NOTHING;
*int floats_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigfloat_count") : NOTHING;
list<auto> result;
for (int i = 0; i < 3; ++i) {
    result = apply();
    # a NaN operand: the result is a float compared by its string form
    result += (neg_float(@nan@).toString(), neg_float(-1.5), neg_number(), neg_date());
}
printf("%y\n", result);
if (exists ints_before) {
    # the result list holds 7 heap integers
    printf("live heap integers: %d\n", call_function("dbg_get_live_bigint_count") - ints_before - 7);
    printf("live heap floats: %d\n", call_function("dbg_get_live_bigfloat_count") - floats_before);
}
