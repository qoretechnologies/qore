#!/usr/bin/env qore
# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.

# A host Program lent to code restricted from external information (qore#7678): the requests of information of the
# host that its code makes for the borrower (the functions of the external-information domain, tmp_location(),
# SandboxManager::callerMayReadExternalInfo()) are answered as for the borrower; every other check, and the host's own
# calls, are as before.  Every execution mode and qcc executable must print the expected output (see
# lent-program-information.qtest)
#
# usage: lent-program-information.q <scratch directory> <execution mode of the Programs: ast|ir|jit|tiered>

%modern
%no-child-restrictions

%requires Util

string scratch = ARGV[0];

#! the execution modes of the Programs that the worker creates, which do not take the process's
const Modes = {
    "ast": EM_AST,
    "ir": EM_IR,
    "jit": EM_JIT,
    "tiered": EM_TIERED,
};
ExecMode mode = Modes{ARGV[1] ?? "ast"};
string dir = scratch + "/host";
mkdir(dir);
string bdir = scratch + "/borrower";
mkdir(bdir);
string helper = get_script_dir() + "/mods/LentHelper.qm";
string data = scratch + "/data.txt";
{
    File f();
    f.open2(data, O_CREAT | O_WRONLY | O_TRUNC);
    f.write("host data");
}

#! a result with the host's temporary directory, other paths and the other host values replaced
string sub show(auto v) {
    if (v.typeCode() == NT_STRING) {
        if (v.find(dir) == 0) {
            return "<host dir>";
        }
        if (v.find(bdir) == 0) {
            return "<borrower dir>";
        }
        if (absolute_path(v)) {
            return "<path>";
        }
        if (v !~ /^[A-Z-]+$/ && v != "host data" && v != "none") {
            return "<host value>";
        }
    }
    return sprintf("%y", v);
}

#! the fields of an info() result, each shown
string sub showInfo(auto v) {
    if (v.typeCode() != NT_STRING || v.find("|") == -1) {
        return show(v);
    }
    return (map show($1 == "True" ? True : ($1 == "False" ? False : $1)), v.split("|")).join(" ");
}

