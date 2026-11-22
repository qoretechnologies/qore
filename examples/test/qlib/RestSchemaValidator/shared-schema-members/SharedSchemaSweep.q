# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A sandboxed Program calls every public method of every object of a shared Swagger, OpenAPI 3 and AsyncAPI schema
# (the classes of the RestSchemaValidator, Swagger, OpenApi3 and AsyncApi modules, and the methods that they inherit,
# such as those of LoggerWrapper), with arguments of the types that the method accepts: no call changes the shared
# schema, except for the caches that it builds from itself (qore#6843); see shared-schema-sweep.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires reflection
%requires Logger
%requires Swagger
%requires OpenApi3
%requires AsyncApi
%requires json

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "test", "version": "1.0", "x-test": True},
    "basePath": "/api",
    "paths": {
        "/things/{id}": {
            "get": {
                "parameters": (
                    {"name": "id", "in": "path", "type": "string", "required": True},
                    {"name": "q", "in": "query", "type": "string"},
                ),
                "responses": {"200": {"description": "ok", "schema": {"$ref": "#/definitions/Thing"}}},
            },
        },
    },
    "definitions": {
        "Thing": {"type": "object", "description": "a thing", "required": ("name",), "properties": {
            "name": {"type": "string", "maxLength": 10},
            "tags": {"type": "array", "items": {"type": "string"}},
        }},
    },
};

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0", "x-test": True},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {
        "/things/{id}": {
            "get": {
                "parameters": (
                    {"name": "id", "in": "path", "required": True, "schema": {"type": "string"}},
                    {"name": "q", "in": "query", "schema": {"type": "string"}},
                ),
                "responses": {"200": {"description": "ok", "content": {"application/json": {
                    "schema": {"$ref": "#/components/schemas/Thing"},
                }}}},
            },
        },
    },
    "components": {
        "schemas": {
            "Thing": {"type": "object", "description": "a thing", "required": ("name",), "properties": {
                "name": {"type": "string", "maxLength": 10},
                "tags": {"type": "array", "items": {"type": "string"}},
            }},
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

#! the namespaces whose objects are part of a schema
const SchemaNamespaces = ("RestSchemaValidator", "Swagger", "OpenApi3", "AsyncApi");

#! the members that hold caches that the schema builds from itself, which a call can fill
const CacheMembers = {
    "dataType": True, "orNothingDataType": True, "fieldmap": True, "usemap": True, "building": True,
    "holds_opaque_type_caches": True, "pattern_cache": True, "typemap": True, "response_types": True,
    "alternativeDataType": True, "type_build_undo": True, "type_build_lock": True, "opaque_type_caches": True,
    "opaque_type_cache_holders": True, "release_opaque_type_caches": True, "template_res": True,
    "shared_object": True,
};

#! a hash argument: a description with content that a method could add to a schema
const SweepHash = {
    "x-sweep": True,
    "description": "sweep",
    "type": "string",
    "name": "sweep",
    "in": "query",
    "delete": {"responses": {"204": {"description": "sweep"}}},
};

#! the methods that are not called: the destructor and the copy method, which no caller calls directly
const SkippedMethods = {"destructor": True, "copy": True, "constructor": True};

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# calls a method; the arguments that are \"CODE\" are closures of the tenant
string sub call_method(object obj, string method, list<auto> args) {
    args = map $1 === \"CODE\" ? sub () {} : $1, args;
    try {
        call_object_method_args(obj, method, args);
        return \"called\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

#! the logger that trusted code gives the shared schemas
const HostLogger = new Logger("host");

# a schema that trusted code creates, which every Program of the process can share
object sub make_schema(string kind) {
    object rv;
    switch (kind) {
        case "Swagger": rv = SwaggerLoader::fromString(make_json(SwaggerDoc), True); break;
        case "OpenAPI 3": rv = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True); break;
        default: rv = AsyncApi3Loader::fromString(make_json(AsyncApiDoc), True); break;
    }
    rv.setLogger(HostLogger);
    return rv;
}

# true if the object is part of a schema
bool sub schema_object(object o) {
    string path = Class::getClass(o).getPathName();
    foreach string ns in (SchemaNamespaces) {
        if (path.equalPartial(ns + "::") || path.equalPartial("::" + ns + "::")) {
            return True;
        }
    }
    return False;
}

# the identities and the state of the objects of a schema, without the caches that it builds from itself: objects are
# given by their order in the walk, never formatted with their members
string sub snapshot(object root, reference<list<object>> objs) {
    hash<string, int> ids = {root.uniqueHash(): 0};
    objs = (root,);
    list<string> rv = ();
    for (int i = 0; i < objs.size(); ++i) {
        object o = objs[i];
        list<string> l = ();
        foreach Class c in (Class::getClass(o).getClassHierarchy()) {
            foreach AbstractClassMember m in (c.getMembers()) {
                if (m.isStatic() || !(m instanceof NormalMember) || CacheMembers{m.getName()}) {
                    continue;
                }
                auto v;
                try {
                    v = cast<NormalMember>(m).getValue(o);
                } catch (hash<ExceptionInfo> ex) {
                    continue;
                }
                l += sprintf("%s.%s=%s", c.getName(), m.getName(), describe(v, \ids, \objs));
            }
        }
        rv += sprintf("#%d %s: %s", i, Class::getClass(o).getName(), join(" ", l));
    }
    return join("\n", rv);
}

# a value: an object of a schema is given by its number, another object by its class
string sub describe(auto v, reference<hash<string, int>> ids, reference<list<object>> objs) {
    switch (v.typeCode()) {
        case NT_OBJECT: {
            if (v instanceof Logger::LoggerInterface) {
                return v === HostLogger ? "<host logger>" : "<other logger>";
            }
            if (!schema_object(v)) {
                return sprintf("<%s>", v.className());
            }
            string key = v.uniqueHash();
            if (!exists ids{key}) {
                ids{key} = objs.size();
                objs += v;
            }
            return sprintf("<#%d>", ids{key});
        }
        case NT_HASH:
            return "{" + join(",", map sprintf("%s:%s", $1.key, describe($1.value, \ids, \objs)), v.pairIterator())
                + "}";
        case NT_LIST:
            return "[" + join(",", map describe($1, \ids, \objs), v) + "]";
        case NT_CLOSURE:
        case NT_CALLREF:
            return "<code>";
    }
    return sprintf("%y", v);
}

# an argument of the given type, if one can be given: objects of the schema, plain values, or a closure of the tenant
*list<auto> sub argument(Type t, list<object> objs) {
    if (t.getName() =~ /^\*?code/ || t.isTypedCode()) {
        return ("CODE",);
    }
    if (t.getName() =~ /^reference/) {
        return;
    }
    foreach auto v in (("sweep", 1, True, 1.5, 1n, 2026-01-01, <00>, SweepHash, ("sweep",)) + objs + (NOTHING,)) {
        try {
            return (t.acceptsValue(v),);
        } catch (hash<ExceptionInfo> ex) {
        }
    }
}

# the variants of the methods of a class and of the classes that it inherits
list<AbstractMethodVariant> sub get_variants(Class cls) {
    list<AbstractMethodVariant> rv = ();
    foreach Class c in (cls.getClassHierarchy()) {
        rv += c.getVariants();
    }
    return rv;
}

# the position of an object in a list, or -1
int sub index_of(list<object> l, object o) {
    foreach object e in (l) {
        if (e === o) {
            return $#;
        }
    }
    return -1;
}

int calls = 0;
list<string> changed = ();
hash<string, bool> classes = {};
foreach string kind in (("Swagger", "OpenAPI 3", "AsyncAPI")) {
    list<object> objs;
    snapshot(make_schema(kind), \objs);
    # every public method of each class of the schema's objects once
    hash<string, bool> done = {};
    foreach object o in (objs) {
        Class cls = Class::getClass(o);
        classes{cls.getPathName()} = True;
        foreach AbstractMethodVariant v in (get_variants(cls)) {
            AbstractMethod m = v.getMethod();
            if (m.isStatic() || SkippedMethods{m.getName()} || v.getAccessModifierString() != "public") {
                continue;
            }
            string key = sprintf("%s::%s(%s)", cls.getName(), m.getName(), v.getParamString());
            if (done{key}) {
                continue;
            }
            done{key} = True;
            list<auto> args = ();
            bool ok = True;
            foreach Type t in (v.getParamTypes()) {
                *list<auto> a = argument(t, objs);
                if (!a) {
                    ok = False;
                    break;
                }
                args += a[0];
            }
            if (!ok) {
                continue;
            }
            # a fresh shared schema for each call: its walk has the objects of the first walk at the same positions
            list<object> fresh;
            string before = snapshot(make_schema(kind), \fresh);
            if (fresh.size() != objs.size()) {
                continue;
            }
            int pos = index_of(objs, o);
            args = map $1.typeCode() == NT_OBJECT && index_of(objs, $1) >= 0 ? fresh[index_of(objs, $1)] : $1, args;
            string rv = p.callFunction("call_method", fresh[pos], m.getName(), args);
            ++calls;
            list<object> after_objs;
            string after = snapshot(fresh[0], \after_objs);
            if (after != before) {
                changed += sprintf("%s %s: %s", kind, key, rv);
            }
        }
    }
}
printf("schema classes swept: %y\n", classes.size() >= 20);
printf("public methods called: %y\n", calls >= 400);
printf("calls that changed a shared schema: %y\n", changed);
