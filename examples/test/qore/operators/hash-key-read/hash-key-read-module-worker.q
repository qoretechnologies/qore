#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# runs the checks from the HashKeyReadChecks module found in the module path given by the caller (the compiled
# module) and prints each result as "name=value"

%modern

%requires HashKeyReadChecks

foreach hash<auto> i in (HashKeyReadChecks::get_results().pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