# the code of the host Programs: requests of information of the host, a file read, and a call of another lent Program
const HostCode = "%modern
string sub loc() { return tmp_location(); }
bool sub may() { return SandboxManager::callerMayReadExternalInfo(); }
auto sub host() { try { return gethostname(); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub env() { try { return getenv(\"PATH\") ?? \"none\"; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub cwd() { try { return getcwd(); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
string sub read(string f) { return ReadOnlyFile::readTextFile(f); }
string sub info() { return sprintf(\"%s|%y|%s|%s|%s\", loc(), may(), host(), env(), cwd()); }
auto sub boom() { throw \"BOOM\"; }
auto sub dyn() { try { return call_function(\"gethostname\"); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub meth(Program p) { try { return p.getTempDir() ?? \"none\"; } catch (hash<ExceptionInfo> ex) { return ex.err; } }
string sub modpid() { return LentHelper::pid(); }
auto sub throwloc() { try { throw \"X\"; } catch (hash<ExceptionInfo> ex) { return ex.file ?? \"\"; } }
string sub nested(Program other) { return other.callFunction(\"info\"); }
class C { static string info() { return ::info(); } }
";

Program host();
host.setExecMode(mode);
host.loadModule("Util");
host.loadModule(helper);
host.setTempDir(dir);
host.parse(HostCode, "host");

Program host2();
host2.setExecMode(mode);
host2.loadModule("Util");
host2.loadModule(helper);
host2.setTempDir(dir);
host2.parse(HostCode, "host2");

# a Program of the host whose top-level code records its answers when the borrower runs it
Program runner();
runner.setExecMode(mode);
runner.loadModule("Util");
runner.setTempDir(dir);
runner.parse("%modern\nour string result = sprintf(\"%s|%y\", tmp_location(), "
    "SandboxManager::callerMayReadExternalInfo());\n", "runner");

# the borrower: restricted from external information and from the file system
Program r(PO_MODERN | PO_NO_EXTERNAL_INFO | PO_NO_FILESYSTEM);
r.setExecMode(mode);
# by name: the module that the host loaded above (the borrower may not access the file system)
r.loadModule("LentHelper");
r.setTempDir(bdir);
r.parse("auto sub t(code c) { try { return c(); } catch (hash<ExceptionInfo> ex) { return ex.err; } }
auto sub cf(Program h, string f) { return t(auto sub () { return h.callFunction(f); }); }
auto sub cfa(Program h, string f, *list<auto> a) { return t(auto sub () { return h.callFunctionArgs(f, a); }); }
auto sub csm(Program h) { return t(auto sub () { return h.callStaticMethod(\"C\", \"info\"); }); }
auto sub csma(Program h) { return t(auto sub () { return h.callStaticMethodArgs(\"C\", \"info\", ()); }); }
auto sub fref(Program h, string f, *list<auto> a) {
    return t(auto sub () { return call_function_args(h.getCallReference(f), a); });
}
auto sub mref(Program h) { return t(auto sub () { code c = h.getCallReference(\"C::info\"); return c(); }); }
auto sub expr(Program h) { return t(auto sub () { return h.getExpression(\"info()\", \"e\").eval(); }); }
auto sub run(Program h) { return t(auto sub () { return h.run(); }); }
auto sub cfref(Program h) { return t(auto sub () { code c = \\h.callFunction(); return c(\"info\"); }); }
auto sub nest(Program h, Program h2) { return t(auto sub () { return h.callFunctionArgs(\"nested\", (h2,)); }); }
code sub getref(Program h) { return h.getCallReference(\"info\"); }
auto sub cfa1(Program h, string f, Program a) { return t(auto sub () { return h.callFunctionArgs(f, (a,)); }); }
auto sub thr(Program h) { return t(auto sub () { return LentHelper::thr(h, \"info\"); }); }
", "borrower");

printf("host: %s\n", showInfo(host.callFunction("info")));
printf("borrower callFunction: %s\n", showInfo(r.callFunction("cf", host, "info")));
printf("borrower callFunctionArgs: %s\n", showInfo(r.callFunction("cfa", host, "info")));
printf("borrower callStaticMethod: %s\n", showInfo(r.callFunction("csm", host)));
printf("borrower callStaticMethodArgs: %s\n", showInfo(r.callFunction("csma", host)));
printf("borrower function reference: %s\n", showInfo(r.callFunction("fref", host, "info")));
printf("borrower method reference: %s\n", showInfo(r.callFunction("mref", host)));
printf("borrower expression: %s\n", showInfo(r.callFunction("expr", host)));
printf("borrower reference to callFunction(): %s\n", showInfo(r.callFunction("cfref", host)));
r.callFunction("run", runner);
printf("borrower run(): %s\n", showInfo(runner.getGlobalVariable("result")));
printf("borrower, nested lent call: %s\n", showInfo(r.callFunction("nest", host, host2)));
# a call resolved at run time, a builtin method of the domain, module code that uses such a function internally (as
# when the borrower calls it directly), the location of an exception, and a lent call from a thread of module code
# that runs for the borrower (no Program of the borrower on its stack)
printf("borrower, a call resolved at run time: %s\n", show(r.callFunction("cf", host, "dyn")));
printf("borrower, a builtin method of the domain: %s\n", show(r.callFunction("cfa1", host, "meth", host)));
printf("borrower, module code that uses the domain internally: %y; host: %y\n", r.callFunction("cf", host, "modpid"),
    host.callFunction("modpid"));
printf("borrower, the location of an exception: %y; host: %y\n", r.callFunction("cf", host, "throwloc"),
    host.callFunction("throwloc"));
printf("borrower, a lent call from a module thread: %s\n", showInfo(r.callFunction("thr", host)));
# a check that does not concern external information: the host's code reads a file that the borrower may not read
printf("borrower, a host function that reads a file: %s %s\n", show(r.callFunction("cfa", host, "read", (data,))),
    show(r.callFunction("fref", host, "read", (data,))));
# an exception in a lent call ends the borrower's call only
printf("borrower, an exception in the lent call: %s\n", show(r.callFunction("cf", host, "boom")));
printf("host after the exception: %s\n", showInfo(host.callFunction("info")));
# a reference that the borrower obtained, called by the host: the host's answers
code c = r.callFunction("getref", host);
printf("host calls the borrower's reference: %s\n", showInfo(c()));
printf("host after the borrower's calls: %s\n", showInfo(host.callFunction("info")));
