#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the ReturnNoValueChecks module found in the module path given by the caller (the compiled
# module) and prints each result as "name=value"

%modern

%requires ReturnNoValueChecks

# Repeat so ExecModeHarness's thresholds exercise the AST, IR and native tiers in tiered mode; every run must give
# the same results
hash<string, string> results = ReturnNoValueChecks::get_results();
for (int run = 1; run < 3; ++run) {
    foreach hash<auto> i in (ReturnNoValueChecks::get_results().pairIterator()) {
        if (results{i.key} != i.value) {
            results{i.key} = sprintf("run %d gave %s after %s", run, i.value, results{i.key});
        }
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
