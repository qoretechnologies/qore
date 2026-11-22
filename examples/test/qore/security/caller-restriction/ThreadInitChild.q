#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# a check of caller-restriction-hand-offs.qtest: the thread initialization code that library code sets for its own
# Program runs for the code that set it, with no Program of a caller on the stack: ARGV[0] is the caller that sets it,
# ARGV[1] the caller that starts the thread ("r" restricted from the file system, or "u"); prints whether the code
# ran restricted

%modern
%no-child-restrictions
%requires HandOffProbe

const Source = "%modern
%requires HandOffProbe
nothing sub set() { HandOffProbe::set_init(); }
bool sub spawn() { return HandOffProbe::spawn(); }
";

Program r(PO_NEW_STYLE | PO_NO_FILESYSTEM);
r.parse(Source, "restricted");
Program u(PO_NEW_STYLE);
u.parse(Source, "unrestricted");
hash<string, Program> callers = {"r": r, "u": u};
callers{ARGV[0]}.callFunction("set");
printf("%y\n", callers{ARGV[1]}.callFunction("spawn"));
