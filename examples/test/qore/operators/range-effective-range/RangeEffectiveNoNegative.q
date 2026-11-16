#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# checks the effective range of the range operator (x[m..n]) without negative offsets (qore#6306); prints the number
# of checks and of failures, which must be the line in RangeEffectiveNoNegative.expected; as code without negative
# offsets cannot use %modern, it runs in the AST interpreter only

%new-style
%require-types
%strict-args
%no-negative-offsets

const NegativeOffsets = False;

%include RangeEffectiveChecks.qc
