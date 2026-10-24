#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints each result as "name=value"; the
# checks are included in this program, since code in a module loaded from source does not take the execution mode;
# with the argument "call-references", the checks of call references are run as well

%modern
%allow-weak-references

%include weak-object-read-checks.qi

bool call_references = ARGV[0] == "call-references";

# Repeat so ExecModeHarness's thresholds exercise the AST, IR and native tiers in tiered mode.
hash<string, int> results = {};
for (int run = 0; run < 3; ++run) {
    hash<string, int> run_results = WeakObjectReadChecks::get_results();
    if (call_references) {
        run_results += WeakObjectReadChecks::get_call_reference_results();
    }
    foreach hash<auto> i in (run_results.pairIterator()) {
        results{i.key} = (results{i.key} ?? 1) & i.value;
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
