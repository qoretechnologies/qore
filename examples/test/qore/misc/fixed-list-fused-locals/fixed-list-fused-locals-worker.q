#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# locals read by fused instructions (a loop bound, an int += local, an increment) keep their values when a fixed list
# in the same function is scalarized

%modern

# the loop bound is a local that holds a constant (BranchIfLtLocalInt)
int sub local_bound(int value) {
    int bound = 3;
    int sum = 0;
    for (int i = 0; i < bound; ++i) {
        list<int> pair = (value, value + 1);
        sum += pair[0];
    }
    return sum;
}

# the bound is a local that holds a constant and the list element is added with a local (AddAssignLocalInt)
int sub local_bound_add(int value) {
    int bound = 4;
    int step = 2;
    int sum = 0;
    for (int i = 0; i < bound; ++i) {
        list<int> pair = (value, value + 1);
        sum += pair[1];
        sum += step;
    }
    return sum;
}

# a constant local that is incremented (IncrementLocalInt) and then the bound
int sub incremented_bound(int value) {
    int bound = 2;
    ++bound;
    int sum = 0;
    for (int i = 0; i < bound; ++i) {
        list<int> pair = (value, value * 2);
        sum += pair[1];
    }
    return sum;
}

# the bound is the first local of the comparison
int sub reversed_bound(int value) {
    int top = 5;
    int sum = 0;
    for (int i = 10; top < i; --i) {
        list<int> pair = (value, value + 1);
        sum += pair[0];
    }
    return sum;
}

const Expected = {
    "local_bound": (-39, 3, 15),
    "local_bound_add": (-40, 16, 32),
    "incremented_bound": (-78, 6, 30),
    "reversed_bound": (-65, 5, 25),
};

foreach hash<auto> i in (Expected.pairIterator()) {
    list<int> results = map call_function(i.key, $1), (-13, 1, 5);
    # each function runs several times, so that the tiers that compile a function after calls compile it
    for (int n = 0; n < 3; ++n) {
        list<int> again = map call_function(i.key, $1), (-13, 1, 5);
        if (again != results) {
            results = again;
            break;
        }
    }
    printf("%s %s: %y\n", results == i.value ? "PASS" : "FAIL", i.key, results);
}
