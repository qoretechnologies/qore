#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# while the module cleanup clears the module's data, a foreign thread calls module code through a Program that module
# code created, which is not a module's: the module's Program is closed to every thread but the one that runs the
# cleanup, so the call fails; module-code-cleanup.qtest checks the output

%modern
%requires CleanupMod

# the foreign thread can call module code before the cleanup
printf("another thread before the cleanup: %y\n", call_function("dbg_run_in_foreign_thread",
    sub () { CleanupMod::f(1); }));
CleanupMod::arm_cleanup_entry_check();
printf("done\n");
