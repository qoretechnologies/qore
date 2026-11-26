# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The release schemas of the process-wide EDIFACT schema registry are shared objects: sandboxed code that gets them
# cannot change them (issue #7685); see shared-release-schemas.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires FsUtil
%requires EdifactUtil

# trusted code loads a release into the registry
TmpDir tmp("edifact-shared-");
EdifactSchema loaded = EdifactSchemaRegistry::loadRelease("D21A", {
    "dir": normalize_dir(get_script_dir() + "/../fixtures/D21A") + DirSep,
    "cache_dir": tmp.path,
    "skip_cache_read": True,
    "reload": True,
});

# the code of a tenant (a sandboxed Program)
const TenantCode = "
%modern
%requires EdifactUtil

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the changes that a tenant can attempt on a release schema
hash<string, code> sub changes() {
    return {
        \"release\": sub (EdifactSchema s) { s.release = \"tenant\"; },
        \"meta\": sub (EdifactSchema s) { s.meta.parsed = False; },
        \"segment definition\": sub (EdifactSchema s) { s.segment_defs{(keys s.segment_defs)[0]}.name = \"tenant\"; },
        \"new message definition\": sub (EdifactSchema s) {
            s.message_defs.TENANT = s.message_defs{(keys s.message_defs)[0]};
        },
        \"remove segment definitions\": sub (EdifactSchema s) { remove s.segment_defs; },
        \"code lists\": sub (EdifactSchema s) { s.codelists.uncl = {}; },
        \"delete\": sub (EdifactSchema s) { delete s; },
    };
}

list<string> sub change_names() {
    return keys changes();
}

# changes the schema that the registry returns to the tenant
string sub change_release(string name) {
    return try_change(sub () { changes(){name}(EdifactSchemaRegistry::getRelease(\"D21A\")); });
}

# a schema that the tenant creates itself is its own
string sub own_schema() {
    EdifactSchema s(\"own\");
    return try_change(sub () { s.release = \"own2\"; }) + sprintf(\", shared: %y\", SandboxManager::isSharedObject(s));
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# the state of the schema, or the exception if it was deleted
string sub state(EdifactSchema s) {
    try {
        return sprintf("%y %y %y %y %y", s.release, s.meta.parsed, s.segment_defs, keys s.message_defs,
            s.codelists.uncl.size());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string before = state(loaded);
printf("the registry returns the loaded schema: %y\n", EdifactSchemaRegistry::getRelease("D21A") == loaded);
foreach string name in (tenant.callFunction("change_names")) {
    string rv = tenant.callFunction("change_release", name);
    printf("%s: %s, %s\n", name, rv, state(loaded) == before ? "unchanged" : "CHANGED");
}
printf("the tenant's own schema: %s\n", tenant.callFunction("own_schema"));
printf("trusted code changes the schema: %s\n", (string sub () {
    try {
        loaded.meta.parsed = loaded.meta.parsed;
        return "changed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
