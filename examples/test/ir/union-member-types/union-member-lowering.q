#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# lowered with --ir-dump by union-member-types.qtest: a member of a union that returns only an int is read with an
# int access, a member of a union that can return another type is not

%modern

hash<string, union<int, *int>> one = {"x": 2};
printf("%y\n", one.x);
hash<string, union<int, float>> two = {"x": 2.5};
printf("%y\n", two.x);
