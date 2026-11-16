#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the worker of unhandled-exception-report.qtest: starts ARGV[0] threads that raise an unhandled exception each at the
# same time, after all of them have started, so that their reports are written to standard error at the same time

%modern

sub thr(int i, Counter started, Counter go) {
    started.dec();
    go.waitForZero();
    throw sprintf("REPORT-%d", i), sprintf("the description of the exception of thread %d", i);
}

int threads = ARGV[0].toInt();
Counter started(threads);
Counter go(1);
for (int i = 0; i < threads; ++i) {
    background thr(i, started, go);
}
started.waitForZero();
go.dec();
