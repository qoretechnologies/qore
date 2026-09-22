#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the ClosureWeakRefChecks checks and prints each result as "name=value"; the module is found in the module
# path given by the caller, so that the same checks run from source or from a compiled module

%modern

%requires ClosureWeakRefChecks

foreach hash<auto> i in (ClosureWeakRefChecks::get_results().pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
