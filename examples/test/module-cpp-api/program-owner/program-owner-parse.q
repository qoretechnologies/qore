# -*- mode: qore; indent-tabs-mode: nil -*-
# program-owner-parse.q - child program of program-owner-teardown.qtest: requires tdpgmowner when it is parsed

# Copyright 2026 Qore Technologies, s.r.o.

%modern

%requires tdpgmowner

# ARGV[0]: the code that the Program owned by tdpgmowner runs; ARGV[1]: "user" to load TdOwnedUser, which requires
# tdpgmowner
TdPgmOwner::own(ARGV[0]);
if (ARGV[1] == "user") {
    load_module("TdOwnedUser");
}
print("main done\n");
