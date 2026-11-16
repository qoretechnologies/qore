#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The worker of assert-message-exception.qtest: the message of a failed @assert is made before the assertion error is
# raised, and an exception raised while it is made goes to the handler of the try block around the @assert, as with
# the AST interpreter; it must be run with --enable-debug, as an @assert does nothing otherwise.

%modern

#! a format argument that cannot be converted to the format's encoding raises ENCODING-CONVERSION-ERROR
string sub format_arg(string s) {
    try {
        @assert(False, "message %s", s);
        return "not raised";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! a message that is not formatted keeps its encoding: the assertion error is raised
string sub plain_message(string s) {
    try {
        @assert(False, s);
        return "not raised";
    } catch (hash<ExceptionInfo> ex) {
        return "caught " + ex.err;
    }
}

#! the message of no type is formatted as the arguments of sprintf() if it is a list
string sub list_message(auto m) {
    try {
        @assert(False, m);
        return "not raised";
    } catch (hash<ExceptionInfo> ex) {
        return sprintf("caught %s: %s", ex.err, ex.desc);
    }
}

#! the handler is in a loop
string sub format_arg_loop(string s) {
    list<string> rv = ();
    for (int i = 0; i < 2; ++i) {
        try {
            @assert(False, "message %s", s);
        } catch (hash<ExceptionInfo> ex) {
            rv += sprintf("%d:%s", i, ex.err);
        }
    }
    return rv.join(" ");
}

#! no handler in the function: the exception leaves it
string sub format_arg_out(string s) {
    @assert(False, "message %s", s);
    return "not raised";
}

#! runs a case; an exception that leaves the case is reported as escaped
sub run(string label, code c) {
    try {
        printf("%s: %s\n", label, c());
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: escaped %s\n", label, ex.err);
    }
}

string invalid = binary_to_string(<ff>, "ISO-2022-JP");
# each round calls every case again, so that the tiered and JIT modes run them as native code too
for (int i = 0; i < 3; ++i) {
    printf("round %d\n", i);
    run("format arg", auto sub () { return format_arg(invalid); });
    run("format arg ok", auto sub () { return format_arg("x"); });
    run("plain message", auto sub () { return plain_message(invalid); });
    run("list message", auto sub () { return list_message(("a %d", 1)); });
    run("int message", auto sub () { return list_message(2); });
    run("bool message", auto sub () { return list_message(True); });
    run("hash message", auto sub () { return list_message({"a": 1}); });
    run("nothing message", auto sub () { return list_message(); });
    run("null message", auto sub () { return list_message(NULL); });
    run("date message", auto sub () { return list_message(2026-01-02T03:04:05Z); });
    run("float message", auto sub () { return list_message(1.5); });
    run("format arg loop", auto sub () { return format_arg_loop(invalid); });
    run("format arg out", auto sub () { return format_arg_out(invalid); });
}
