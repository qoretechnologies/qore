#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
#
# Worker for object-constant-locals.qtest: runs the shared checks and prints "name=value" lines.  Run once per
# execution mode by the test.

%modern
%require-types
%enable-all-warnings

%include object-constant-locals-checks.qi

foreach hash<auto> i in (ocl_results().pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
