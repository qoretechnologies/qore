#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints each result as "name=value"; with an
# argument ("arg" or "target"), runs that exception check instead; the checks are included in this program, since
# code in a module loaded from source does not take the execution mode

%modern

%include background-thread-eval-checks.qi

if (ARGV[0]) {
    BackgroundThreadEvalChecks::exception_check(ARGV[0]);
    return;
}

# the checks run three times, so that with tiered execution they also run as native code; every round must give the
# same results
hash<string, string> results = BackgroundThreadEvalChecks::get_results();
for (int i = 0; i < 2; ++i) {
    hash<string, string> round = BackgroundThreadEvalChecks::get_results();
    foreach hash<auto> r in (round.pairIterator()) {
        if (results{r.key} != r.value) {
            results{r.key} = sprintf("round %d differs: %s", i + 2, r.value);
        }
    }
}
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
