# -*- mode: qore; indent-tabs-mode: nil -*-
# teardown-order.q - child program of teardown-order.qtest: makes a module dependency cycle through a run-time load

# Copyright 2026 Qore Technologies, s.r.o.

# ARGV[0]: the module group (see teardown-order.qtest):
# - cyc: TdoCycC (requires TdoCycA) is loaded, then TdoCycA loads TdoCycD (requires TdoCycC) at run time
# - run, src, aot: Tdo<X>C (requires Tdo<X>B) is loaded, then Tdo<X>B loads Tdo<X>A (requires Tdo<X>C) at run time
# - dup: TdoDupB (requires TdoDupA, which requires TdoDupC) is loaded, then TdoDupC loads TdoDupB at run time, and
#   TdoDupB loads TdoDupA, which it requires already, at run time

%modern

switch (ARGV[0]) {
    case "cyc":
        load_module("TdoCycC");
        call_function("TdoCycA::load");
        break;
    case "run":
    case "src":
    case "aot": {
        string prefix = {"run": "TdoRun", "src": "TdoSrc", "aot": "TdoAot"}{ARGV[0]};
        load_module(prefix + "C");
        call_function(prefix + "B::load");
        break;
    }
    case "dup":
        load_module("TdoDupB");
        call_function("TdoDupC::load");
        call_function("TdoDupB::loadA");
        break;
    default:
        throw "TEARDOWN-ORDER-ERROR", sprintf("unknown module group %y", ARGV[0]);
}
print("main done\n");
