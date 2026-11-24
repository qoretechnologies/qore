# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A shared REST schema whose document is destroyed, the values of the per-Program caches, the variants of REST client
# defaults that serialize alike, deserialized schemas, and a document whose last reference a tenant releases: no code
# of a tenant can change what other Programs get, and a destroyed document releases its caches (qore#6843); see
# shared-schema-orphans.qtest

%modern
%enable-all-warnings
%no-child-restrictions
%allow-weak-references

%requires OpenApi3
%requires json

hash<auto> sub spec() {
    return {
        "openapi": "3.0.0", "info": {"title": "t", "version": "1"}, "paths": {},
        "components": {"schemas": {
            "Thing": {"type": "object", "required": ("token", "name"), "properties": {
                "token": {"type": "string"}, "name": {"type": "string"},
                "u": {"oneOf": ({"$ref": "#/components/schemas/A"}, {"$ref": "#/components/schemas/B"})},
            }},
            "Other": {"type": "object", "properties": {"token": {"type": "string"}}},
            "Ver": {"type": "object", "properties": {"ver": {"type": "string"}, "name": {"type": "string"}}},
            "A": {"type": "object", "properties": {"a": {"type": "string"}}},
            "B": {"type": "object", "properties": {"b": {"type": "integer"}}},
        }},
    };
}

OpenApi3Schema sub new_doc() {
    return OpenApi3Loader::fromString(make_json(spec()), True);
}

our *OpenApi3Schema shared;
*OpenApi3Schema sub get_shared() { return shared; }

class Box { public { *OpenApi3Schema doc; } }
our Box box();
Box sub get_box() { return box; }

#! a REST client of the host
class HostRest {
    private { hash<auto> d; }
    constructor(hash<auto> d) { self.d = d; }
    auto preprocessRequestBody() { return d; }
}

#! host code that a tenant calls back: asks with the host's own client
string sub host_cb() {
    AbstractDataProviderType t = shared.getTypeForReference("Ver", True, new HostRest({"ver": "v2"}));
    return sprintf("fields %y, default %y", keys t.getFields(), t.getFields().ver.getDefaultValue());
}

class WeakRef {
    private { *object o; }
    constructor(object o) { self.o := o; }
    bool alive() { return exists o; }
}

