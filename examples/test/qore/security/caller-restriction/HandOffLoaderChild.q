#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a check of caller-restriction-hand-offs.qtest: a Program that may not use uncontrolled APIs loads the library modules
# first, whose Programs then have its options; then a restricted and an unrestricted caller use the process-wide
# controller, the restricted one first (issue #6996); prints one line per caller

%modern
%no-child-restrictions

Program loader(PO_NEW_STYLE | PO_NO_UNCONTROLLED_APIS);
loader.parse("%modern\n%requires HandOffProbe\n", "loader");

const Source = "%modern
%requires HandOffProbe
bool sub run() {
    return HandOffProbe::global_task();
}
";

Program r(PO_NEW_STYLE | PO_NO_FILESYSTEM);
r.parse(Source, "restricted");
Program u(PO_NEW_STYLE);
u.parse(Source, "unrestricted");
printf("r: %y\n", r.callFunction("run"));
printf("u: %y\n", u.callFunction("run"));
