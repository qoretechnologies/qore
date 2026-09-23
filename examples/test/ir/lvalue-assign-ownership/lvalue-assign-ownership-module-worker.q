#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the LValueAssignOwnershipChecks module found in the module path given by the caller (the
# compiled module) and prints each result as "name=value"

%modern

%requires LValueAssignOwnershipChecks

foreach hash<auto> i in (LValueAssignOwnership::get_results().pairIterator()) {
    printf("%s=%y\n", i.key, i.value);
}
