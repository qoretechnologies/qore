#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# the child process of library-registry-origin.qtest (qore#6520)

# Copyright (C) 2026 Qore Technologies, s.r.o.

# args: <1: the registering Program is restricted (PO_NO_PROCESS_CONTROL)> <exec mode of the registering Program>
# the registering Program puts a call reference, a closure and an object into a library registry; then the
# initialization of another library module fires the registry

%modern
%requires RoLib

const Modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT};

bool restricted = ARGV[0] == "1";
Program p(PO_NEW_STYLE | (restricted ? PO_NO_PROCESS_CONTROL : 0));
p.setExecMode(Modes{ARGV[1] ?? "ast"});
p.loadModule("RoLib");
p.parse("sub reg() { RoLib::ro_register_ref(); RoLib::ro_register_closure(); RoLib::ro_hold(); }", "reg");
p.callFunction("reg");
load_module("RoFire");
printf("fired: %s\n", RoLib::ro_fired().join(","));
printf("methods: %s\n", RoLib::ro_methods().join(","));
printf("destroyed: %s\n", RoLib::ro_destroyed().join(","));
