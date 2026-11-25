# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The types and fields that a shared REST schema builds for its callers outside its type builds, kept in the
# per-Program cache of each caller, are released with the schema: in a trusted caller's cache when the schema is
# destroyed, in a tenant's cache by the tenant's next use of its cache, in the tenant's own thread, or with the
# tenant's Program; only the code of a class of an object can release its values (qore#6843); see
# shared-schema-release.qtest

%modern
%enable-all-warnings
%no-child-restrictions
%allow-weak-references

%requires OpenApi3
%requires Swagger
%requires json

const OpenApi3Doc = {
    "openapi": "3.0.3",
    "info": {"title": "release", "version": "1"},
    "paths": {},
    "components": {"schemas": {
        "Customer": {
            "type": "object",
            "properties": {
                "id": {"type": "string"},
                "default_source": {"anyOf": ({"type": "string"}, {"\$ref": "#/components/schemas/Card"})},
            },
        },
        "Subscription": {
            "type": "object",
            "properties": {
                "id": {"type": "string"},
                "customer": {"anyOf": ({"type": "string"}, {"\$ref": "#/components/schemas/Customer"})},
            },
        },
        "Card": {
            "type": "object",
            "properties": {
                "id": {"type": "string"},
                "customer": {"anyOf": ({"type": "string"}, {"\$ref": "#/components/schemas/Customer"})},
            },
        },
    }},
};

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "release", "version": "1"},
    "paths": {},
    "definitions": {
        "Pet": {"type": "object", "properties": {"id": {"type": "string"}, "tags": {"type": "array",
            "items": {"\$ref": "#/definitions/Tag"}}}},
        "Tag": {"type": "object", "properties": {"name": {"type": "string"}}},
    },
};

#! Returns a new OpenAPI 3 document; shared, as trusted code creates it
OpenApi3Schema sub new_openapi3() {
    return OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
}

#! Returns True if the weak reference under the key gives an object that exists
bool sub alive(hash<auto> weak, string key) {
    try {
        return is_object_arg(weak{key});
    } catch (hash<ExceptionInfo> ex) {
        if (ex.err == "RUNTIME-TYPE-ERROR" || ex.err == "RUNTIME-OVERLOAD-ERROR") {
            return False;
        }
        rethrow;
    }
}

bool sub is_object_arg(object o) {
    return True;
}

#! Returns the keys whose weak references give an object that exists
string sub alive_keys(hash<auto> weak) {
    list<string> l = sort(map $1, keys weak, alive(weak, $1));
    return l ? join(", ", l) : "none";
}

#! the host document that the tenants use, and where the destructors of the tenants' objects ran
our *OpenApi3Schema host_doc;
our list<string> dtor_log = ();
our string phase = "setup";
our int host_tid = gettid();

OpenApi3Schema sub get_host_doc() {
    return host_doc;
}

sub record_dtor(string name) {
    dtor_log += sprintf("%s: in the %s thread, during %s", name, gettid() == host_tid ? "host" : "tenant", phase);
}

#! a class of the main Program that keeps values in the per-Program cache, for the release checks
public class CacheOwner {
    set(string key, auto value, *object group) {
        SandboxManager::setProgramCacheValue(self, key, value, 64, group);
    }

    auto get(string key) {
        return SandboxManager::getProgramCacheValue(self, key);
    }

    static release(object o) {
        SandboxManager::releaseProgramCacheValues(o);
    }
}

