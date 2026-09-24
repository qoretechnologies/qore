#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints each result as "name=value"; the
# checks are included in this program, since code in a module loaded from source does not take the execution mode

%modern

%include lvalue-assign-ownership-checks.qi

foreach hash<auto> i in (LValueAssignOwnership::get_results().pairIterator()) {
    printf("%s=%y\n", i.key, i.value);
}
