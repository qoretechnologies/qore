# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A type build of a shared REST schema that fails leaves every cache that it wrote exactly as it was: the types, the
# canonical and per-use fields, the replaced types and the alternative types of the schema objects and the type caches
# of OpenAPI 3 and Swagger documents, with the entries that were absent removed and the values that it replaced
# restored (the same objects, which the records of the build keep alive); a nested build that fails undoes only its own
# writes; the records of the schema's caches and the code that a subclass records are undone in the reverse order of
# the writes; the records cannot be read by sandboxed code (qore#6843); see shared-schema-undo-kinds.qtest

%modern
%enable-all-warnings
%no-child-restrictions
%allow-weak-references

%requires reflection
%requires OpenApi3
%requires Swagger
%requires json

const OpenApi3Doc = {
    "openapi": "3.0.0",
    "info": {"title": "test", "version": "1.0"},
    "servers": ({"url": "http://localhost/api"},),
    "paths": {},
    "components": {
        "schemas": {
            "Thing": {"type": "object", "required": ("name",), "properties": {
                "name": {"type": "string"},
                "tags": {"type": "array", "items": {"type": "string"}},
                "kind": {"oneOf": ({"type": "string"}, {"type": "integer"})},
            }},
            "Other": {"type": "object", "properties": {"x": {"type": "string"}}},
            "Code": {"type": "string"},
        },
    },
};

const SwaggerDoc = {
    "swagger": "2.0",
    "info": {"title": "test", "version": "1.0"},
    "basePath": "/api",
    "paths": {},
    "definitions": {
        "Thing": {"type": "object", "properties": {"tags": {"type": "array", "items": {"type": "integer"}}}},
    },
};

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires reflection

