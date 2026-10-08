#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the ExpressionResultTypeChecks module found in the module path given by the caller (the compiled
# module) and prints one result per line

%modern

%requires ExpressionResultTypeChecks

foreach string line in (ExpressionResultTypeChecks::get_results()) {
    printf("%s\n", line);
}
