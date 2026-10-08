#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the BuiltinExceptionResultChecks module found in the module path given by the caller (the
# compiled module) and prints one result per line

%modern

%requires BuiltinExceptionResultChecks

foreach string line in (BuiltinExceptionResultChecks::get_results() + BuiltinExceptionResultChecks::get_results()) {
    printf("%s\n", line);
}
