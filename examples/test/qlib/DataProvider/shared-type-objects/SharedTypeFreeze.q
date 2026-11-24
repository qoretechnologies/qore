# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Freezing a data type that a tenant created marks the types and fields that it holds as shared too: the element type
# of a list, the alternatives of a union and the default type of the undeclared fields of a hash, and the fields of a
# hash and their types; a copy of a frozen type belongs to the code that makes it, but the types that it holds stay
# frozen (qore#6843); see shared-type-freeze.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# a hash type of the tenant's own, with a field
DataProvider::HashDataType sub new_hash(string name) {
    DataProvider::HashDataType rv(name);
    rv.addField(new DataProvider::QoreDataField(name + \"_field\", \"field\", Reflection::StringOrNothingType));
    return rv;
}

# the types of the tenant's own: each holds hash types that hold a field
hash<auto> sub make_types() {
    DataProvider::HashDataType with_other = new_hash(\"outer\");
    with_other.setDefaultOtherFieldType(new_hash(\"other\"));
    return {
        \"list\": new DataProvider::ListDataType(new_hash(\"element\")),
        \"union\": new DataProvider::QoreUnionDataType((new_hash(\"alt1\"), new_hash(\"alt2\"))),
        \"hash\": with_other,
        \"field\": new DataProvider::QoreDataField(\"held\", \"held\", new_hash(\"field_type\")),
    };
}

# the hash types that the given types hold
hash<string, DataProvider::HashDataType> sub inner_types(hash<auto> t) {
    return {
        \"list element type\": t.list.getElementType(),
        \"union alternative 1\": t.union.getUnionTypes()[0],
        \"union alternative 2\": t.union.getUnionTypes()[1],
        \"hash default other field type\": t.hash.getDefaultOtherFieldType(),
        \"type of the field\": t.field.getType(),
    };
}

# tries to change the types, the hash types that they hold and the fields of those
string sub try_changes(hash<auto> t) {
    list<string> l = (
        sprintf(\"list: %s\", try_change(sub () { t.list.setPath(\"tenant\"); })),
        sprintf(\"union: %s\", try_change(sub () { t.union.setPath(\"tenant\"); })),
        sprintf(\"hash: %s\", try_change(sub () { t.hash.setPath(\"tenant\"); })),
        sprintf(\"field: %s\", try_change(sub () { t.field.setAttributes({\"tenant\": True}); })),
        sprintf(\"field of the hash: %s\", try_change(sub () {
            t.hash.getFields().firstValue().setAttributes({\"tenant\": True});
        })),
    );
    foreach hash<auto> i in (inner_types(t).pairIterator()) {
        l += sprintf(\"%s: %s\", i.key, try_change(sub () { i.value.setPath(\"tenant\"); }));
        l += sprintf(\"field of the %s: %s\", i.key, try_change(sub () {
            i.value.getFields().firstValue().setAttributes({\"tenant\": True});
        }));
    }
    return join(\"\\n\", l);
}

# tries to change copies of the types, and the types that the copies hold
string sub try_copy_changes(hash<auto> t) {
    DataProvider::ListDataType list_copy = t.list.copy();
    DataProvider::QoreUnionDataType union_copy = t.union.copy();
    DataProvider::HashDataType hash_copy = t.hash.copy();
    list<string> l = (
        sprintf(\"copy of the list: %s\", try_change(sub () { list_copy.setPath(\"tenant\"); })),
        sprintf(\"element type of the copy of the list: %s\", try_change(sub () {
            list_copy.getElementType().setPath(\"tenant\");
        })),
        sprintf(\"copy of the union: %s\", try_change(sub () { union_copy.setPath(\"tenant\"); })),
        sprintf(\"alternative of the copy of the union: %s\", try_change(sub () {
            union_copy.getUnionTypes()[0].setPath(\"tenant\");
        })),
        sprintf(\"copy of the hash: %s\", try_change(sub () {
            hash_copy.addField(new DataProvider::QoreDataField(\"added\", \"added\",
                Reflection::StringOrNothingType));
        })),
        sprintf(\"default other field type of the copy of the hash: %s\", try_change(sub () {
            hash_copy.getDefaultOtherFieldType().setPath(\"tenant\");
        })),
    );
    return join(\"\\n\", l);
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# types that the tenant created can be changed by it
hash<auto> before = p.callFunction("make_types");
printf("before freezing:\n%s\n", p.callFunction("try_changes", before));

# library code keeps the tenant's types for every caller and freezes them
hash<auto> types = p.callFunction("make_types");
map $1.freeze(), types.iterator();
printf("after freezing:\n%s\n", p.callFunction("try_changes", types));
printf("copies of the frozen types:\n%s\n", p.callFunction("try_copy_changes", types));

# the frozen types are unchanged
printf("frozen paths: %y\n", (map $1.getPath(), p.callFunction("inner_types", types).iterator()));
printf("frozen field attributes: %y\n", types.field.getAttributes());
printf("frozen hash fields: %y\n", keys types.hash.getFields());
