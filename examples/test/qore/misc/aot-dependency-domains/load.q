# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of aot-dependency-domains.qtest: a Program that disallows locale control loads an outer module that requires
# a module using it (qore#6351); arguments: the module to require (a name, or a path for a load outside the module
# path, which is not library code), its namespace and optionally "runtime" to load it with load_module() in the
# Program's code instead of %requires; a module is loaded once in a process, so each case runs in a process of its own

%modern
%no-child-restrictions

sub main() {
    string mod = ARGV[0];
    string ns = ARGV[1];
    # PO_NO_GUI: the modules with a conditional on it take its branch (DdCfN, DdCgO), as do those with a conditional
    # on DD_HOST_DEFINE (DdHd)
    Program p(PO_NEW_STYLE | PO_NO_LOCALE_CONTROL | PO_NO_GUI);
    p.define("DD_HOST_DEFINE", True);
    try {
        if (ARGV[2] == "runtime") {
            p.parse("string sub f(string mod, string ns) { load_module(mod); return call_function(ns + \"::run\"); }",
                "load");
            printf("%s\n", p.callFunction("f", mod, ns));
        } else {
            p.parse(sprintf("%%requires %s\nstring sub f() { return %s::run(); }", mod, ns), "load");
            printf("%s\n", p.callFunction("f"));
        }
    } catch (hash<ExceptionInfo> ex) {
        # the error of a source module (a parse error for the locale control domain) and of an AOT module
        # (AOT-MODULE-RESTRICTED) differ; the inner module is refused for its use of locale control in both; any other
        # error is printed as it is
        if (ex.desc =~ /DdInner/ && (ex.err == "AOT-MODULE-RESTRICTED" || ex.desc =~ /LOCALE_CONTROL/)) {
            printf("refused: DdInner uses locale control\n");
        } else {
            printf("%s: %s\n", ex.err, ex.desc);
        }
    }
}

main();
