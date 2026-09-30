#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# runs the checks in the execution mode given on the command line and prints each result as "name=value"; the
# checks are included in this program, since code in a module loaded from source does not take the execution mode.
# The checks run three times, so that a function the JIT compiles on its first call runs native on the later ones,
# and every run must report the same results.

%modern

%include lvalue-operand-temps-checks.qi

hash<string, auto> first = LValueOperandTemps::get_results();
for (int i = 1; i < 3; ++i) {
    hash<string, auto> again = LValueOperandTemps::get_results();
    foreach string key in (keys first) {
        if (again{key} !== first{key}) {
            printf("%s=%y (run %d: %y)\n", key, first{key}, i + 1, again{key});
            exit(1);
        }
    }
}
foreach hash<auto> i in (first.pairIterator()) {
    printf("%s=%s\n", i.key, i.value);
}
