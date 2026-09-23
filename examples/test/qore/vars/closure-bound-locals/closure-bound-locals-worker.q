#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the ClosureBoundLocalChecks checks and prints each result as "name=value"; the module is found in the
# module path given by the caller, so that the same checks run from source or from a compiled module

%modern

%requires ClosureBoundLocalChecks

hash<string, int> results = ClosureBoundLocalChecks::get_scan_counts()
    + ClosureBoundLocalChecks::get_scan_object_counts() + ClosureBoundLocalChecks::get_cycle_results()
    + ClosureBoundLocalChecks::get_thread_results() + ClosureBoundLocalChecks::get_removal_results();
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
