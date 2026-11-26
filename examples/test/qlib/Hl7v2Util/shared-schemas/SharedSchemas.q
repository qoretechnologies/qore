# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The schemas of the process-wide HL7 v2 schema registry are shared objects: sandboxed code that gets them cannot
# change them (issue #7683); see shared-schemas.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires Hl7v2Util

# a schema that trusted code registers, and one that it loads into the registry's cache
Hl7v2Schema registered({
    "version": "test-7683",
    "segments": {
        "MSH": {"tag": "MSH", "name": "Message Header", "fields": ({"position": 1, "name": "Field Separator"},)},
    },
});
Hl7v2SchemaRegistry::registerSchema(registered);
Hl7v2Schema loaded = Hl7v2SchemaRegistry::loadFile(get_script_dir() + "/../fixtures/v251-schema.qv", True);

# the code of a tenant (a sandboxed Program)
const TenantCode = "
%modern
%requires Hl7v2Util

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the changes that a tenant can attempt on a schema
hash<string, code> sub changes() {
    return {
        \"version\": sub (Hl7v2Schema s) { s.version = \"tenant\"; },
        \"meta\": sub (Hl7v2Schema s) { s.meta.version = \"tenant\"; },
        \"segment definition\": sub (Hl7v2Schema s) { s.segments.MSH.name = \"tenant\"; },
        \"new segment\": sub (Hl7v2Schema s) { s.segments.ZZZ = s.segments.MSH; },
        \"remove segment\": sub (Hl7v2Schema s) { remove s.segments.MSH; },
        \"delete\": sub (Hl7v2Schema s) { delete s; },
    };
}

list<string> sub change_names() {
    return keys changes();
}

# changes the schema that the registry returns to the tenant
string sub change_registered(string name) {
    return try_change(sub () { changes(){name}(Hl7v2SchemaRegistry::getRegisteredSchema(\"test-7683\")); });
}

# changes a schema that the host gives the tenant
string sub change_given(string name, Hl7v2Schema s) {
    return try_change(sub () { changes(){name}(s); });
}

# a schema that the tenant creates itself is its own
string sub own_schema() {
    Hl7v2Schema s(\"own\");
    return try_change(sub () { s.version = \"own2\"; }) + sprintf(\", shared: %y\", SandboxManager::isSharedObject(s));
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# the state of a schema, or the exception if it was deleted
string sub state(Hl7v2Schema s) {
    try {
        return sprintf("%y %y %y %y", s.version, s.meta.version, s.segments.MSH.name, keys s.segments);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string registered_before = state(registered);
string loaded_before = state(loaded);
foreach string name in (tenant.callFunction("change_names")) {
    string rv = tenant.callFunction("change_registered", name);
    printf("registered schema, %s: %s, %s\n", name, rv,
        state(registered) == registered_before ? "unchanged" : "CHANGED");
    rv = tenant.callFunction("change_given", name, loaded);
    printf("cached schema, %s: %s, %s\n", name, rv, state(loaded) == loaded_before ? "unchanged" : "CHANGED");
}
printf("the tenant's own schema: %s\n", tenant.callFunction("own_schema"));
printf("trusted code changes a registered schema: %s\n", (string sub () {
    try {
        registered.meta.note = "host";
        return sprintf("%y", registered.meta.note);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
