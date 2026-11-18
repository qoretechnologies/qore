# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# fixture of index-writer-filesystem.qtest: the APIs that write the data provider index files are in the FILESYSTEM
# domain (#6500); arguments: the execution mode of the restricted Program
# a module is loaded once in a process, so the test runs this script in a process of its own

%modern
%no-child-restrictions

# the modules are loaded by unrestricted code first: a Program that loads a module that is already loaded is not
# checked against the module's code again, so its restrictions decide only by the domains that the module declares
%requires ProviderIndexUtil
%requires ProviderIndex


hash<string, ExecMode> modes = {"ast": EM_AST, "ir": EM_IR, "jit": EM_JIT};
string mode = ARGV[0] ?? "ast";
string dir = ENV.QORE_PROVIDER_INDEX_DIR;

# the class, the method, the arguments of a call made at run time and those of a direct call in source
hash<string, list<auto>> Apis = {
    "writeDataProviderIndex": ("ProviderIndexUtil::ProviderIndexUtil", "writeDataProviderIndex", (NOTHING, 0s, {}),
        "NOTHING, 0s, {}"),
    "writeDataProviderIndexByApp": ("ProviderIndexUtil::ProviderIndexUtil", "writeDataProviderIndexByApp",
        ({}, NOTHING, sub (string app) {}, 0s, {}), "{}, NOTHING, sub (string app) {}, 0s, {}"),
    "createDataProviderIndex": ("ProviderIndex::ProviderIndex", "createDataProviderIndex", (), ""),
    "prepareIndexWorker": ("ProviderIndex::ProviderIndex", "prepareIndexWorker", ("/nonexistent-batch",),
        "\"/nonexistent-batch\""),
    "runIndexWorker": ("ProviderIndex::ProviderIndex", "runIndexWorker", ("/nonexistent-batch",),
        "\"/nonexistent-batch\""),
    "dumpIndexYaml": ("ProviderIndexUtil::ProviderIndexUtil", "dumpIndexYaml", (dir, NOTHING, dir + "/dump.yaml"),
        sprintf("%y, NOTHING, %y", dir, dir + "/dump.yaml")),
    "readSerializedDataProviderIndex": ("ProviderIndexUtil::ProviderIndexUtil", "readSerializedDataProviderIndex",
        (dir,), sprintf("%y", dir)),
    "getSerializedDataProviderIndexSource": ("ProviderIndexUtil::ProviderIndexUtil",
        "getSerializedDataProviderIndexSource", (dir,), sprintf("%y", dir)),
    "tryLoadModule": ("ProviderIndexUtil::ProviderIndexUtil", "tryLoadModule", (dir + "/none.qm",),
        sprintf("%y", dir + "/none.qm")),
    "setEnvironmentVariable": ("ProviderIndexUtil::ProviderIndexUtil", "setEnvironmentVariable", (dir + "/sub",),
        sprintf("%y", dir + "/sub")),
};

# returns the files in the index directory
string sub files() {
    Dir d();
    d.chdir(dir);
    return (d.listFiles() ?? ()).join(",") ?: "none";
}

# code restricted only from the file system, which can also control the process
foreach hash<auto> i in (Apis.pairIterator()) {
    # a direct call is refused when it is parsed
    Program p(PO_NEW_STYLE | PO_NO_FILESYSTEM);
    p.setExecMode(modes{mode} ?? EM_AST);
    try {
        p.parse(sprintf("%%modern\n%%requires ProviderIndexUtil\n%%requires ProviderIndex\n"
            "sub t() { %s::%s(%s); }", i.value[0], i.value[1], i.value[3]), "direct");
        printf("%s direct: parsed\n", i.key);
    } catch (hash<ExceptionInfo> ex) {
        printf("%s direct: %s %s\n", i.key, ex.err, (ex.desc =~ /functional domain: FILESYSTEM/) ? "FILESYSTEM"
            : replace(ex.desc, "\n", " "));
    }
    # a call resolved at run time is refused when it is made
    Program r(PO_NEW_STYLE | PO_NO_FILESYSTEM);
    r.setExecMode(modes{mode} ?? EM_AST);
    r.parse("%modern\n%requires ProviderIndexUtil\n%requires ProviderIndex\n"
        "string sub t(string cls, string m, list<auto> args) { try { call_static_method_args(cls, m, args); "
        "return \"called\"; } catch (hash<ExceptionInfo> ex) { return ex.err + "
        "((ex.desc =~ /functional domain: FILESYSTEM/) ? \" FILESYSTEM\" : \"\"); } }", "runtime");
    printf("%s runtime: %s\n", i.key, r.callFunction("t", i.value[0], i.value[1], i.value[2]));
}
printf("index files after the restricted calls: %s\n", files());
printf("index directory after the restricted calls: %s\n", ENV.QORE_PROVIDER_INDEX_DIR == dir ? "unchanged" : "changed");

# the process control half of the domains is still checked
{
    Program p(PO_NEW_STYLE | PO_NO_PROCESS_CONTROL);
    p.setExecMode(modes{mode} ?? EM_AST);
    try {
        p.parse("%modern\n%requires ProviderIndexUtil\n"
            "sub t() { ProviderIndexUtil::ProviderIndexUtil::writeDataProviderIndex(NOTHING, 0s, {}); }", "process");
        printf("writeDataProviderIndex process control: parsed\n");
    } catch (hash<ExceptionInfo> ex) {
        printf("writeDataProviderIndex process control: %s %s\n", ex.err,
            (ex.desc =~ /functional domain: PROCESS_CONTROL/) ? "PROCESS_CONTROL" : replace(ex.desc, "\n", " "));
    }
}

# unrestricted code writes the index
ProviderIndexUtil::writeDataProviderIndex(NOTHING, 0s, {});
printf("index files after the unrestricted call: %s\n", files() != "none" ? "written" : "none");
