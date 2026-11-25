# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The data that a caller gives never reaches the caches of a shared REST schema, and the code of a sandboxed caller
# never runs in a type build of one: a type of the caller's class given for a response type, a REST client object of
# the caller's class and its request body defaults, and the strings and objects that a sandboxed caller gives to the
# public type and field builders, which still find what the schema cached; the types that a build of a shared schema
# caches cannot be changed by sandboxed code (qore#6843); see shared-schema-caller-data.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires reflection
%requires Swagger
%requires OpenApi3
%requires json

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "test", "version": "1.0"},
    "basePath": "/api",
    "paths": {},
    "definitions": {
        "Thing": {"type": "object", "required": ("name",), "properties": {
            "name": {"type": "string"},
            "inner": {"type": "object", "readOnly": True, "properties": {"a": {"type": "number"}}},
        }},
    },
};

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {},
    "components": {"schemas": {
        "Thing": {"type": "object", "required": ("token", "name"), "properties": {
            "token": {"type": "string"},
            "name": {"type": "string"},
        }},
        "Other": {"type": "object", "properties": {"x": {"type": "string"}}},
        "Auth": {"type": "object", "required": ("meta",), "properties": {
            "meta": {"type": "object", "properties": {"k": {"type": "string"}}},
            "note": {"type": "object", "properties": {"k": {"type": "string"}}},
        }},
    }},
};

#! a REST client of the host whose request bodies carry a secret, as an API client with an auth token does
class HostRest {
    auto preprocessRequestBody() {
        return {"token": "host-secret"};
    }
}

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires reflection
%requires Swagger
%requires OpenApi3
%requires DataProvider
%requires json

our list<string> events = ();
# the objects that the tenant keeps
our list<object> keep = ();

# a type of the tenant's own class
class TenantType inherits DataProvider::HashDataType {
    public {
        string fake;
    }
    constructor(string fake) : HashDataType(\"tenant-type\") {
        self.fake = fake;
    }
    string uniqueHash() {
        return fake;
    }
}

# a schema object of the tenant's own class, which reports when the schema releases its caches
class TenantSchemaObject inherits OpenApi3::SchemaObject {
    constructor(OpenApi3Schema own) : OpenApi3::SchemaObject(\"tenant\", {\"type\": \"string\"}, own) {
    }
    releaseTypeCaches() {
        events += \"the schema ran the tenant's releaseTypeCaches()\";
    }
}

