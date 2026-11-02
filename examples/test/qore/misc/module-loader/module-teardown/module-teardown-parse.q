# -*- mode: qore; indent-tabs-mode: nil -*-
# module-teardown-parse.q - child program of module-teardown.qtest: requires the dependent test module when parsed

# Copyright 2026 Qore Technologies, s.r.o.

%modern
%requires TdZzzUser

# ARGV[0]: "cycle" to make an opaque cycle whose holder the base module owns
if (ARGV[0] == "cycle") {
    TdZzzUser::make_cycle();
}
print("main done\n");
