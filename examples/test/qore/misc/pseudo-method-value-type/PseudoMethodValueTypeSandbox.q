#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# runs the cases of PseudoMethodValueTypeCases.ql in a sandboxed Program and prints their results
# (pseudo-method-value-type.qtest, qore#7363): those of PmvtFastCases with the argument "fast", of PmvtCases otherwise

%modern

# no file system, network, process, thread control, terminal or external access in the Program
const SandboxOptions = PO_NO_FILESYSTEM | PO_NO_NETWORK
    | PO_NO_PROCESS_CONTROL | PO_NO_EXTERNAL_PROCESS | PO_NO_THREAD_CONTROL | PO_NO_TERMINAL_IO | PO_NO_GUI
    | PO_NO_EXTERNAL_INFO | PO_NO_DATABASE | PO_NO_MODULES | PO_NO_INHERIT_USER_FUNC_VARIANTS;

Program pgm(SandboxOptions);
pgm.parse(ReadOnlyFile::readTextFile(get_script_dir() + "/PseudoMethodValueTypeCases.ql"),
    "PseudoMethodValueTypeCases.ql");
foreach string line in (pgm.callFunction("run_cases", ARGV[0] == "fast")) {
    printf("%s\n", line);
}
