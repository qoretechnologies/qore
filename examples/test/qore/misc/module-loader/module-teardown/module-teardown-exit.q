# -*- mode: qore; indent-tabs-mode: nil -*-
# module-teardown-exit.q - child program of module-teardown.qtest: an unhandled exception at teardown

# Copyright 2026 Qore Technologies, s.r.o.

%modern

# ARGV[0]: "module" to load a module whose teardown throws, "program" for a global variable whose destructor throws
# when the Program is deleted, "none" for neither; ARGV[1]: the exit status to return, if any
class ProgramThrower {
    destructor() {
        throw "TD-PROGRAM-TEARDOWN-ERROR", "thrown when the Program is deleted";
    }
}

our *ProgramThrower thrower;

if (ARGV[0] == "module") {
    load_module("TdThrowing");
} else if (ARGV[0] == "program") {
    thrower = new ProgramThrower();
}
print("main done\n");
# the return value of the top-level code is the exit status
return ARGV[1] ? ARGV[1].toInt() : 0;
