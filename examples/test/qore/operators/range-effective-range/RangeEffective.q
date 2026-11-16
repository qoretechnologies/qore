#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# checks the effective range of the range operator (x[m..n]) with negative offsets (qore#6306); prints the number of
# checks and of failures, which must be the line in RangeEffective.expected in every execution mode

%modern

const NegativeOffsets = True;

%include RangeEffectiveChecks.qc
