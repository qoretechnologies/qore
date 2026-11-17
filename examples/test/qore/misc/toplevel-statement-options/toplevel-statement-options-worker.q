#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# the checks of toplevel-statement-options.qtest (qore#6293): each top-level statement prints the parse options that
# it runs with, which are those of the statement itself

%modern
%no-child-restrictions

#! returns the code of a statement that prints whether it runs with two restrictions
string sub stmt(string label) {
    return sprintf("printf(\"%s=%%y %%y\\n\", Program::hasParseOption(\"no-filesystem\"), "
        "Program::hasParseOption(\"no-database\"));\n", label);
}

#! parses the files into one Program, runs its top-level code, and prints the options of the caller after it
sub run_case(string name, list<string> files) {
    Program p(PO_NEW_STYLE);
    try {
        int i = 0;
        foreach string code in (files) {
            p.parsePending(code, sprintf("%s-%d", name, ++i));
        }
        p.parseCommit();
        printf("%s:\n", name);
        p.run();
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", name, ex.err, ex.desc);
    }
    printf("after=%y\n", Program::hasParseOption("no-filesystem"));
}

run_case("restricted-second-file", ("%modern\n" + stmt("first"), "%modern\n%no-filesystem\n" + stmt("second")));
run_case("restricted-first-file", ("%modern\n%no-filesystem\n" + stmt("first"), "%modern\n" + stmt("second")));
run_case("directive-mid-file", ("%modern\n" + stmt("first") + "%no-filesystem\n" + stmt("second")
    + "%no-database\n" + stmt("third"),));
run_case("same-options", ("%modern\n%no-filesystem\n" + stmt("first") + stmt("second"),));
# a statement that leaves the top-level code: the statements after it do not run
run_case("return", ("%modern\n" + stmt("first") + "%no-filesystem\nif (True) { return; }\n" + stmt("second"),));
# an on_exit handler runs with the options of the statement that declares it
run_case("on_exit-before-directive", ("%modern\non_exit " + stmt("handler") + stmt("first") + "%no-filesystem\n"
    + stmt("second"),));
run_case("on_exit-after-directive", ("%modern\n" + stmt("first") + "%no-filesystem\non_exit " + stmt("handler")
    + "%no-database\n" + stmt("second"),));
run_case("on_exit-first-file", ("%modern\non_exit { " + stmt("handler") + "}\n" + stmt("first"),
    "%modern\n%no-filesystem\n" + stmt("second")));
run_case("on_exit-exception", ("%modern\non_exit " + stmt("handler") + stmt("first") + "%no-filesystem\n"
    + stmt("second") + "throw \"STOP\";\n",));

# the top-level code of this program: a restriction and an option that allows more after a statement; the on_exit
# handlers run at the end, each with the options of the statement that declares it
on_exit printf("own-handler1=%y %y\n", Program::hasParseOption("no-filesystem"),
    Program::hasParseOption("allow-weak-references"));
printf("own-first=%y %y\n", Program::hasParseOption("no-filesystem"),
    Program::hasParseOption("allow-weak-references"));
%no-filesystem
printf("own-second=%y %y\n", Program::hasParseOption("no-filesystem"),
    Program::hasParseOption("allow-weak-references"));
%allow-weak-references
on_exit printf("own-handler2=%y %y\n", Program::hasParseOption("no-filesystem"),
    Program::hasParseOption("allow-weak-references"));
printf("own-third=%y %y\n", Program::hasParseOption("no-filesystem"),
    Program::hasParseOption("allow-weak-references"));
