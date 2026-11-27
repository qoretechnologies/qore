#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright 2026 Qore Technologies, s.r.o.
# loads the module at ARGV[0], which must fail, then the module at ARGV[1] (qore#7794)

%modern

try {
    load_module(ARGV[0]);
    printf("dup: loaded\n");
} catch (hash<ExceptionInfo> ex) {
    printf("dup: %s|%s|%d|%s\n", ex.err, basename(ex.file ?? ""), ex.line, ex.desc);
}

load_module(ARGV[1]);
printf("good: %d\n", call_function("DupDeclGood::get"));
