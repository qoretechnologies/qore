#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints each result as "name=value"; the
# checks are included in this program, since code in a module loaded from source does not take the execution mode

%modern

%include map-result-type-checks.qi

# Repeat so ExecModeHarness's thresholds exercise the AST, IR and native tiers in tiered mode; every run must give
# the same results
hash<string, string> results = MapResultTypeChecks::get_results();
for (int run = 1; run < 3; ++run) {
    hash<string, string> run_results = MapResultTypeChecks::get_results();
    foreach hash<auto> i in (run_results.pairIterator()) {
        if (results{i.key} != i.value) {
            results{i.key} = sprintf("run %d gave %s after %s", run, i.value, results{i.key});
        }
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
