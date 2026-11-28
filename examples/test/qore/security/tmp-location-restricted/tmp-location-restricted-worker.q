#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The temporary directory that library code gives code restricted from external information without a directory of
# its Program: the platform default, unresolved, whatever the current directory (see tmp-location-restricted.qtest)
#
# usage: tmp-location-restricted-worker.q <execution mode of the Programs: ast|ir|jit|tiered>
#
# prints one "<caller>/<check>=<value>" line per check, with the values that the test compares

%modern
%no-child-restrictions

%requires Util
%requires FsUtil

#! the execution modes of the Programs that the worker creates
const Modes = {
    "ast": EM_AST,
    "ir": EM_IR,
    "jit": EM_JIT,
    "tiered": EM_TIERED,
};
ExecMode mode = Modes{ARGV[0] ?? "ast"};

# the code of each Program: the temporary directory, and the directories of a temporary directory and file that the
# library creates without being given a directory
const Code = "%modern
auto sub t(code c) { try { return c(); } catch (hash<ExceptionInfo> ex) { return ex.err + \": \" + ex.desc; } }
auto sub loc() { return t(auto sub () { return tmp_location(); }); }
auto sub tmpdir() {
    return t(auto sub () { FsUtil::TmpDir d(\"tlr-\"); return dirname(d.path); });
}
auto sub tmpfile() {
    return t(auto sub () {
        hash<FsUtil::TmpFileHash> f = FsUtil::make_tmp_file(\"tlr-\");
        on_exit unlink(f.path);
        return dirname(f.path);
    });
}
";

#! the Programs: a restricted one, one that a sandbox also governs, and an unrestricted one
hash<string, int> kinds = {
    "restricted": PO_NO_EXTERNAL_INFO,
    "sandboxed-restricted": PO_NO_EXTERNAL_INFO,
    "unrestricted": 0,
};

foreach hash<auto> kind in (kinds.pairIterator()) {
    Program p(PO_MODERN | kind.value);
    p.setExecMode(mode);
    p.loadModule("Util");
    p.loadModule("FsUtil");
    if (kind.key == "sandboxed-restricted") {
        # a sandbox that allows the file system: only the restriction from external information is checked
        SandboxManager sm();
        sm.setFilesystemDefaultPolicy(True);
        p.setSandboxManager(sm);
    }
    p.parse(Code, kind.key);
    foreach string f in (("loc", "tmpdir", "tmpfile")) {
        printf("%s/%s=%s\n", kind.key, f, p.callFunction(f));
    }
}
