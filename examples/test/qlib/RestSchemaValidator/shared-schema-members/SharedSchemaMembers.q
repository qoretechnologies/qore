# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Sandboxed code cannot change the public members of a REST or AsyncAPI schema that trusted code created, which every
# Program of the process can share, or of its schema objects, nor call the methods that change it; it can read them,
# and it can change a schema that it created itself (qore#6843); see shared-schema-members.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires Swagger
%requires OpenApi3
%requires AsyncApi
%requires StripeDataProvider
%requires json

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "test", "version": "1.0"},
    "basePath": "/api",
    "paths": {
        "/things": {
            "get": {
                "parameters": ({"name": "q", "in": "query", "type": "string"},),
                "responses": {"200": {"description": "ok", "schema": {"$ref": "#/definitions/Thing"}}},
            },
        },
    },
    "definitions": {
        "Thing": {"type": "object", "description": "a thing", "properties": {"name": {"type": "string"}}},
    },
};

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {
        "/things": {
            "get": {
                "responses": {"200": {"description": "ok", "content": {"application/json": {
                    "schema": {"$ref": "#/components/schemas/Thing"},
                }}}},
            },
        },
    },
    "components": {
        "schemas": {
            "Thing": {"type": "object", "description": "a thing", "properties": {"name": {"type": "string"}}},
        },
    },
};

const AsyncApiDoc = {
    "asyncapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "components": {
        "schemas": {
            "Thing": {"type": "object", "description": "a thing", "properties": {"name": {"type": "string"}}},
        },
    },
};

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires Swagger
%requires OpenApi3
%requires AsyncApi
%requires StripeDataProvider
%requires json

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the schema object of the given schema for the reusable schema of the given name
object sub schema_object(object schema, string name) {
    if (schema instanceof SwaggerSchema) {
        return schema.definitions{name};
    }
    return schema.components.schemas{name};
}

# the path of an operation of a REST schema: the test schemas' or Stripe's
string sub op_path(object schema) {
    return schema.info.title == \"test\" ? \"/things\" : \"/v1/customers\";
}

# changes of a REST schema (Swagger or OpenAPI 3)
hash<string, code> sub rest_changes(string name) {
    return {
        \"basePath\": sub (object s) { s.basePath = \"/injected\"; },
        \"setBasePath()\": sub (object s) { s.setBasePath(\"/injected\"); },
        \"setTimeZoneLocale()\": sub (object s) { s.setTimeZoneLocale(new TimeZone(\"Europe/Prague\")); },
        \"host\": sub (object s) { s.host = \"injected\"; },
        \"info.title\": sub (object s) { s.info.title = \"injected\"; },
        \"info object's title\": sub (object s) { object info = s.info; info.title = \"injected\"; },
        \"reusable schemas\": sub (object s) {
            if (s instanceof SwaggerSchema) {
                remove s.definitions{name};
            } else {
                remove s.components.schemas{name};
            }
        },
        \"addParameter()\": sub (object s) {
            object op = s.paths.match(op_path(s)).getOperation(\"get\", op_path(s));
            if (s instanceof SwaggerSchema) {
                op.addParameter(\"injected\", op.parameters.firstValue());
            } else {
                op.addParameter(\"injected\", OpenApi3::ParameterObject::newParameter(\"injected\",
                    {\"name\": \"injected\", \"in\": \"query\", \"schema\": {\"type\": \"string\"}}, s));
            }
        },
        \"schema object's desc\": sub (object s) { object so = schema_object(s, name); so.desc = \"injected\"; },
        \"schema object's dataType\": sub (object s) { object so = schema_object(s, name); so.dataType = NOTHING; },
        \"schema object's orNothingDataType\": sub (object s) {
            object so = schema_object(s, name);
            so.orNothingDataType = so.dataType;
        },
        \"schema object's properties\": sub (object s) { object so = schema_object(s, name); so.properties = {}; },
        \"schema object's required\": sub (object s) { object so = schema_object(s, name); so.required += (\"injected\",); },
        \"schema object's items\": sub (object s) { object so = schema_object(s, name); so.items = schema_object(s, name); },
        \"schema object's additionalProperties\": sub (object s) {
            object so = schema_object(s, name);
            so.additionalProperties = True;
        },
        \"schema object's example\": sub (object s) { object so = schema_object(s, name); so.example = \"injected\"; },
        \"schema object's type\": sub (object s) { object so = schema_object(s, name); so.type = \"string\"; },
        \"delete the schema\": sub (object s) { delete s; },
    };
}

# changes of an AsyncAPI schema
hash<string, code> sub async_changes() {
    return {
        \"info.title\": sub (object s) { s.info.title = \"injected\"; },
        \"defaultContentType\": sub (object s) { s.defaultContentType = \"injected\"; },
        \"components object's schemas\": sub (object s) { object c = s.components; c.schemas = {}; },
        \"schema object's desc\": sub (object s) { s.components.schemas.Thing.desc = \"injected\"; },
        \"schema object's properties\": sub (object s) {
            object so = s.components.schemas.Thing;
            so.properties = {};
        },
        \"delete the schema\": sub (object s) { delete s; },
    };
}

# the names of the changes of a REST schema (with the name of a reusable schema) or of an AsyncAPI schema
list<string> sub change_names(*string name) {
    return keys (name ? rest_changes(name) : async_changes());
}

# tries the given change of a schema
string sub try_one(string change, object schema, *string name) {
    return try_change(sub () { (name ? rest_changes(name) : async_changes()){change}(schema); });
}

