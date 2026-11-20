#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later

%modern

# a program compiled by aot-runtime-identity.qtest for this runtime and for another AOT runtime identity
class IdentityCounter {
    public {
        int value;
    }

    constructor(int value) {
        self.value = value;
    }

    int next() {
        return ++value;
    }
}

IdentityCounter c(41);
printf("identity program ran: %d\n", c.next());
