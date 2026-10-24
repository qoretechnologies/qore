#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the WeakContainerReadChecks module found in the module path given by the caller (the compiled
# module) and prints each result as "name=value"

%modern

%requires WeakContainerReadChecks

hash<string, string> sub all_results() {
    return WeakContainerReadChecks::get_results() + WeakContainerReadChecks::get_lifetime_results()
        + WeakContainerReadChecks::get_loop_results();
}

# Repeat so ExecModeHarness's thresholds exercise the AST, IR and native tiers in tiered mode; every run must give
# the same results
hash<string, string> results = all_results();
for (int run = 1; run < 3; ++run) {
    foreach hash<auto> i in (all_results().pairIterator()) {
        if (results{i.key} != i.value) {
            results{i.key} = sprintf("run %d gave %s after %s", run, i.value, results{i.key});
        }
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
