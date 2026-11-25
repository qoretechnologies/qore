#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# prints the results of the cases of PseudoMethodValueTypeCases.ql (pseudo-method-value-type.qtest, qore#7363): those
# of PmvtFastCases with the argument "fast", of PmvtCases otherwise

%modern

%include PseudoMethodValueTypeCases.ql

foreach string line in (run_cases(ARGV[0] == "fast")) {
    printf("%s\n", line);
}
