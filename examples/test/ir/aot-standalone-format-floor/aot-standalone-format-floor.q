#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later

%modern

# a standalone program compiled by aot-standalone-format-floor.qtest
class Box<T> {
    public { T value; }
    constructor(T v) { value = v; }
}

auto sub readBox<T>(auto input) {
    union<Box<T>, ? super T> box = input;
    return box.value;
}

printf("standalone program ran: %d\n", readBox<int>(new Box<int>(42)));