string sub try_call(code c) {
    try {
        c();
        return "allowed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# 1: a trusted caller: the document and everything that its schema objects built for the caller are released with it
{
    hash<auto> weak = {};
    {
        OpenApi3Schema doc = new_openapi3();
        SchemaObject subscription = doc.components.schemas.Subscription;
        AbstractDataField body = doc.getFieldFromSchema("body", "the body", subscription);
        AbstractDataField with_default = subscription.properties.id.getField("id", "an id", False, NOTHING, "x");
        AbstractDataProviderType union = body.getType().getFields().customer.getType();
        AbstractDataProviderType str = doc.getType("/release/path", "string");
        weak.doc := doc;
        weak.subscription := subscription;
        weak.body := body;
        weak.with_default := with_default;
        weak.union := union;
        weak.string := str;
        printf("openapi3 trusted, shared %y, while held: %s\n", SandboxManager::isSharedObject(doc),
            alive_keys(weak));
    }
    printf("openapi3 trusted, released: %s\n", alive_keys(weak));
}

# the same for a Swagger document
{
    hash<auto> weak = {};
    {
        SwaggerSchema doc = SwaggerLoader::fromString(make_json(SwaggerDoc), True);
        Swagger::SchemaObject pet = doc.definitions.Pet;
        AbstractDataField field = pet.getField("pet", "a pet", False);
        AbstractDataProviderType str = doc.getType("/release/path", "string");
        weak.doc := doc;
        weak.pet := pet;
        weak.field := field;
        weak.string := str;
        printf("swagger trusted, shared %y, while held: %s\n", SandboxManager::isSharedObject(doc),
            alive_keys(weak));
    }
    printf("swagger trusted, released: %s\n", alive_keys(weak));
}

# 2: tenants using a host document
const TenantCode = "
%modern
%requires OpenApi3

# a default value of the tenant, which reports where it is destroyed
class Marker {
    private { string name; }
    constructor(string name) { self.name = name; }
    destructor() { record_dtor(name); }
}

# asks the host document for a field with a default value object of its own
AbstractDataField sub use_host(string name) {
    OpenApi3Schema doc = get_host_doc();
    return doc.components.schemas.Subscription.getField(name, \"a field\", False, NOTHING, new Marker(name));
}

# any use of the tenant's per-Program cache: a field of the host document
sub touch_cache() {
    get_host_doc().components.schemas.Card.getField(\"card\", \"a card\");
}

# tries to release the values of the host document
string sub try_release() {
    try {
        SandboxManager::releaseProgramCacheValues(get_host_doc());
        return \"allowed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

Program sub new_tenant() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    p.setSandboxManager(new SandboxManager());
    p.importFunction("get_host_doc");
    p.importFunction("record_dtor");
    p.parse(TenantCode, "tenant");
    return p;
}

{
    host_doc = new_openapi3();
    hash<auto> weak = {};
    Program tenant = new_tenant();
    Program other = new_tenant();
    {
        AbstractDataField f = tenant.callFunction("use_host", "tenant-field");
        AbstractDataField g = other.callFunction("use_host", "other-field");
        weak.tenant_field := f;
        weak.other_field := g;
    }
    weak.doc := host_doc;
    printf("tenant callers, while the host holds the document: %s\n", alive_keys(weak));
    # the host releases the document: the tenants' values are kept as pending, and no tenant code runs here
    phase = "the host's release of the document";
    remove host_doc;
    printf("tenant callers, the document released: %s; destructors run: %s\n", alive_keys(weak),
        dtor_log ? join("; ", dtor_log) : "none");
    # the next use of the tenant's cache, in a thread of its own, releases them there
    host_doc = new_openapi3();
    phase = "the tenant's next use of its cache";
    Counter c(1);
    background sub () {
        on_exit c.dec();
        tenant.callFunction("touch_cache");
    }();
    c.waitForZero();
    printf("after the tenant's use of its cache: %s; destructors: %s\n", alive_keys(weak), join("; ", dtor_log));
    # the other tenant never uses its cache again: its pending values are released with its Program
    dtor_log = ();
    phase = "the deletion of the tenant's Program";
    delete other;
    printf("after the other tenant's Program is deleted: %s; destructors: %s\n", alive_keys(weak),
        join("; ", dtor_log));
    remove host_doc;
    delete tenant;
}

# 3: only the code of a class of an object releases its values
{
    OpenApi3Schema doc = new_openapi3();
    printf("release of a document's values by a class of another Program: %s\n",
        try_call(sub () { CacheOwner::release(doc); }));
    host_doc = doc;
    Program p = new_tenant();
    printf("release of a document's values by a tenant: %s\n", p.callFunction("try_release"));
    Program trusted(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    trusted.importFunction("get_host_doc");
    trusted.parse("%requires OpenApi3\nstring sub try_release() { try { "
        "SandboxManager::releaseProgramCacheValues(get_host_doc()); return \"allowed\"; } "
        "catch (hash<ExceptionInfo> ex) { return ex.err; } }", "release");
    printf("release of a document's values by trusted code of another Program: %s\n",
        trusted.callFunction("try_release"));
    remove host_doc;

    # an owner of this Program's class, with another object of its class as its group
    CacheOwner owner();
    CacheOwner group();
    owner.set("k", "value", group);
    printf("a value set with a group: %y\n", owner.get("k"));
    printf("a group of another Program's class: %s\n", try_call(sub () { owner.set("j", "value", doc); }));
    CacheOwner::release(group);
    printf("after the release of its group: %y\n", owner.get("k"));
    owner.set("k", "again");
    CacheOwner::release(owner);
    printf("after the release of the owner: %y\n", owner.get("k"));
}
