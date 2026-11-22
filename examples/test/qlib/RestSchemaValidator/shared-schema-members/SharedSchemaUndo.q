# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A tenant that obtains the undo records of a type build of a shared schema and runs them later changes nothing: an
# undo runs once only, in the build that recorded it (qore#6843); see shared-schema-undo.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires reflection
%requires OpenApi3
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
        },
    },
};

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern

# runs each undo record that it is given
string sub run_undos(list<code> undos) {
    list<string> l = ();
    foreach code undo in (undos) {
        try {
            undo();
            l += \"ran\";
        } catch (hash<ExceptionInfo> ex) {
            l += ex.err;
        }
    }
    return join(\" \", (map $1, l, $# < 1) + (l.size() > 1 ? (sprintf(\"(%d records)\", l.size()),) : ()));
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# the state of the caches of the schema's objects: the types and fields that they hold, by identity
string sub snapshot(OpenApi3Schema schema) {
    list<string> l = ();
    foreach hash<auto> i in (schema.components.schemas.pairIterator()) {
        SchemaObject so = i.value;
        l += sprintf("%s: %y %y", i.key, so.dataType ? so.dataType.uniqueHash() : NOTHING,
            so.orNothingDataType ? so.orNothingDataType.uniqueHash() : NOTHING);
        foreach hash<auto> p in (so.properties.pairIterator()) {
            l += sprintf("%s.%s: %y", i.key, p.key, p.value.dataType ? p.value.dataType.uniqueHash() : NOTHING);
        }
    }
    return join("\n", l);
}

# the undo records of the type build in progress, read with Reflection by trusted test code
list<code> sub undo_records(OpenApi3Schema schema) {
    foreach Class c in (Class::getClass(schema).getClassHierarchy()) {
        foreach AbstractClassMember m in (c.getMembers()) {
            if (m.getName() == "type_build_undo" && m instanceof NormalMember) {
                return cast<NormalMember>(m).getValue(schema) ?? ();
            }
        }
    }
    return ();
}

OpenApi3Schema schema = OpenApi3Loader::fromString(make_json(OpenApi3Doc), True);
list<code> undos;
# trusted code builds the types of the shared schema; the undo records of the build are taken before it ends
schema.runTypeBuild(sub () {
    schema.getTypeForReference("Thing");
    undos = undo_records(schema);
});
printf("undo records taken: %y\n", undos.size() > 0);
string before = snapshot(schema);
printf("the tenant runs them after the build: %s\n", p.callFunction("run_undos", undos));
printf("the caches are unchanged: %y\n", snapshot(schema) == before);
# trusted code runs them in a later build
schema.runTypeBuild(sub () {
    map $1(), undos;
});
printf("trusted code runs them in a later build, the caches are unchanged: %y\n", snapshot(schema) == before);
