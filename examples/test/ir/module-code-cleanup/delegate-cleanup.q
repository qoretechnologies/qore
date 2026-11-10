#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT

# while the module cleanup clears the module's data, the cleanup thread and a foreign thread hand work that calls
# module code to other threads (ThreadPool tasks and background threads): the module's Program is closed to every
# thread but the one that runs the cleanup and the threads running the work that it handed over, so only the work of
# the cleanup thread can call module code; module-code-cleanup.qtest checks the output

%modern
%requires CleanupMod

CleanupMod::arm_cleanup_delegate_check();
printf("done\n");
