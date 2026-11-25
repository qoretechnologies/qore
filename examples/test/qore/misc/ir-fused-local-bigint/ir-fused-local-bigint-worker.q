#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# locals that fused int instructions read keep their values when they hold an int outside the immediate range of a
# value (above 2^47 in magnitude)

%modern

const Big = 140737488355330;

# the loop counter is large (the left operand of BranchIfLtLocalInt)
int sub counter_return(int limit) { for (int i = Big; i < limit; ++i) { return i; } return 0; }
int sub counter_break(int limit) { int r = 0; for (int i = Big; i < limit; ++i) { r += i; break; } return r; }
int sub counter_loop(int limit) { int n = 0; for (int i = Big; i < limit; ++i) { ++n; } return n; }

# the bound is large (the right operand)
int sub bound_loop(int start) { int bound = Big; int n = 0; for (int i = start; i < bound; ++i) { ++n; } return n; }

# a large negative counter
int sub negative_counter(int limit) { int n = 0; for (int i = -Big; i < limit; ++i) { ++n; } return n; }

# the target of an int += local is large (AddAssignLocalInt)
int sub add_assign_target(int step) {
    int sum = Big;
    for (int i = 0; i < 3; ++i) {
        sum += step;
    }
    return sum;
}

# the source of an int += local is large
int sub add_assign_source(int count) {
    int sum = 0;
    int step = Big;
    for (int i = 0; i < count; ++i) {
        sum += step;
    }
    return sum;
}

# a sum of the distances from a large counter (an int += of a value computed from it); the base gave 0
int sub wide_counter_sum(int count) {
    int sum = 0;
    for (int i = Big; i < Big + count; ++i) {
        sum += i - Big;
    }
    return sum;
}

# the same local is assigned a different large int in each iteration and read by the fused += as its source
int sub rewritten_wide_source(int count) {
    int total = 0;
    for (int i = 0; i < count; ++i) {
        int v = Big + i;
        total += v;
    }
    return total;
}

# the target of the fused += is a large int that each iteration replaces
int sub rewritten_wide_target(int count) {
    int last = 0;
    for (int i = 0; i < count; ++i) {
        int acc = Big * (i + 1);
        acc += i;
        last = acc;
    }
    return last;
}

# the counter and the bound are large parameters, the counter is advanced with += (AddAssignLocalInt and
# BranchIfLtLocalInt both on large locals): the base gave 0
int sub wide_param_sum(int start, int limit) {
    int s = 0;
    for (int i = start; i < limit; i += 1) {
        s = s + (i - start);
    }
    return s;
}
int sub wide_param_loop(int start) { return wide_param_sum(start, start + 20); }

const Checks = (
    ("counter_return", Big + 1, Big),
    ("counter_break", Big + 1, Big),
    ("counter_loop", Big + 3, 3),
    ("bound_loop", Big - 2, 2),
    ("negative_counter", -Big + 4, 4),
    ("add_assign_target", 5, Big + 15),
    ("add_assign_source", 3, Big * 3),
    ("wide_counter_sum", 20, 190),
    ("wide_param_loop", Big, 190),
    ("rewritten_wide_source", 20, Big * 20 + 190),
    ("rewritten_wide_target", 20, Big * 20 + 19),
);

foreach list<auto> c in (Checks) {
    # each function runs several times, so that the tiers that compile a function after calls compile it
    list<int> results = ();
    for (int n = 0; n < 4; ++n) {
        push results, call_function(c[0], c[1]);
    }
    bool ok = (select results, $1 != c[2]).empty();
    printf("%s %s: %y\n", ok ? "PASS" : "FAIL", c[0], results[0]);
}
