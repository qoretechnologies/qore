#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# a child process of program-tlpd-switch.qtest: loads the tlpdprobe test module into another Program at run time
# (qore#6537); the module's namespace initialization calls the Program's function tlpdprobe_on_load(), which reads
# the Program's variable and writes "loaded"
#
# usage: program-tlpd-load-child.q
#
# prints "result: <value>" with the value that the function read, as the module recorded it (the value as a string,
# "NOTHING" for no value, or "error: <err>"), then "owner: <value>" with the value of the variable that the Program's
# own code reads
#
# This Program does not load the module before the other Program does: the other Program would get the module from
# it when it is created, and the load would not initialize the module's namespace in it

%modern

const OwnerCode = "%new-style\n"
    # a top-level variable that a closure captures is closure-bound
    "string owner_tl = \"owner\";\n"
    "code owner_setter = nothing sub (string v) { owner_tl = v; };\n"
    "auto sub read_tl() { return owner_tl; }\n"
    # called by the tlpdprobe module when it is loaded into the Program: reads and writes the variable
    "auto sub tlpdprobe_on_load() { auto old = owner_tl; owner_tl = \"loaded\"; return old; }\n";

Program owner(get_parse_options());
owner.parse(OwnerCode, "owner");
owner.run();

try {
    owner.loadModule("tlpdprobe");
    # the module's functions are called by name: this Program loads it only now
    load_module("tlpdprobe");
    printf("result: %s\n", call_function("TlpdProbe::lastCommandResult") ?? "<none>");
} catch (hash<ExceptionInfo> ex) {
    printf("error: %s: %s\n", ex.err, ex.desc);
}
printf("owner: %y\n", owner.callFunction("read_tl"));
