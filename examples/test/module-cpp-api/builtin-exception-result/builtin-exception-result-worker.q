#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints one result per line; the checks are
# included in this program, since code in a module loaded from source does not take the execution mode

%modern
%requires cppapiuser

%include builtin-exception-result-checks.qi

# the checks run twice: when native compilation follows the first call of a function, the first pass runs with the IR
# interpreter and the second one as native code with --exec-mode=jit or tiered
foreach string line in (BuiltinExceptionResultChecks::get_results() + BuiltinExceptionResultChecks::get_results()) {
    printf("%s\n", line);
}
