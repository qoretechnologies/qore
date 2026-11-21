#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# issues module commands that fail to the cppapiuser test module and prints the description of each exception, for
# module-command-error-encoding.qtest (issue #6741): "<case> <err> <base64 of the description in UTF-8> <its
# encoding> <valid>"; the non-ASCII text is made from bytes, as a string literal is in the process encoding

%modern
%requires cppapiuser

# "fail " and 300 "é" in UTF-8
binary long_command = <6661696c20>;
for (int i = 0; i < 300; ++i) {
    long_command += <c3a9>;
}

# the commands, made from bytes in the encoding given (ü: c3bc in UTF-8, fc in ISO-8859-1; €: e282ac; é: c3a9)
list<list<string>> cases = (
    ("ascii", "fail"),
    ("utf8-command", binary_to_string(<6661696c20c3bc>, "UTF-8")),
    ("iso-command", binary_to_string(<6661696c20fc>, "ISO-8859-1")),
    ("euro-command", binary_to_string(<6661696c20e282ac>, "UTF-8")),
    ("utf8-description", binary_to_string(<6661696c2d7574663820fc>, "ISO-8859-1")),
    ("long-command", binary_to_string(long_command, "UTF-8")),
    ("multi-line-command", binary_to_string(<6661696c20c3bc0a6e657874>, "UTF-8")),
);

foreach list<string> c in (cases) {
    try {
        Program::issueModuleCmd("cppapiuser", c[1]);
        printf("%s no exception\n", c[0]);
    } catch (hash<ExceptionInfo> ex) {
        string desc = ex.desc;
        string text;
        try {
            text = binary(convert_encoding(desc, "UTF-8")).toBase64();
        } catch (hash<ExceptionInfo> ex1) {
            # a description that is not valid in its encoding
            text = "invalid";
        }
        printf("%s %s %s %s %y\n", c[0], ex.err, text, desc.encoding(), desc.isEncodingValid());
    }
}
