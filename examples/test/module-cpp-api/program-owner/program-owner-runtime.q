# -*- mode: qore; indent-tabs-mode: nil -*-
# program-owner-runtime.q - child program of program-owner-teardown.qtest: loads tdpgmowner at runtime

# Copyright 2026 Qore Technologies, s.r.o.

%modern

# ARGV[0]: the code that the Program owned by tdpgmowner runs; ARGV[1]: "user" to load TdOwnedUser, which requires
# tdpgmowner
load_module("tdpgmowner");
call_function("TdPgmOwner::own", ARGV[0]);
if (ARGV[1] == "user") {
    load_module("TdOwnedUser");
}
print("main done\n");
