#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the ExpressionResultTypeChecks module found in the module path given by the caller (the compiled
# module) and prints one result per line

%modern

%requires ExpressionResultTypeChecks

# the checks run twice: when native compilation follows the first call of a function, the first pass runs with the IR
# interpreter and the second one as native code with --exec-mode=jit or tiered
foreach string line in (ExpressionResultTypeChecks::get_results() + ExpressionResultTypeChecks::get_results()) {
    printf("%s\n", line);
}
