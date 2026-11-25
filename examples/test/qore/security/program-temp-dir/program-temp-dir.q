#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# The temporary directory of a Program (qore#7656): prints what restricted, sandboxed and unrestricted code gets from
# tmp_location() and FsUtil::TmpDir, with the directory replaced by "<dir>"; every execution mode and qcc executable
# must print the expected output (see program-temp-dir.qtest)
#
# usage: program-temp-dir.q <scratch directory>

%modern
%no-child-restrictions

%requires Util
%requires FsUtil
# a test module of mods/, which program-temp-dir.qtest puts in the module path
%requires LazyPgm

string scratch = ARGV[0];
string dir = scratch + "/pgmtmp";
mkdir(dir);

#! replaces the directory and the scratch directory in a result; any other directory is the environment's or the
#! platform default, which depends on the system: "<default>"
string sub show(auto v) {
    if (v.typeCode() == NT_STRING && absolute_path(v) && v.find(scratch) != 0) {
        return "<default>";
    }
    string s = sprintf("%y", v);
    s = replace(s, dir, "<dir>");
    return replace(s, scratch, "<scratch>");
}

#! the code that the Programs run: tmp_location(), a TmpDir and a child Program that it creates
const Code = "
string sub location() {
    return tmp_location();
}

string sub tmp_dir() {
    TmpDir t(\"pgmtmp-\");
    return dirname(t.path);
}

string sub child() {
    Program c(PO_NO_EXTERNAL_INFO);
    c.loadModule(\"Util\");
    c.parse(\"string sub w() { return tmp_location(); }\", \"child\");
    return c.callFunction(\"w\");
}

auto sub set_own(auto p) {
    try {
        p.setTempDir(\"/\");
        return \"set\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub shared() {
    return LazyPgm::location();
}

auto sub set_child() {
    Program c();
    try {
        c.setTempDir(\"/\");
        return \"set\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

#! creates a Program with the modules loaded and the code parsed, then restricts it as given
Program sub make(int po, *SandboxManager sm) {
    Program p(po);
    p.loadModule("Util");
    p.loadModule("FsUtil");
    p.loadModule("LazyPgm");
    if (sm) {
        p.setSandboxManager(sm);
    }
    p.setTempDir(dir);
    p.parse("%modern\n" + Code, "code");
    return p;
}

sub call(string label, Program p, string func, *list<auto> args) {
    try {
        printf("%s: %s\n", label, show(p.callFunctionArgs(func, args)));
    } catch (hash<ExceptionInfo> ex) {
        printf("%s: %s\n", label, ex.err);
    }
}

# code restricted from external information: the Program's directory, for tmp_location(), TmpDir and a child Program
Program restricted = make(PO_NO_EXTERNAL_INFO);
call("restricted tmp_location", restricted, "location");
call("restricted TmpDir", restricted, "tmp_dir");
call("restricted child", restricted, "child");
# it cannot change the directory of a Program, lent to it or created by it
call("restricted set lent", restricted, "set_own", (restricted,));
call("restricted set child", restricted, "set_child");

# a Program that module code creates and keeps for all of its callers takes no directory from the caller that created
# it: neither this Program's nor the next caller's directory is used for the other's files
call("restricted module Program", restricted, "shared");
string dir2 = scratch + "/second";
mkdir(dir2);
Program second = make(PO_NO_EXTERNAL_INFO);
second.setTempDir(dir2);
call("second restricted module Program", second, "shared");
call("second restricted tmp_location", second, "location");
printf("host module Program: %s\n", show(LazyPgm::location()));

# sandboxed code whose file system policy allows the directory
SandboxManager allowed();
allowed.setFilesystemDefaultPolicy(False);
allowed.addFilesystemAllowedPath(dir, QSEC_READ | QSEC_WRITE | QSEC_CREATE | QSEC_DELETE);
Program sandboxed = make(0, allowed);
call("sandboxed tmp_location", sandboxed, "location");
call("sandboxed TmpDir", sandboxed, "tmp_dir");
call("sandboxed child", sandboxed, "child");
call("sandboxed set lent", sandboxed, "set_own", (sandboxed,));

# a sandbox whose policy does not allow the directory: no other directory is used
SandboxManager denied();
denied.setFilesystemDefaultPolicy(False);
denied.addFilesystemAllowedPath(scratch + "/elsewhere", QSEC_ALL);
Program outside = make(0, denied);
call("outside the policy tmp_location", outside, "location");

# unrestricted code of the Program gets the directory too, not the environment's
Program host = make(0);
call("unrestricted tmp_location", host, "location");
printf("getTempDir: %s\n", show(host.getTempDir()));
host.setTempDir();
printf("cleared: %y\n", host.getTempDir());

# validation
{
    File f();
    f.open2(scratch + "/file", O_CREAT | O_WRONLY | O_TRUNC);
}
foreach list<string> bad in ((("relative", "relative/path"), ("missing", dir + "/missing"),
        ("not a directory", scratch + "/file"), ("NUL character", "/tmp\0x"))) {
    try {
        host.setTempDir(bad[1]);
        printf("set %s: set\n", bad[0]);
    } catch (hash<ExceptionInfo> ex) {
        printf("set %s: %s\n", bad[0], ex.err);
    }
}

# a directory removed after it was set: no other directory is used
string gone = scratch + "/gone";
mkdir(gone);
Program removed = make(PO_NO_EXTERNAL_INFO);
removed.setTempDir(gone);
rmdir(gone);
call("removed tmp_location", removed, "location");

# code restricted from external information cannot read the directory of a Program object
Program reader(PO_NO_EXTERNAL_INFO);
try {
    reader.parse("%modern\nauto sub get(Program p) { return p.getTempDir(); }", "reader");
    printf("restricted getTempDir: parsed\n");
} catch (hash<ExceptionInfo> ex) {
    printf("restricted getTempDir: %s: %s\n", ex.err, ex.desc =~ /EXTERNAL[-_]INFO/ ? "external info" : ex.desc);
}
