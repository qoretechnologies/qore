#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
#
# Worker for block-scope-unwind.qtest: the same nested-block unwind shape, but
# with the handler-bearing scope being the program's top-level block rather
# than a function body.  The throw is deliberately left unhandled, so this
# script prints its one result line on stdout and then exits non-zero with
# BOOM on stderr.

%modern

Mutex m();

on_error {
    try {
        AutoLock probe_lock(m);
        printf("top-level=RELEASED\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("top-level=STILL-HELD:%s\n", ex.err);
    }
}

{
    AutoLock al(m);
    throw "BOOM", "unhandled on purpose";
}
