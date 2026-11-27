# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A schema that a tenant loads with SwaggerLoader, OpenApi3Loader or AsyncApi3Loader is the tenant's own, and only its
# own: another tenant cannot delete it or give it a logger; a schema that library code loads for a tenant, and one
# that trusted code loads, are not handed to the tenant (AOT3 fault (p), qore#6842); see shared-schema-ownership.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires Swagger
%requires OpenApi3
%requires AsyncApi
%requires RestClient
%requires json
%requires Logger

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "test", "version": "1.0"},
    "basePath": "/api",
    "paths": {
        "/things": {
            "get": {
                "responses": {"200": {"description": "ok"}},
            },
        },
    },
};

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {
        "/things": {
            "get": {
                "responses": {"200": {"description": "ok"}},
            },
        },
    },
};

const AsyncApiDoc = {
    "asyncapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
};

const Docs = {
    "Swagger": SwaggerDoc,
    "OpenAPI 3": OpenApi3Doc,
    "AsyncAPI": AsyncApiDoc,
};

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires Swagger
%requires OpenApi3
%requires AsyncApi
%requires RestClient
%requires Logger

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# loads a schema of the given kind with the given entry point of its loader
object sub load(string kind, string how, string arg) {
    switch (kind) {
        case \"Swagger\":
            switch (how) {
                case \"fromString\": return SwaggerLoader::fromString(arg, True);
                case \"fromFile\": return SwaggerLoader::fromFile(arg);
            }
            return SwaggerLoader::fromUrl(arg, True);
        case \"OpenAPI 3\":
            switch (how) {
                case \"fromString\": return OpenApi3Loader::fromString(arg, True);
                case \"fromFile\": return OpenApi3Loader::fromFile(arg);
            }
            return OpenApi3Loader::fromUrl(arg, True);
    }
    switch (how) {
        case \"fromString\": return AsyncApi3Loader::fromString(arg, True);
        case \"fromFile\": return AsyncApi3Loader::fromFile(arg);
    }
    return AsyncApi3Loader::fromUrl(arg, True);
}

# what the tenant can do with a schema; deletes it last
string sub check(object schema) {
    return sprintf(\"owned: %y, shared: %y, setLogger(): %s, delete: %s\", SandboxManager::isObjectOwnedByCaller(schema),
        SandboxManager::isSharedObject(schema), try_change(sub () { schema.setLogger(new Logger(\"tenant\")); }),
        try_change(sub () { delete schema; }));
}

# the validator that the code of the RestClient module loads for the tenant
object sub client_validator(string path) {
    RestClient rc({\"url\": \"http://localhost:1\", \"swagger\": path}, True);
    return rc.getValidator();
}
";

# the directory of the schema files, which the tenants may read
string dir = tmp_location() + DirSep + sprintf("shared-schema-ownership-%d-%s", getpid(), get_random_string(8));
mkdir(dir, 0700);
on_exit {
    foreach string f in (glob(dir + DirSep + "*")) {
        unlink(f);
    }
    rmdir(dir);
}

# a tenant at the enforcement level of tenant isolation (the default)
Program sub tenant(string name, *int level) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    SandboxManager sm();
    if (exists level) {
        sm.setEnforcementLevel(level);
    }
    sm.addFilesystemAllowedPath(dir, QSEC_READ);
    p.setSandboxManager(sm);
    p.parse(TenantCode, name);
    return p;
}

Program a = tenant("tenant A");
Program b = tenant("tenant B");

string sub try_change(code c) {
    try {
        c();
        return "changed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# what trusted code can do with a schema, without deleting it
string sub host_check(object schema) {
    return sprintf("shared: %y, setLogger(): %s", SandboxManager::isSharedObject(schema),
        try_change(sub () { schema.setLogger(new Logger("host")); schema.setLogger(); }));
}

foreach string kind in (keys Docs) {
    string text = make_json(Docs{kind});
    string path = dir + DirSep + replace(kind, " ", "") + ".json";
    File f();
    f.open2(path, O_CREAT | O_TRUNC | O_WRONLY);
    f.write(text);
    f.close();
    printf("%s:\n", kind);
    foreach string how in ("fromString", "fromFile", "fromUrl") {
        object schema = a.callFunction("load", kind, how, how == "fromString" ? text : path);
        # another tenant first, then trusted code, then the tenant that loaded it, which deletes it
        printf("  %s: tenant B: %s; host: %s; tenant A: %s\n", how, b.callFunction("check", schema),
            host_check(schema), a.callFunction("check", schema));
    }
    # a schema that trusted code loads is shared, and not handed to a tenant
    object schema;
    switch (kind) {
        case "Swagger": schema = SwaggerLoader::fromString(text, True); break;
        case "OpenAPI 3": schema = OpenApi3Loader::fromString(text, True); break;
        default: schema = AsyncApi3Loader::fromString(text, True); break;
    }
    printf("  trusted code's schema: host: %s; tenant A: %s; host deletes it: %s\n", host_check(schema),
        a.callFunction("check", schema), try_change(sub () { delete schema; }));
}

# a schema that library code loads for a tenant may be kept for other callers: it is not handed to the tenant
printf("the RestClient module's validator for tenant A: %s\n",
    a.callFunction("check", a.callFunction("client_validator", dir + DirSep + "Swagger.json")));

# a tenant at the enforcement level of code safety owns every object: nothing is handed over, and loading works
Program c = tenant("tenant C", QSEL_CODE_SAFETY);
printf("a tenant at the level of code safety: %s\n", c.callFunction("check", c.callFunction("load", "OpenAPI 3",
    "fromString", make_json(OpenApi3Doc))));
