#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Worker for loop-jump-temp-scope.qtest: runs the shared checks and prints "name=value" lines.  Run once per
# execution mode by the test.

%modern
%require-types
%enable-all-warnings

%include loop-jump-temp-scope-checks.qi

# growth checks (0 in every mode after the fix)
foreach hash<auto> i in (ljts_growth_results().pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}

# a temp held across the jump is released when the jump leaves its scope, not kept until frame exit: in the IR
# interpreter and AST this is 0 right after the loop; native code may hold the last one in its slot until the next
# overwrite, so the permanent check (below) is what holds in every mode
printf("switch-continue-pending=%d\n", ljts_switch_continue_pending());

# no permanent leak in any mode: once the function that ran the loops has returned, every object it created has
# been destroyed
printf("permanent-leak=%d\n", ljts_created - ljts_destroyed);
