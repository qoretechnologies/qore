#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the ExceptionTempScopeChecks module found in the module path given by the caller (the compiled
# module) and prints each result as "name=value"

%modern

%requires ExceptionTempScopeChecks

hash<string, int> results = ExceptionTempScopeChecks::get_results();
foreach hash<auto> i in (results.pairIterator()) {
    printf("%s=%d\n", i.key, i.value);
}
