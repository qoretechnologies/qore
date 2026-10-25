#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks from the TemporaryLifetimeChecks module found in the module path given by the caller (the compiled
# module) and prints the trace of each case, the results of the loops and the weak reference reads

%modern

%requires TemporaryLifetimeChecks

TemporaryLifetimeChecks::print_results();
