#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a member of an object local declared *BlpN that holds no value, modified in a background expression: the new thread
# raises the error that the same assignment raises outside of it, in every execution mode

%modern

class BlpN {
    public {
        int x = 0;
    }
}

sub t() {
    *BlpN o;
    try {
        o.x = 5;
    } catch (hash<ExceptionInfo> ex) {
        printf("outside: %s: %s\n", ex.err, ex.desc);
    }
    thread_join(background (o.x = 5));
}

t();
