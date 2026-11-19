#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-

# Copyright 2026 Qore Technologies, s.r.o.

# the child process of program-tlpd-switch.qtest: runs code of another Program through the tlpdprobe test module
# (callEntered(), callSwitched(), callNested(), or a module command issued to the Program: command), and prints the
# result; "deleted" checks that an entry into a Program deleted while it is current fails (switchDeleteEnter())
#
# usage: program-tlpd-child.q <entered|switched|nested|command|deleted> <read|write>
#
# prints "result: <value>" with the value that the code returns (read) or the value of the variable after the code
# ran (write), and "error: <err>: <desc>" if the code raised an exception, then, for a write, "owner: <value>" with
# the value of the variable that the owner's own code reads; for a command, the result is the one that the command
# recorded: the value as a string ("NOTHING" for no value) or "error: <err>"; for "deleted", "entered: <bool>" and
# "error: <err>" with the result of the entry

%modern
%requires tlpdprobe

const OwnerCode = "%new-style\n"
    # a top-level variable that a closure captures is closure-bound
    "string owner_tl = \"owner\";\n"
    "code owner_setter = nothing sub (string v) { owner_tl = v; };\n"
    # the return type does not check the value, so the test checks it
    "auto sub read_tl() { return owner_tl; }\n"
    "nothing sub write_tl(string v) { owner_tl = v; }\n"
    "nothing sub write_default() { write_tl(\"written\"); }\n"
    "code sub reader() { return \\read_tl(); }\n"
    "code sub writer() { return nothing sub () { write_tl(\"written\"); }; }\n";

string how = ARGV[0] ?? "";
string access = ARGV[1] ?? "";
if ((how != "entered" && how != "switched" && how != "nested" && how != "command" && how != "deleted")
        || (access != "read" && access != "write") || (how == "deleted" && access != "read")) {
    stderr.printf("usage: %s <entered|switched|nested|command|deleted> <read|write>\n", get_script_name());
    exit(2);
}

# the owner has the parse options of this Program: a module command can be issued to another Program only if it has
# every restriction of the issuing code
Program owner(get_parse_options());
owner.parse(OwnerCode, "owner");
owner.run();

try {
    if (how == "command") {
        # the module command handler calls the function in the Program that the command is issued to
        owner.issueModuleCmd("tlpdprobe", "call " + (access == "read" ? "read_tl" : "write_default"));
        if (access == "read") {
            printf("result: %s\n", TlpdProbe::lastCommandResult() ?? "<none>");
        } else if (*string r = TlpdProbe::lastCommandResult()) {
            if (r =~ /^error: /) {
                printf("%s\n", r);
            }
        }
    } else if (how == "deleted") {
        # the thread has data in the Program
        owner.callFunction("read_tl");
        hash<auto> h = TlpdProbe::switchDeleteEnter(owner, sub () { delete owner; });
        printf("entered: %y\nerror: %s\n", h.entered, h.error ?? "<none>");
    } else {
        code c = owner.callFunction(access == "read" ? "reader" : "writer");
        auto v;
        switch (how) {
            case "entered": v = TlpdProbe::callEntered(owner, c); break;
            case "switched": v = TlpdProbe::callSwitched(owner, c); break;
            # more levels than the helpers keep in the thread data without an allocation
            case "nested": v = TlpdProbe::callNested(owner, 41, c); break;
        }
        if (access == "read") {
            printf("result: %y\n", v);
        }
    }
} catch (hash<ExceptionInfo> ex) {
    printf("error: %s: %s\n", ex.err, ex.desc);
}
if (access == "write") {
    printf("owner: %y\n", owner.callFunction("read_tl"));
}
