#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# module code sets a signal handler, which belongs to the module's Program: it runs in the module's Program, and it is
# removed when the module cleanup starts, before the cleanup clears the module's data, which the module checks with a
# debug-build function; module-code-cleanup.qtest checks the output

%modern
%requires CleanupMod

const Sig = SIGUSR1;

CleanupMod::set_handler(Sig);
kill(getpid(), Sig);
printf("handler ran in: %s\n", CleanupMod::wait_handler());
CleanupMod::arm_cleanup_check(Sig);
printf("done\n");
