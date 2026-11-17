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
auto sub pos_float(auto f) { return +f; }
# the inline floats whose negation needs a heap float (its encoding collides with a value tag: -(2 ** 1021) and
# -(2 ** 1022)), the one just past that range (-(2 ** 1023)), and the edges that stay inline; compared as values, as
# the printed form of a float depends on the C library
list<bool> sub float_edges() {
    float a = pow(2.0, 1021);
    float b = pow(2.0, 1022);
    float c = pow(2.0, 1023);
    float neg_zero = neg_float(0.0);
    return (
        neg_float(a) == -a,
        neg_float(b) == -b,
        neg_float(c) == -c,
        neg_zero == 0.0,
        neg_float(@inf@) == -@inf@,
        pos_float(-1.5) == -1.5,
        pos_float(a) == a,
    );
}
# a global read and a call result as the operand (issue #6228); the hash member form is #6297 in the AST interpreter
our auto heap_global = 1 << 60;
auto sub bigi(int i) { return (1 << 62) + i; }
auto sub pos_global() { auto v = +heap_global; return v; }
auto sub neg_global() { auto v = -heap_global; return v; }
auto sub pos_call() { auto v = +bigi(12345); return v; }
auto sub neg_call() { auto v = -bigi(12345); return v; }
# a number and a date, which the runtime negates into new nodes (issue #6132); a memory checker shows a leak of them
auto sub neg_number() { auto n = 7n; code c = auto sub () { return -n; }; return c(); }
auto sub neg_date() { auto d = hours(1); code c = auto sub () { return -d; }; return c(); }

# the slot that keeps the result of the slow path for the rest of the function: released at an early return while it
# holds a value, a result kept while the same operation runs again, and the slots of recursive calls
auto sub early_return(auto x, int n) {
    foreach int i in (xrange(n)) {
        auto v = -x;
        if (i == 1) {
            return v;
        }
    }
}
list<auto> sub keep_results(auto x) {
    list<auto> l = ();
    for (int i = 0; i < 3; ++i) {
        l += -x;
    }
    return l;
}
auto sub recurse(auto x, int depth) {
    return depth ? -recurse(x, depth - 1) : -x;
}
sub check_slots() {
    auto x = 1 << 60;
    if (early_return(x, 3) != -x) {
        throw "SLOT-ERROR", "early return";
    }
    if (keep_results(x) != (-x, -x, -x)) {
        throw "SLOT-ERROR", "kept results";
    }
    # an even depth gives -x
    if (recurse(x, 4) != -x) {
        throw "SLOT-ERROR", "recursion";
    }
}

list<auto> sub apply() {
    check_slots();
    return (neg_heap(), pos_heap(), neg_edge(), neg_arg(1 << 60), neg_closure(), neg_assigned(), neg_argument(),
        neg_discarded(), neg_try(), pos_global(), neg_global(), pos_call(), neg_call());
}

*int ints_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigint_count") : NOTHING;
*int floats_before = Option::HAVE_LIBRARY_DEBUGGING ? call_function("dbg_get_live_bigfloat_count") : NOTHING;
list<auto> result;
list<float> kept_floats;
for (int i = 0; i < 3; ++i) {
    result = apply();
    # a NaN operand: the result is a float compared by its string form
    result += (neg_float(@nan@).toString(), neg_float(-1.5), neg_number(), neg_date());
    result += float_edges();
    # the heap floats of the colliding negations, kept (not printed, as their form depends on the C library)
    kept_floats = (neg_float(pow(2.0, 1021)), neg_float(pow(2.0, 1022)));
}
printf("%y\n", result);
if (exists ints_before) {
    # the result list holds 11 heap integers
    printf("live heap integers: %d\n", call_function("dbg_get_live_bigint_count") - ints_before - 11);
    # kept_floats holds 2 heap floats
    printf("live heap floats: %d\n", call_function("dbg_get_live_bigfloat_count") - floats_before - 2);
}
