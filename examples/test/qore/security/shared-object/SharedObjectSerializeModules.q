# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The shared objects of the classes of the standard modules that guard their state with
# SandboxManager::isSharedObject(self) cannot be serialized by a tenant; trusted code serializes them as before
# (qore#7673); see shared-object-serialize.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider
%requires OpenApi3
%requires Swagger
%requires json

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# the result of a serialization, or its exception
string sub serialize_it(object o) {
    try {
        return sprintf(\"serialized (%s)\", Serializable::serialize(o).type());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# the tenant's and trusted code's serializations of the object
sub check(object o) {
    string trusted;
    try {
        Serializable::serialize(o);
        trusted = "serialized";
    } catch (hash<ExceptionInfo> ex) {
        trusted = ex.err;
    }
    printf("%s (shared: %y): the tenant: %s; trusted code: %s\n", o.className(), SandboxManager::isSharedObject(o),
        p.callFunction("serialize_it", o), trusted);
}

# DataProvider: AbstractDataProviderType and AbstractDataField
{
    HashDataType t();
    t.addField(new QoreDataField("a", NOTHING, StringType));
    t.freeze();
    check(t);
    QoreDataField f("f", NOTHING, StringType);
    f.freeze();
    check(f);
    # a type that is not shared is not refused as a shared object, but the non-public state of a class of a module
    # is not the tenant's to serialize (SERIALIZATION-ERROR, qore#7146)
    HashDataType own();
    own.addField(new QoreDataField("a", NOTHING, StringType));
    check(own);
}

# the REST schemas: AbstractRestSchemaValidator and AbstractRestSchemaObject, with the classes of OpenApi3 and Swagger
{
    OpenApi3Schema doc = OpenApi3Loader::fromString(make_json({
        "openapi": "3.0.0", "info": {"title": "t", "version": "1"}, "paths": {},
        "components": {"schemas": {
            "Thing": {"type": "object", "properties": {"name": {"type": "string"}}},
        }},
    }), True);
    check(doc);
    check(doc.components.schemas.Thing);
}

{
    SwaggerSchema doc = SwaggerLoader::fromString(make_json({
        "swagger": "2.0", "info": {"title": "t", "version": "1"}, "paths": {},
        "definitions": {
            "Thing": {"type": "object", "properties": {"name": {"type": "string"}}},
        },
    }), True);
    check(doc);
    check(doc.definitions.Thing);
}
