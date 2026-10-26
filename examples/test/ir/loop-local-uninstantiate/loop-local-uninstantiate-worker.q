#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Worker for loop-local-uninstantiate.qtest: runs the shared checks and prints "name=value" lines.  Run once per
# execution mode by the test.

%modern
%require-types
%enable-all-warnings

%include loop-local-uninstantiate-checks.qi

foreach hash<auto> i in (llu_growth_results().pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
printf("branch-local-pending=%d\n", llu_branch_local_pending());

# nothing the checks created outlives them
printf("permanent-leak=%d\n", LluCounted::created - LluCounted::destroyed);
