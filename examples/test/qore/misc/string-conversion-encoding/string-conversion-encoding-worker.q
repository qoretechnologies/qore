#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints one result per line; the checks are
# included in this program, since code in a module loaded from source does not take the execution mode

%modern

%include string-conversion-encoding-checks.qi

foreach string line in (StringConversionEncodingChecks::get_results()) {
    printf("%s\n", line);
}
