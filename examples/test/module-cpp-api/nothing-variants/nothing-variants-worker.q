#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# prints the variant each NothingVariantChecks call selects as "name=variant"; the module is found in the module path
# given by the caller, so that the same calls run from source or from a compiled module

%modern

%requires NothingVariantChecks

# repeat so ExecModeHarness's thresholds exercise the AST, IR and native tiers in tiered mode
hash<string, string> results;
for (int run = 0; run < 3; ++run) {
    hash<string, string> r = NothingVariantChecks::get_results();
    if (results && results != r) {
        printf("inconsistent=%y\n", r);
    }
    results = r;
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
