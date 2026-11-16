# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# the provider of the static members that deferred-static-consumer.qc changes

%modern
class DeferredStaticProvider {
    public {
        static int count = 1;
        static hash<auto> h = {"k": 1, "j": "removed", "d": "deleted"};
    }
}
