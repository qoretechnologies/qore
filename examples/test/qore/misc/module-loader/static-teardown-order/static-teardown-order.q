# -*- mode: qore; indent-tabs-mode: nil -*-
# static-teardown-order.q - child program of static-teardown-order.qtest: loads TsoZHolder and ends normally; with
# the argument "cycle", TsoZHolder then loads TsoZCyc, which requires it, at run time; with the argument "runtime", it
# loads TsoZRtHolder instead, which loads TsoAStatic at run time

# Copyright 2026 Qore Technologies, s.r.o.

%modern

if (ARGV[0] == "runtime") {
    load_module("TsoZRtHolder");
    call_function("TsoZRtHolder::init");
} else {
    load_module("TsoZHolder");
    call_function("TsoZHolder::touch");
    if (ARGV[0] == "cycle") {
        call_function("TsoZHolder::loadCycle");
        print("cycle loaded\n");
    }
}
print("main done\n");
