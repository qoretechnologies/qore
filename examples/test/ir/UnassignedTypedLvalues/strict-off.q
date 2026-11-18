#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): the probe of strict.q with %no-strict-parse-errors: the error
# is raised at run time; its output in every execution mode and as a qcc executable must be that of the AST
# interpreter

%modern
# the unassigned value is deliberate: its run-time handling is tested
%no-strict-parse-errors

int sub get() {
    int x;
    return x;
}

try {
    printf("%y\n", get());
} catch (hash<ExceptionInfo> ex) {
    printf("%s\n", ex.err);
}
