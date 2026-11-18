#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# a probe of UnassignedTypedLvalues.qtest (qore#5937): locals that keep their values when the code of a
# loop, of an iterated expression or of an on_exit block runs again or later; its output in every execution mode and
# as a qcc executable must be that of the AST interpreter

%modern

#! a local declared in the initialization of a for statement keeps its value across iterations
int sub forInit() {
    for (int i = 0, int x; i < 3; ++i) {
        if (i == 1) {
            return x;
        }
        x = 7;
    }
    return -1;
}

int sub id(int a) {
    return a;
}

#! a local declared in the expression of a map operator keeps its value for the next elements
list<auto> sub mapDecl() {
    return map (exists (int y) ? id(y) : (y = $1)), (1, 2);
}

#! an on_exit block runs when its block exits, after the delete that follows it
int sub onExitAfterDelete() {
    int x = 0;
    {
        on_exit x = 1;
        delete x;
    }
    return x;
}

sub show(string l, code c) {
    try {
        auto v = c();
        printf("%s => %y %s\n", l, v, v.fullType());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s => %s\n", l, ex.err);
    }
}

show("for init", \forInit());
show("map declaration", \mapDecl());
show("on_exit after delete", \onExitAfterDelete());
