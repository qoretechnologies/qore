# -*- mode: qore; indent-tabs-mode: nil -*-
# module-teardown-runtime.q - child program of module-teardown.qtest: loads the dependent test module at runtime

# Copyright 2026 Qore Technologies, s.r.o.

%modern

load_module("TdZzzUser");
# ARGV[0]: "cycle" to make an opaque cycle whose holder the base module owns
if (ARGV[0] == "cycle") {
    call_function("TdZzzUser::make_cycle");
}
print("main done\n");
