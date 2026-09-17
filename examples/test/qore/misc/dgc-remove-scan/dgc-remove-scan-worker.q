#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# runs the DgcRemoveScanChecks checks and prints each result as "name=value"; the module is found in the
# module path given by the caller, so that the same checks run from source or from a compiled module

%modern

%requires DgcRemoveScanChecks

hash<string, int> results = DgcRemoveScanChecks::get_scan_counts() + DgcRemoveScanChecks::get_cycle_results();
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