const TenantCode = "
%requires OpenApi3
%requires DataProvider
class TRest {
    private { auto d; }
    constructor(auto d) { self.d = d; }
    auto preprocessRequestBody() { return d; }
}
string sub attempt(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
AbstractDataField sub evil() {
    return new QoreDataField({\"name\": \"evil\", \"type\": AbstractDataProviderTypeMap.\"string\"});
}
# changes of the schema's type and of a type of the tenant's per-Program cache
string sub change_types() {
    AbstractDataProviderType t = get_shared().getTypeForReference(\"Thing\");
    AbstractDataProviderType v = get_shared().getTypeForReference(\"Ver\", True, new TRest({\"ver\": \"v2\"}));
    AbstractDataProviderType p = get_shared().getType(\"/tenant/path\", \"string\");
    AbstractDataField f = get_shared().components.schemas.Other.getField(\"f\", \"a field\");
    return sprintf(\"schema type: %s; variant: %s; variant field: %s; caller type: %s; caller field: %s\",
        attempt(sub () { cast<HashDataType>(t).addField(evil()); }),
        attempt(sub () { cast<HashDataType>(v).addField(evil()); }),
        attempt(sub () { v.getFields().ver.setDefaultValue(\"tenant\"); }),
        attempt(sub () { p.setOption(\"qore.no_null\", True); }),
        attempt(sub () { f.setDefaultValue(\"tenant\"); }));
}
string sub cb() { return host_cb(); }
*SchemaObject sub grab(string n) { return get_shared().components.schemas{n}; }
string sub orphan(SchemaObject so) {
    string built;
    try {
        AbstractDataProviderType t = so.getDataType(False);
        built = sprintf(\"built %y\", keys t.getFields());
    } catch (hash<ExceptionInfo> ex) {
        built = ex.err;
    }
    return built + \", change: \" + attempt(sub () { cast<HashDataType>(so.getDataType(False)).addField(evil()); });
}
# the defaults of two clients that serialize alike
string sub alike() {
    auto a = get_shared().getTypeForReference(\"Ver\", True, new TRest({\"ver\": 0.1 + 0.2})).getFields().ver
        .getDefaultValue();
    auto b = get_shared().getTypeForReference(\"Ver\", True, new TRest({\"ver\": 0.3})).getFields().ver
        .getDefaultValue();
    return sprintf(\"first is 0.1 + 0.2: %y, second is 0.3: %y\", a === 0.1 + 0.2, b === 0.3);
}
*string sub token_default() {
    return get_shared().getTypeForReference(\"Thing\").getFields().token.getDefaultValue();
}
string sub change_restored() {
    return attempt(sub () { cast<HashDataType>(get_shared().getTypeForReference(\"Thing\")).addField(evil()); });
}
our *OpenApi3Schema held;
SchemaObject sub take() {
    held = get_box().doc;
    held.getTypeForReference(\"Thing\", True, new TRest({\"token\": \"t\"}));
    return held.components.schemas.Thing;
}
string sub drop() {
    try {
        remove held;
        return \"released\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

Program sub tenant() {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS);
    p.setSandboxManager(new SandboxManager());
    p.importFunction("get_shared");
    p.importFunction("get_box");
    p.importFunction("host_cb");
    p.importClass("Box");
    p.parse(TenantCode, "tenant");
    return p;
}

# 1: what a tenant gets cannot be changed: the schema's types and the values of its per-Program cache
shared = new_doc();
Program t1 = tenant();
Program t2 = tenant();
printf("tenant changes: %s\n", t1.callFunction("change_types"));
# host code that the tenant calls back gets the variant from the tenant's per-Program cache
printf("host code called back by the tenant: %s\n", t1.callFunction("cb"));
printf("host code: %s\n", host_cb());

# 2: schema objects whose document is destroyed (which released their caches): they build nothing and write no cache
shared.getTypeForReference("Thing");
object built = t1.callFunction("grab", "Thing");
object other1 = t1.callFunction("grab", "Other");
object other2 = t2.callFunction("grab", "Other");
remove shared;
printf("orphan built before the document was destroyed, tenant: %s\n", t1.callFunction("orphan", built));
printf("orphan without a cached type, tenant: %s\n", t1.callFunction("orphan", other1));
printf("orphan without a cached type, another tenant: %s\n", t2.callFunction("orphan", other2));
try {
    printf("orphan without a cached type, host: %y\n", keys other1.getDataType(False).getFields());
} catch (hash<ExceptionInfo> ex) {
    printf("orphan without a cached type, host: %s\n", ex.err);
}
remove built;
remove other1;
remove other2;

# 3: client defaults that serialize alike get their own variants
shared = new_doc();
printf("defaults that serialize alike: %s\n", t1.callFunction("alike"));

# 4: a deserialized schema is shared as the one it was serialized from, and its types are built again, frozen
{
    OpenApi3Schema orig = new_doc();
    orig.getTypeForReference("Thing");
    shared = Serializable::deserialize(Serializable::serialize(orig));
    printf("deserialized: shared %y, its schema objects shared %y\n", SandboxManager::isSharedObject(shared),
        SandboxManager::isSharedObject(shared.components.schemas.Thing));
    printf("host type with its client: default %y\n",
        shared.getTypeForReference("Thing", True, new HostRest({"token": "host-secret"})).getFields().token
            .getDefaultValue());
    printf("tenant sees the default: %y; changes the type: %s\n", t2.callFunction("token_default"),
        t2.callFunction("change_restored"));
}

# 5: a shared document whose last reference a tenant releases releases its caches
{
    box.doc = new_doc();
    box.doc.getTypeForReference("Thing");
    object so = t1.callFunction("take");
    WeakRef doc(box.doc);
    remove box.doc;
    printf("the tenant releases the last reference: %s; the document is destroyed: %y\n", t1.callFunction("drop"),
        !doc.alive());
    try {
        printf("its schema object's type: %y\n", keys so.getDataType().getFields());
    } catch (hash<ExceptionInfo> ex) {
        printf("its schema object's type: %s (its caches were released)\n", ex.err);
    }
}
remove shared;
delete t1;
delete t2;
