#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints the trace of each case, the results of
# the loops and the weak reference reads; the checks are included in this program, since code in a module loaded
# from source does not take the execution mode

%modern
%allow-weak-references

%include temporary-lifetime-checks.qi

TemporaryLifetimeChecks::print_results();
