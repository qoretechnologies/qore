#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): a local that cannot have been assigned whose value is returned
# where NOTHING is not accepted: a parse error with %strict-parse-errors, which %modern implies; its output in every
# execution mode and as a qcc executable must be that of the AST interpreter

%modern

int sub get() {
    int x;
    return x;
}

printf("%y\n", get());
