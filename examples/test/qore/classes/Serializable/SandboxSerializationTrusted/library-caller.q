#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# run by SandboxSerializationTrusted.qtest with the directory of the module in the process-wide module path
# (QORE_MODULE_DIR), so that the module is library code: the code of its functions acts for its caller, the code of its
# classes acts for itself (qore#7146)

%modern

%requires SandboxSerializationTrustedLib

Program p(PO_NEW_STYLE | PO_STRICT_ARGS | PO_REQUIRE_TYPES);
p.setSandboxManager(new SandboxManager());
p.parse("%requires SandboxSerializationTrustedLib\n"
    "string sub lib_function() { return st_lib_round_trip(); }\n"
    "string sub lib_class() { return StLibCache::roundTrip(); }\n", "sandboxed");
printf("function: %s\n", p.callFunction("lib_function"));
printf("class: %s\n", p.callFunction("lib_class"));
printf("host: %s\n", st_lib_round_trip());
