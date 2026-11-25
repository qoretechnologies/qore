#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# remove of a range with %broken-list-range, which %modern refuses, so that only the AST interpreter runs it: through
# a weak reference to a list, and of a copy of a list freed under a weak reference (qore#7306, qore#7163); see
# ../weak-reference-remove.qtest

%new-style
%require-types
%strict-args
%allow-weak-references
%broken-list-range
%no-child-restrictions

sub live() {
    list<auto!> src = (1, 2, 3, 4);
    list<auto!> wl;
    wl := src;
    printf("broken range remove: %y src %y\n", remove wl[2..1], src);
    printf("broken range past the end: %y src %y\n", remove wl[1..4], src);
}

sub freed_copy() {
    list<auto!> wl;
    {
        list<auto!> src = (1, 2, 3);
        wl := src;
    }
    list<auto!> c = wl;
    try {
        remove c[0];
    } catch (hash<ExceptionInfo> ex) {
        printf("freed copy remove c[0]: %s\n", ex.err);
    }
    try {
        printf("freed copy remove c[0..-1]: %y\n", remove c[0..-1]);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed copy remove c[0..-1]: %s\n", ex.err);
    }
    try {
        printf("freed weak remove wl[0..-1]: %y\n", remove wl[0..-1]);
    } catch (hash<ExceptionInfo> ex) {
        printf("freed weak remove wl[0..-1]: %s\n", ex.err);
    }
}

live();
freed_copy();
printf("end\n");