# reads of a schema
string sub reads(object schema, *string name) {
    if (!name) {
        return sprintf(\"%y %y %y\", schema.info.title, schema.components.schemas.Thing.desc,
            keys schema.components.schemas.Thing.properties);
    }
    object so = schema_object(schema, name);
    return sprintf(\"%y %y %y %y\", schema.getBasePath(), schema.info.title, so.desc, keys so.properties);
}

# a schema of the tenant's own
object sub make_schema(string kind, hash<auto> doc) {
    switch (kind) {
        case \"Swagger\": return SwaggerLoader::fromString(make_json(doc), True);
        case \"OpenAPI 3\": return OpenApi3Loader::fromString(make_json(doc), True);
    }
    return AsyncApi3Loader::fromString(make_json(doc), True);
}

# tries the given change of a schema of the tenant's own
string sub try_own(string kind, hash<auto> doc, string change) {
    return try_one(change, make_schema(kind, doc), kind == \"AsyncAPI\" ? NOTHING : \"Thing\");
}

# the Stripe schema, which the module builds on first use for every Program
object sub stripe_schema() {
    return StripeSchema::getSchema();
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# the path of an operation of a REST schema: the test schemas' or Stripe's
string sub op_path(object schema) {
    return schema.info.title == "test" ? "/things" : "/v1/customers";
}

# the identity of a value: an object is given by its class and identity, never formatted with its members, as a
# schema object can reach a large graph of shared schema objects, and a change can make it refer to itself
auto sub ident(auto v) {
    return v.typeCode() == NT_OBJECT ? sprintf("<%s %s>", v.className(), v.uniqueHash()) : v;
}

# the snapshot of the members of a REST schema that the changes change; values only, no object is formatted
string sub rest_snapshot(object schema, string name) {
    object op = schema.paths.match(op_path(schema)).getOperation("get", op_path(schema));
    string rv = sprintf("%y %y %y %y %y %y", schema.basePath, schema.getBasePath(), schema.host, schema.info.title,
        (exists (auto tz = schema.getTimeZoneLocale()) ? tz.region() : NOTHING), op.parameters.size());
    *object so = schema instanceof SwaggerSchema ? schema.definitions{name} : schema.components.schemas{name};
    if (!so) {
        return rv + " no reusable schema";
    }
    return rv + sprintf(" %y %y %y %y %y %y %y %y %y", so.desc, so.dataType.typeCode(), so.orNothingDataType.typeCode(),
        keys so.properties, so.required,
        ident(so.items), ident(so.additionalProperties), ident(so.example), so.type);
}

string sub async_snapshot(object schema) {
    object so = schema.components.schemas.Thing;
    return sprintf("%y %y %y %y", schema.info.title, schema.defaultContentType, so.desc, keys so.properties);
}

const Docs = {
    "Swagger": SwaggerDoc,
    "OpenAPI 3": OpenApi3Doc,
    "AsyncAPI": AsyncApiDoc,
};

# a schema that trusted code creates, which every Program of the process can share
object sub make_schema(string kind) {
    switch (kind) {
        case "Swagger": return SwaggerLoader::fromString(make_json(SwaggerDoc), True);
        case "OpenAPI 3": return OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
    }
    return AsyncApi3Loader::fromString(make_json(AsyncApiDoc), True);
}

string sub snapshot(auto schema, *string name) {
    try {
        return name ? rest_snapshot(schema, name) : async_snapshot(schema);
    } catch (hash<ExceptionInfo> ex) {
        # the tenant deleted the schema
        return ex.err;
    }
}

# each change of a shared schema of each kind, and of the tenant's own
foreach string kind in (keys Docs) {
    *string name = kind == "AsyncAPI" ? NOTHING : "Thing";
    printf("%s:\n", kind);
    foreach string change in (p.callFunction("change_names", name)) {
        object schema = make_schema(kind);
        if (name) {
            # a type of the schema, so that its cached type is checked
            schema.getTypeForReference(name);
        }
        string before = snapshot(schema, name);
        string rv = p.callFunction("try_one", change, schema, name);
        printf("  %s: shared: %s, %s; own: %s\n", change, rv, snapshot(schema, name) == before ? "unchanged" : "CHANGED",
            p.callFunction("try_own", kind, Docs{kind}, change));
    }
    printf("  reads of a shared schema: %s\n", p.callFunction("reads", make_schema(kind), name));
}

# the Stripe schema, which the module builds for every Program on first use, here by the tenant; only the changes that
# leave the schema usable are tried, so that a change that is not refused does not hide the others
object stripe = p.callFunction("stripe_schema");
printf("Stripe:\n");
foreach string change in (p.callFunction("change_names", "customer")) {
    if (change == "delete the schema" || change == "reusable schemas") {
        continue;
    }
    string before = rest_snapshot(stripe, "customer");
    string rv = p.callFunction("try_one", change, stripe, "customer");
    printf("  %s: shared: %s, %s\n", change, rv, rest_snapshot(stripe, "customer") == before ? "unchanged" : "CHANGED");
}

# trusted code can change a shared schema
SwaggerSchema trusted_schema = make_schema("Swagger");
trusted_schema.setBasePath("/trusted");
trusted_schema.definitions.Thing.desc = "trusted";
printf("trusted code changes a shared schema: %y %y\n", trusted_schema.getBasePath(),
    trusted_schema.definitions.Thing.desc);
