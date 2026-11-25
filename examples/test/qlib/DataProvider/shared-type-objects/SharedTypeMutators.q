# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# Each method that changes a data type or field refuses sandboxed code for a type or field that every Program of the
# process shares, and does not refuse it for one of its own (qore#6843); see shared-type-mutators.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider
%requires OpenApi3

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider
%requires OpenApi3

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the changes, each with a type or field of the given objects
hash<string, code> sub changes() {
    return {
        \"QoreDataField::appendDescription()\": sub (hash<auto> o) {
            o.field.appendDescription(\" \", \"more\");
        },
        \"HashDataType::checkConvertOrNothing()\": sub (hash<auto> o) {
            o.optional_hash.checkConvertOrNothing();
        },
        \"OpenApi3UnionDataType::setResponseDirection()\": sub (hash<auto> o) {
            o.union.setResponseDirection();
        },
    };
}

# the results of the changes of the given objects
string sub try_changes(hash<auto> o) {
    list<string> l = ();
    foreach hash<auto> i in (changes().pairIterator()) {
        l += sprintf(\"%s: %s\", i.key, try_change(sub () { i.value(o); }));
    }
    return join(\"\\n\", l);
}

# objects of the tenant's own
hash<auto> sub own() {
    DataProvider::HashDataType optional_hash(\"own\");
    optional_hash.addField(new DataProvider::QoreDataField(\"o\", \"o\", Reflection::StringOrNothingType));
    return {
        \"field\": new DataProvider::QoreDataField(\"own\", \"own\", Reflection::StringOrNothingType),
        \"optional_hash\": optional_hash,
        \"union\": new OpenApi3::OpenApi3UnionDataType((DataProvider::AbstractDataProviderType::get(
            Reflection::StringType), DataProvider::AbstractDataProviderType::get(Reflection::IntType)), \"anyOf\"),
    };
}

string sub try_own_changes() {
    return try_changes(own());
}

# a call of HashDataType::checkConvertOrNothing() that changes nothing
string sub check_mandatory(DataProvider::HashDataType t) {
    return try_change(sub () { t.checkConvertOrNothing(); });
}
";

Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
p.setSandboxManager(new SandboxManager());
p.parse(TenantCode, "tenant");

# objects that code no sandbox governs creates, which every Program of the process can share
HashDataType optional_hash("shared");
optional_hash.addField(new QoreDataField("o", "o", StringOrNothingType));
HashDataType mandatory_hash("mandatory");
mandatory_hash.addField(new QoreDataField("m", "m", StringType));
hash<auto> shared = {
    "field": new QoreDataField("shared", "shared", StringOrNothingType),
    "optional_hash": optional_hash,
    "union": new OpenApi3UnionDataType((AbstractDataProviderType::get(StringType),
        AbstractDataProviderType::get(IntType)), "anyOf"),
};

printf("shared objects:\n%s\n", p.callFunction("try_changes", shared));
printf("the tenant's own objects:\n%s\n", p.callFunction("try_own_changes"));
printf("a call that changes nothing: %s\n", p.callFunction("check_mandatory", mandatory_hash));
printf("the shared field's description: %y\n", shared.field.getDescription());
printf("the shared hash type is \"or nothing\": %y\n", optional_hash.isOrNothingType());
