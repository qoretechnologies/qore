#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the LValueOperandTempsChecks module found in the module path given by the caller (the
# compiled module) and prints each result as "name=value"

%modern

%requires LValueOperandTempsChecks

foreach hash<auto> i in (LValueOperandTemps::get_results().pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
