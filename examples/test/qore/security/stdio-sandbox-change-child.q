#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The child process of stdio-sandbox-change.qtest (qore#7670): sandboxed code that may use terminal I/O calls methods of
# the process-wide standard I/O objects; the result of each call is written to the file that is given as the argument
# (the calls can close or redirect the standard streams of this process, so they are made in a process of their own)

%modern
%no-child-restrictions

const SandboxedCode = "%modern
#! calls a method of a process-wide standard I/O object and returns the error code of its exception, or \"done\"
string sub call(string name, string method, list<auto> args) {
    object o = name == \"stdin\" ? stdin : (name == \"stdout\" ? stdout : stderr);
    try {
        call_object_method_args(o, method, args);
        return \"done\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

string out_path = ARGV[0];
bool sandboxed = ARGV[1] == "sandboxed";

# the code of the test: a Program that no sandbox governs, or one that a sandbox governs and that may use terminal I/O
Program p(PO_NO_CHILD_PO_RESTRICTIONS);
if (sandboxed) {
    SandboxManager sm();
    p.setSandboxManager(sm);
}
p.parse(SandboxedCode, "code");

File devnull();
devnull.open2("/dev/null", O_WRONLY);

# the methods that change the state of the object, with arguments that they accept
const Changing = (
    ("setCharset", ("ISO-8859-2",)),
    ("setEncoding", ("ISO-8859-2",)),
    ("setEncodingErrors", ()),
    ("setPos", (0,)),
    ("open", ("/dev/null",)),
    ("open2", ("/dev/null",)),
    ("lock", ()),
    ("lockBlocking", ()),
    ("chown", (-1, -1)),
    ("setEventQueue", ()),
    ("close", ()),
);

# the methods that read or write through the object, as the terminal I/O permission allows
const Through = (
    ("write", ("",)),
    ("print", ("",)),
    ("sync", ()),
    ("isOpen", ()),
    ("getCharset", ()),
);

list<string> lines;
foreach string name in ("stdin", "stdout", "stderr") {
    foreach list<auto> m in (Changing) {
        lines += sprintf("%s %s %s", name, m[0], p.callFunction("call", name, m[0], m[1]));
    }
    # redirect takes a file of its own
    lines += sprintf("%s redirect %s", name, p.callFunction("call", name, "redirect", (devnull,)));
    foreach list<auto> m in (Through) {
        # a method that the class of the object does not have is not a result
        if (!(name == "stdin" && (m[0] == "write" || m[0] == "print" || m[0] == "sync"))) {
            lines += sprintf("%s %s %s", name, m[0], p.callFunction("call", name, m[0], m[1]));
        }
    }
}

File f();
f.open2(out_path, O_CREAT | O_WRONLY | O_TRUNC);
f.write(join("\n", lines) + "\n");
f.close();
