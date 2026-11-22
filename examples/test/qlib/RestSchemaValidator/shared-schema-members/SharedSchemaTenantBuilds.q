# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A sandboxed Program builds the types of shared REST schemas, which the schemas cache in update scopes, and validates
# requests and responses with them: every build and validation works, as for trusted code (qore#6843); see
# shared-schema-tenant-builds.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires Swagger
%requires OpenApi3
%requires StripeDataProvider
%requires json

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "test", "version": "1.0"},
    "basePath": "/api",
    "consumes": ("application/json",),
    "produces": ("application/json",),
    "paths": {
        "/things/{id}": {
            "post": {
                "parameters": (
                    {"name": "id", "in": "path", "type": "string", "required": True},
                    {"name": "body", "in": "body", "required": True, "schema": {"$ref": "#/definitions/Thing"}},
                ),
                "responses": {"200": {"description": "ok", "schema": {"$ref": "#/definitions/Thing"}}},
            },
        },
    },
    "definitions": {
        "Thing": {"type": "object", "required": ("name",), "properties": {
            "name": {"type": "string", "maxLength": 10, "pattern": "^[a-z]+$"},
            "tags": {"type": "array", "items": {"type": "string"}},
            "inner": {"type": "object", "readOnly": True, "properties": {"a": {"type": "number"}}},
        }},
    },
};

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {
        "/things/{id}": {
            "post": {
                "parameters": ({"name": "id", "in": "path", "required": True, "schema": {"type": "string"}},),
                "requestBody": {"required": True, "content": {"application/json": {
                    "schema": {"$ref": "#/components/schemas/Thing"},
                }}},
                "responses": {"200": {"description": "ok", "content": {"application/json": {
                    "schema": {"$ref": "#/components/schemas/Thing"},
                }}}},
            },
        },
    },
    "components": {
        "schemas": {
            "Thing": {"type": "object", "required": ("name",), "properties": {
                "name": {"type": "string", "maxLength": 10, "pattern": "^[a-z]+$"},
                "tags": {"type": "array", "items": {"type": "string"}},
                "kind": {"oneOf": ({"type": "string"}, {"type": "integer"})},
                "inner": {"type": "object", "readOnly": True, "properties": {"a": {"type": "number"}}},
            }},
        },
    },
};

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires Swagger
%requires OpenApi3
%requires StripeDataProvider
%requires json

string sub try_build(code c) {
    try {
        return c();
    } catch (hash<ExceptionInfo> ex) {
        return sprintf(\"%s: %s\", ex.err, ex.desc);
    }
}

# builds the types of a schema and validates a request and a response with it
string sub build_and_validate(object schema) {
    list<string> l = ();
    l += try_build(string sub () {
        return schema.getTypeForReference(\"Thing\").getName();
    });
    l += try_build(string sub () {
        hash<auto> body = {\"name\": \"thing\", \"tags\": (\"a\",), \"kind\": 1};
        if (schema instanceof SwaggerSchema) {
            remove body.kind;
        }
        schema.processRequest(\"POST\", \"/things/1\", body);
        hash<auto> hdr = {\"content-type\": \"application/json\"};
        return sprintf(\"%y\", keys schema.parseRequest(\"POST\", \"/things/1\", make_json(body), \\hdr).body);
    });
    l += try_build(string sub () {
        hash<auto> resp = schema.parseResponse(\"POST\", \"/things/1\", 200,
            make_json({\"name\": \"thing\", \"inner\": {\"a\": 1.5}}), {\"content-type\": \"application/json\"});
        return sprintf(\"%y\", keys resp.body);
    });
    l += try_build(string sub () {
        return sprintf(\"%y\", schema.processRequest(\"POST\", \"/things/1\", {\"name\": \"NOT VALID\"}).body);
    });
    return join(\"; \", l);
}

# builds the request and the response types of Stripe actions, which the module builds on first use
string sub stripe_builds() {
    object set = StripeSchema::getActionSet();
    list<string> l = ();
    foreach string action in (set.getActionNames()[0..4]) {
        l += try_build(string sub () {
            object op = set.getOperationEx(action);
            *object req = op.getRequestType();
            *object resp = op.getResponseType();
            return sprintf(\"%s: %y %y\", action, req ? req.getName() : NOTHING, resp ? resp.getName() : NOTHING);
        });
    }
    l += try_build(string sub () {
        return StripeSchema::getSchema().getTypeForReference(\"customer\").getName();
    });
    return join(\"\\n\", l);
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# schemas that trusted code creates, which every Program of the process can share; the tenant builds their types first
printf("Swagger: %s\n", p.callFunction("build_and_validate", SwaggerLoader::fromString(make_json(SwaggerDoc), True)));
printf("OpenAPI 3: %s\n", p.callFunction("build_and_validate",
    OpenApi3Loader::fromString(make_json(OpenApi3Doc), True)));
printf("Stripe:\n%s\n", p.callFunction("stripe_builds"));
