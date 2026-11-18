# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of set-time-zone-compiled.qtest: modules that restrict locale control themselves (%no-locale-control) and
# set their time zones after their headers (qore#6352); the header's locale control domain removes the restriction
# before it in a load as library code, from source and from a module compiled with qcc, and nothing else does: not a
# load by a path outside the module path, a restriction after the header or a header that declares another domain; a
# module is loaded once in a process, so each is loaded once

%modern
%no-child-restrictions
%requires FsUtil

#! loads the module in a new Program and prints the time zone of its code
sub check(string label, string req, string mod, int po = 0, *string def) {
    Program p(PO_NEW_STYLE | po);
    if (def) {
        p.define(def, True);
    }
    try {
        p.parse(sprintf("%%requires %s\nstring sub f() { return %s::tz_region(); }", req, mod), label);
        printf("%s: %s\n", label, p.callFunction("f"));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s: %s\n", label, ex.err, ex.desc);
    }
}

#! returns the path of the module in the first directory of the module path, from source or compiled
string sub module_file(string mod) {
    string dir = ENV.QORE_MODULE_DIR.split(":")[0];
    foreach string ext in (".qmod", ".qm") {
        if (is_file(dir + "/" + mod + ext)) {
            return dir + "/" + mod + ext;
        }
    }
    throw "NO-MODULE", sprintf("%s not found in %y", mod, dir);
}

sub main() {
    check("own restriction before the header", "TzOwnBefore", "TzOwnBefore");
    check("own restriction after the header", "TzOwnAfter", "TzOwnAfter");
    check("own restriction before and after the header", "TzOwnBoth", "TzOwnBoth");
    check("own restriction, another domain declared", "TzOwnOther", "TzOwnOther");
    # with PO_NO_GUI, the module restricts locale control again after its header, in a conditional on it
    check("own restriction again in a conditional on a parse option", "TzOwnCond", "TzOwnCond", PO_NO_GUI);
    # the loading code defines TZ_HOST_DEFINE: the module restricts locale control again after its header
    check("own restriction again in a conditional on a define of the loading code", "TzOwnHost", "TzOwnHost", 0,
        "TZ_HOST_DEFINE");
    # a copy outside the module path, loaded by its path
    TmpDir tmp("tz-own-");
    string src = module_file("TzOwnUntrusted");
    string path = tmp.path + "/" + basename(src);
    copy_file(src, path);
    check("own restriction before the header, not library code", path, "TzOwnUntrusted");
}

main();
