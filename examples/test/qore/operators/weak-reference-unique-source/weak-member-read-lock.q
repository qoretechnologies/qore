# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: LGPL-2.1-or-later
%modern

class Holder {
    public {
        list<int> items;
        hash<string, int> counts;
    }
}

Holder obj();
push obj.items, 1;
obj.counts.x = 1;
# Hold one reader across the second ownership check, then release it. The fast path must defer to the ordinary
# lvalue when the member lock is shared, even though the container itself has only one strong owner.
foreach string member in (("items", "counts")) {
    foreach bool reader in ((False, True, False)) {
        printf("%y\n", dbg_try_unique_weak_member(obj, member, reader));
    }
}