string sub read_records(object schema) {
    try {
        foreach Reflection::Class c in (Reflection::Class::getClass(schema).getClassHierarchy()) {
            foreach Reflection::AbstractClassMember m in (c.getMembers()) {
                if (m.getName() == \"type_build_undo\") {
                    cast<Reflection::NormalMember>(m).getValue(schema);
                    return \"read\";
                }
            }
        }
        return \"not found\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

string sub call_restore(object so) {
    try {
        call_object_method(so, \"getCacheUndoRestore\");
        return \"called\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}
";

# the value of a member of an object, read with Reflection by trusted test code
auto sub member(object o, string name) {
    foreach Class c in (Class::getClass(o).getClassHierarchy()) {
        foreach AbstractClassMember m in (c.getMembers()) {
            if (m.getName() == name && m instanceof NormalMember) {
                return cast<NormalMember>(m).getValue(o);
            }
        }
    }
}

# the caches of a schema object
hash<auto> sub so_caches(object so) {
    hash<auto> rv = {
        "dataType": so.dataType,
        "orNothingDataType": so.orNothingDataType,
        "replacedDataType": member(so, "replacedDataType"),
        "fieldmap": member(so, "fieldmap"),
        "usemap": member(so, "usemap"),
    };
    if (so instanceof OpenApi3::SchemaObject) {
        rv.alternativeDataType = member(so, "alternativeDataType");
    }
    return rv;
}

# the caches of a document and its schema objects
hash<auto> sub caches(object doc, hash<string, object> sos) {
    return {"typemap": member(doc, "typemap")} + map {$1.key: so_caches($1.value)}, sos.pairIterator();
}

# True if two snapshots are the same: the same keys and the same objects (by identity)
bool sub same(auto a, auto b) {
    if (a.typeCode() != b.typeCode()) {
        return False;
    }
    switch (a.typeCode()) {
        case NT_HASH:
            if (keys a != keys b) {
                return False;
            }
            foreach string k in (keys a) {
                if (!same(a{k}, b{k})) {
                    return False;
                }
            }
            return True;
        case NT_OBJECT:
            return a == b;
        default:
            return a === b;
    }
}

# runs a build that makes its writes and then fails
string sub fail_build(object doc, code writes) {
    try {
        doc.runTypeBuild(sub () {
            writes();
            throw "BUILD-ERROR", "the build fails";
        });
        return "built";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

OpenApi3Schema doc = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
hash<string, object> sos = doc.components.schemas + {"Thing.name": doc.components.schemas.Thing.properties.name};
SchemaObject thing = doc.components.schemas.Thing;
SchemaObject other = doc.components.schemas.Other;
SchemaObject code = doc.components.schemas.Code;

# writes before the failing builds, so that they replace some entries and add others
doc.getTypeForReference("Thing");
thing.properties.name.getField("name", "a use", True);

# each kind of cache write in a build that fails
hash<string, code> kinds = {
    "the types of a schema object": sub () {
        doc.getTypeForReference("Other");
        doc.getTypeForReference("Other", False);
    },
    "the canonical and per-use fields": sub () {
        other.properties.x.getField("x");
        thing.properties.name.getField("name", "another use", False);
    },
    "the type cache of the document": sub () {
        doc.getType("/undo/x", "integer");
        doc.getType("/undo/y", "string", NOTHING, NOTHING, False);
    },
    "an alternative type": sub () {
        code.getAlternativeDataType();
    },
};
foreach hash<auto> i in (kinds.pairIterator()) {
    hash<auto> before = caches(doc, sos);
    string rv = fail_build(doc, i.value);
    printf("%s: %s, the caches are as before: %y\n", i.key, rv, same(before, caches(doc, sos)));
}

# a replaced type: the schema releases the caches that its objects hold through opaque references when it is given a
# type that it did not build, so they are released first, and the types are built again
doc.stopOpaqueTypeCaches();
doc.getTypeForReference("Thing");
doc.getTypeForReference("Other");
{
    hash<auto> before = caches(doc, sos);
    string rv = fail_build(doc, sub () {
        thing.replaceType(AbstractDataProviderType::get("hash"));
    });
    printf("a replaced type: %s, the caches are as before: %y\n", rv, same(before, caches(doc, sos)));
}

# the type cache of a Swagger document
{
    SwaggerSchema sdoc = SwaggerLoader::fromString(make_json(SwaggerDoc), True);
    sdoc.getTypeForReference("Thing");
    hash<auto> before = member(sdoc, "typemap");
    string rv = fail_build(sdoc, sub () {
        sdoc.getType("/undo/z", "integer");
        sdoc.definitions.Thing.getDataType(False);
    });
    printf("the type cache of a Swagger document: %s, as before: %y\n", rv, same(before, member(sdoc, "typemap")));
}

# a nested build that fails undoes only its own writes
{
    hash<auto> before = caches(doc, sos);
    doc.runTypeBuild(sub () {
        doc.getType("/nested/outer", "integer");
        try {
            doc.runTypeBuild(sub () {
                doc.getType("/nested/inner", "integer");
                throw "BUILD-ERROR", "the nested build fails";
            });
        } catch (hash<ExceptionInfo> ex) {
        }
    });
    hash<auto> typemap = member(doc, "typemap");
    printf("a nested build that fails: the outer write is kept: %y, the inner write is undone: %y, the rest is as "
        "before: %y\n", exists typemap{"/nested/outer-integer-x-x"}, !exists typemap{"/nested/inner-integer-x-x"},
        same(before - "typemap", caches(doc, sos) - "typemap"));
}

# the records of the schema's caches and the code that a subclass records, undone in the reverse order of the writes
{
    list<string> events = ();
    string rv = fail_build(doc, sub () {
        doc.recordTypeBuildWrite(sub () {
            events += sprintf("first code record: the field is cached: %y", exists member(other.properties.x,
                "fieldmap")."y");
        });
        other.properties.x.getField("y");
        doc.recordTypeBuildWrite(sub () {
            events += sprintf("last code record: the field is cached: %y", exists member(other.properties.x,
                "fieldmap")."y");
        });
    });
    printf("a build with records and code: %s, undone: %y\n", rv, events);
}

# the value that a write replaced is restored, the same object, also when the build frees other values meanwhile
{
    SchemaObject so = doc.components.schemas.Other;
    *object prev_ref;
    # the type cached by an earlier build, which only the cache holds
    prev_ref := so.dataType;
    string name = prev_ref.getName();
    list<string> fields = keys prev_ref.getFields();
    string rv = fail_build(doc, sub () {
        so.replaceType(AbstractDataProviderType::get("hash"));
        # the cache no longer holds the type: only the record of the build does
        for (int i = 0; i < 10000; ++i) {
            hash<auto> h = {"i": i, "s": strmul("x", i % 64)};
            delete h;
        }
    });
    printf("a replaced type: %s, restored: %y, the same object: %y, intact: %y\n", rv, exists so.dataType,
        so.dataType == prev_ref, so.dataType.getName() == name && keys so.dataType.getFields() == fields);
}

# the records of a build are not readable by sandboxed code, nor the code that restores them callable
{
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    p.setSandboxManager(new SandboxManager());
    p.parse(TenantCode, "tenant");
    printf("a tenant reads the records with Reflection: %s; calls the method that gives the restore code: %s\n",
        p.callFunction("read_records", doc), p.callFunction("call_restore", thing));
}
