# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# loads AotSectionBoundsMod from the module path and prints "loaded" or the error, then the peak resident memory and
# address space of the process in kB on Linux ("hwm: <kB>", "vm: <kB>", from /proc/self/status); run by
# aot-section-bounds.qtest (qore#6265)
%modern

try {
    load_module("AotSectionBoundsMod");
    printf("loaded\n");
} catch (hash<ExceptionInfo> ex) {
    printf("error: %s: %s\n", ex.err, ex.desc);
}

if (is_file("/proc/self/status")) {
    string status = ReadOnlyFile::readTextFile("/proc/self/status");
    *list<*string> m = (status =~ x/VmHWM:\s+([0-9]+) kB/);
    if (m) {
        printf("hwm: %s\n", m[0]);
    }
    m = (status =~ x/VmPeak:\s+([0-9]+) kB/);
    if (m) {
        printf("vm: %s\n", m[0]);
    }
}