# a REST client object of the tenant's own class, whose code a type build would call
class TenantRest {
    private {
        object shared;
        Queue start;
        Queue done;
    }
    constructor(object shared, Queue start, Queue done) {
        self.shared = shared;
        self.start = start;
        self.done = done;
    }
    auto preprocessRequestBody() {
        events += sprintf(\"the tenant's code runs with the type build lock held: %y\", shared.isTypeBuildActive());
        # another thread builds a type of the schema meanwhile; the wait only ends a run in which it cannot
        start.push(True);
        try {
            events += \"another thread builds a type of the schema meanwhile: \" + done.get(20000);
        } catch (hash<ExceptionInfo> ex) {
            events += \"another thread builds a type of the schema meanwhile: \" + ex.err;
        }
        OpenApi3Schema own = OpenApi3Loader::fromString(make_json({\"openapi\": \"3.0.0\", \"info\": {\"title\": \"x\",
            \"version\": \"1\"}, \"paths\": {}}), True);
        TenantSchemaObject so(own);
        keep += (own, so);
        events += sprintf(\"the schema holds the tenant's object in its caches: %y\", shared.holdTypeCacheOpaquely(so));
        return {\"token\": \"tenant-token\"};
    }
}

# a REST client object of the tenant whose request bodies carry the tenant's own token
class TenantTokenRest {
    private {
        string token;
    }
    constructor(string token) {
        self.token = token;
    }
    auto preprocessRequestBody() {
        return {\"token\": token};
    }
}

string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# asks for the response type of a type of its own class
string sub plant_response_type(object shared) {
    object legit = shared.getTypeForReference(\"Thing\");
    object r = shared.getResponseDataType(new TenantType(legit.uniqueHash()));
    return sprintf(\"the tenant gets a type of class %s\", r.className());
}

# builds a type of the shared schema with a REST client object of its own class
list<string> sub build_with_rest(object shared, Queue start, Queue done) {
    events = ();
    try {
        object t = shared.getTypeForReference(\"Thing\", True, new TenantRest(shared, start, done));
        events += sprintf(\"the type is built with the default %y\", t.getFields().token.getDefaultValue());
    } catch (hash<ExceptionInfo> ex) {
        events += sprintf(\"the build: %s\", ex.err);
    }
    return events;
}

list<string> sub get_events() {
    return events;
}

# changes a type that the shared schema built for it
list<string> sub change_type(object shared) {
    object t = shared.getTypeForReference(\"Thing\");
    return (
        \"addField(): \" + try_change(sub () {
            t.addField(new QoreDataField(\"tenant\", \"x\", AbstractDataProviderType::get(\"string\")));
        }),
        \"Reflection: \" + try_change(sub () {
            cast<Reflection::NormalMember>(Reflection::Class::forName(\"DataProvider::QoreDataType\")
                .getMember(\"name\")).setValue(t, \"tenant\");
        }),
        \"a field: \" + try_change(sub () { t.getFields().name.setDefaultValue(\"tenant\"); }),
    );
}

# builds types and fields of the shared schema from data of its own
string sub build_types(object shared, int n) {
    for (int i = 0; i < n; ++i) {
        shared.getType(sprintf(\"/tenant/%d\", i), \"string\");
    }
    return \"built\";
}

string sub build_fields(object so, int n) {
    for (int i = 0; i < n; ++i) {
        so.getField(sprintf(\"tenant-%d\", i), sprintf(\"desc %d\", i), True, NOTHING, sprintf(\"default %d\", i));
    }
    return \"built\";
}

object sub get_field(object so, string name) {
    return so.getField(name);
}

object sub get_type(object shared, string path, string typestr) {
    return shared.getType(path, typestr);
}

*string sub token_default(object shared, *object rest) {
    return shared.getTypeForReference(\"Thing\", True, rest).getFields().token.getDefaultValue();
}

*string sub own_token_default(object shared, string token) {
    return token_default(shared, new TenantTokenRest(token));
}

# the field of a required property with a default of the tenant's REST client
string sub own_token_field(object shared, string token) {
    object f = shared.getTypeForReference(\"Thing\", True, new TenantTokenRest(token)).getFields().token;
    return sprintf(\"default %y, mandatory %y, preselected %y\", f.getDefaultValue(), f.isMandatory(),
        f.getAttributes().preselected);
}

# a REST client object of the tenant whose token changes between calls, as when it gets a new one
class ChangingRest {
    private {
        string token = \"first\";
    }
    setToken(string token) {
        self.token = token;
    }
    auto preprocessRequestBody() {
        return {\"token\": token};
    }
}

# the defaults of one REST client object that change between calls
string sub changing_defaults(object shared) {
    ChangingRest r();
    *string first = token_default(shared, r);
    r.setToken(\"second\");
    return sprintf(\"%y, then %y\", first, token_default(shared, r));
}

# a REST client object of the tenant whose request bodies carry objects for a required and an optional property
class TenantMetaRest {
    auto preprocessRequestBody() {
        return {\"meta\": {\"k\": \"tenant\"}, \"note\": {\"k\": \"tenant\"}};
    }
}

# the fields of a required and an optional object property with defaults of the tenant's REST client
string sub own_object_fields(object shared) {
    hash<auto> fields = shared.getTypeForReference(\"Auth\", True, new TenantMetaRest()).getFields();
    return foldl $1 + \"; \" + $2, (map sprintf(\"%s: default %y, required %y\", $1, fields{$1}.getDefaultValue(),
        fields{$1}.getAttributes().required), (\"meta\", \"note\"));
}
";

Program sub tenant() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    p.setSandboxManager(new SandboxManager());
    p.parse(TenantCode, "tenant");
    return p;
}

# the size of a private hash member of an object, read with Reflection by trusted test code
int sub member_size(object o, string name) {
    foreach Class c in (Class::getClass(o).getClassHierarchy()) {
        foreach AbstractClassMember m in (c.getMembers()) {
            if (m.getName() == name && m instanceof NormalMember) {
                return cast<NormalMember>(m).getValue(o).size();
            }
        }
    }
    return -1;
}

Program p = tenant();

# the response types of a shared Swagger schema
{
    SwaggerSchema shared = SwaggerLoader::fromString(make_json(SwaggerDoc), True);
    printf("a tenant asks for the response type of a type of its own class: %s\n",
        p.callFunction("plant_response_type", shared));
    object legit = shared.getTypeForReference("Thing");
    object resp = shared.getResponseDataType(legit);
    printf("trusted code gets the response type of class %s, the same object again: %y, response types cached: %d\n",
        resp.className(), resp == shared.getResponseDataType(legit), member_size(shared, "response_types"));
}

# a type build of a shared OpenAPI 3 schema with a REST client object of the tenant's class
{
    OpenApi3Schema shared = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
    Queue start();
    Queue done();
    background sub () {
        start.get();
        shared.getTypeForReference("Other");
        done.push("done");
    }();
    map printf("%s\n", $1), p.callFunction("build_with_rest", shared, start, done);
    # trusted code releases the opaque caches of the schema
    shared.stopOpaqueTypeCaches();
    printf("the events after trusted code released the caches: %y\n", p.callFunction("get_events"));
    printf("the shared type has no default: %y\n", shared.getTypeForReference("Thing").getFields().token
        .getDefaultValue());
}

# a type that a tenant's build of a shared schema cached
{
    OpenApi3Schema shared = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
    map printf("a tenant changes a type of the shared schema that it built: %s\n", $1),
        p.callFunction("change_type", shared);
    object t = shared.getTypeForReference("Thing");
    printf("trusted code sees the fields %y, the name %y, shared: %y\n", keys t.getFields(), t.getName(),
        SandboxManager::isSharedObject(t));
}

# the types and fields that a tenant builds from its own data
{
    OpenApi3Schema shared = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
    object so = shared.components.schemas.Thing.properties.name;
    int types = member_size(shared, "typemap");
    printf("a tenant builds types from its strings: %s, the schema's type cache grew: %y\n",
        p.callFunction("build_types", shared, 50), member_size(shared, "typemap") > types);
    int fields = member_size(so, "fieldmap");
    int uses = member_size(so, "usemap");
    printf("a tenant builds fields from its strings and defaults: %s, the schema's field caches grew: %y\n",
        p.callFunction("build_fields", so, 50), member_size(so, "fieldmap") > fields || member_size(so, "usemap") > uses);
    # the field of the property in the type that a build of the schema cached
    object field = shared.getTypeForReference("Thing").getFields().name;
    printf("the tenant gets the field that the schema cached: %y\n", p.callFunction("get_field", so, "name") == field);
    object tt = p.callFunction("get_type", shared, "/tenant/x", "integer");
    printf("the tenant gets the type that it built again: %y; another tenant gets its own: %y\n",
        tt == p.callFunction("get_type", shared, "/tenant/x", "integer"),
        tt != tenant().callFunction("get_type", shared, "/tenant/x", "integer"));
}

# a build that a tenant starts fills the schema's own caches, which every caller then reads
{
    OpenApi3Schema shared = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
    p.callFunction("token_default", shared);
    printf("a tenant's build: the document's type cache has entries: %y, the field cache of a property: %y\n",
        member_size(shared, "typemap") > 0,
        member_size(shared.components.schemas.Thing.properties.name, "fieldmap") > 0);
}

# the request body defaults of REST clients
{
    OpenApi3Schema shared = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
    printf("the host builds a type with its REST client: default %y\n",
        shared.getTypeForReference("Thing", True, new HostRest()).getFields().token.getDefaultValue());
    printf("a tenant builds the type without a REST client: default %y\n", p.callFunction("token_default", shared));
    printf("trusted code builds the type without a REST client: default %y\n",
        shared.getTypeForReference("Thing").getFields().token.getDefaultValue());
    Program p2 = tenant();
    printf("tenants with their own REST clients: %y, %y; the first one again: %y\n",
        p.callFunction("own_token_default", shared, "tenant-1"), p2.callFunction("own_token_default", shared, "tenant-2"),
        p.callFunction("own_token_default", shared, "tenant-1"));
    printf("a tenant's REST client whose defaults change: %s\n", p.callFunction("changing_defaults", shared));
    object schema_field = shared.getTypeForReference("Thing").getFields().token;
    printf("the field of a required property with a tenant's default: %s; the schema's field: mandatory %y, "
        "preselected %y\n", p.callFunction("own_token_field", shared, "tenant-1"), schema_field.isMandatory(),
        schema_field.getAttributes().preselected);
    hash<auto> auth_fields = shared.getTypeForReference("Auth").getFields();
    printf("the object fields with a tenant's defaults: %s; the schema's: meta required %y, note required %y\n",
        p.callFunction("own_object_fields", shared), auth_fields.meta.getAttributes().required,
        auth_fields.note.getAttributes().required);
    object field = shared.components.schemas.Thing.getField("thing", NOTHING, True, new HostRest());
    printf("a field with the host's REST client: default %y; without: %y\n",
        field.getType().getFields().token.getDefaultValue(),
        shared.components.schemas.Thing.getField("thing").getType().getFields().token.getDefaultValue());
}
