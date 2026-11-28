# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A data provider module whose vendored schemas are built with LazySharedValue still loads in a sandboxed Program that
# loads it by its path from the module path: it is library code there, and its values are process-shared; a copy of
# its own outside the module path is refused when it registers its factory in the process-wide catalog (qore#7684);
# see LazySharedValuesTest.qc

%modern
%enable-all-warnings
%no-child-restrictions

%requires FsUtil

# the module's source, from the module path of the test
*string sub find_module() {
    foreach string dir in (ENV.QORE_MODULE_DIR.split(":")) {
        string path = dir + "/TogetherDataProvider/TogetherDataProvider.qm";
        if (is_file(path)) {
            return path;
        }
    }
}

*string path = find_module();
if (!path) {
    printf("the module source was not found in the module path\n");
    exit(1);
}

# a sandbox that lets the Programs read the module's files
SandboxManager sm();
sm.setFilesystemDefaultPolicy(True);

const FirstUseCode = "
%modern
string sub first_use() {
    object schema = TogetherDataProvider::TogetherSchema::getSchema();
    return sprintf(\"schema shared: %y, operations: %y\", SandboxManager::isSharedObject(schema),
        TogetherDataProvider::TogetherSchema::getActionSet().getActionInfoList(\"Together\").size() > 0);
}
";

# the module from the module path, loaded by its path: library code, whose values are process-shared
{
    Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    tenant.setSandboxManager(sm);
    tenant.loadModule(path);
    tenant.parse(FirstUseCode, "tenant");
    printf("a sandboxed Program that loads the module from the module path by its path: %s\n",
        tenant.callFunction("first_use"));
}

# a copy of the module under another name outside the module path: the sandboxed Program's own module, whose values
# are private
string dir = make_tmp_dir();
on_exit remove_tree(dir);
mkdir(dir + "/TogetherDataProviderCopy");
{
    string src = dirname(path);
    Dir d();
    d.chdir(src);
    foreach string f in (d.listFiles()) {
        if (f !~ /\.(qm|qc|json|svg)$/) {
            continue;
        }
        string data = ReadOnlyFile::readTextFile(src + "/" + f);
        string target = f;
        if (f == "TogetherDataProvider.qm") {
            target = "TogetherDataProviderCopy.qm";
            data = replace(data, "module TogetherDataProvider {", "module TogetherDataProviderCopy {");
        }
        File out();
        out.open2(dir + "/TogetherDataProviderCopy/" + target, O_CREAT | O_WRONLY | O_TRUNC);
        out.write(data);
        out.close();
    }
}
{
    Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    tenant.setSandboxManager(sm);
    # the module registers its data provider factory in the process-wide catalog when it is initialized, which the
    # catalog refuses to sandboxed code (qore#6747); the private values of a sandboxed Program's own module are
    # checked with the test module of LazySharedValues.q
    string result;
    try {
        tenant.loadModule(dir + "/TogetherDataProviderCopy/TogetherDataProviderCopy.qm");
        tenant.parse(FirstUseCode, "tenant");
        result = tenant.callFunction("first_use");
    } catch (hash<ExceptionInfo> ex) {
        result = ex.err;
    }
    printf("a sandboxed Program that loads its own copy of the module by its path: %s\n", result);
}
