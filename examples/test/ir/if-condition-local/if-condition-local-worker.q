#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Worker for if-condition-local.qtest: runs the shared checks and prints "name=events" lines.  Run once per
# execution mode by the test.

%modern
%require-types
%enable-all-warnings

%include if-condition-local-checks.qi

foreach hash<auto> i in (icl_results().pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
