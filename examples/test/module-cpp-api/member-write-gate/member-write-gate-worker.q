#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the MemberWriteGateChecks checks and prints each result as "name=value"; the module is found in the module
# path given by the caller, so that the same checks run from source or from a compiled module

%modern

%requires MemberWriteGateChecks

# Repeat so ExecModeHarness's thresholds exercise the AST, IR and native tiers in tiered mode.
hash<string, int> results = {};
for (int run = 0; run < 3; ++run) {
    foreach hash<auto> i in (MemberWriteGateChecks::get_results().pairIterator()) {
        results{i.key} = (results{i.key} ?? 1) & i.value;
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
