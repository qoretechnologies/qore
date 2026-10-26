#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# calls, from a function of a child Program, a module function that calls another one with a constant body: the
# JIT compiles the child Program's function in one batch with both module functions, which belong to the module's
# Program

%modern

Program p(PO_NEW_STYLE);
p.loadModule(get_script_dir() + "/CrossProgramSummary.qm");
p.parse("int sub test0() { return cps_outer(); }", "child");
printf("%d\n", p.callFunction("test0"));
