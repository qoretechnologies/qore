# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A type that code no sandbox governs registers in the process-wide type cache of the DataProvider module, and the
# fields that it holds, are shared objects: a tenant that gets the type from the cache cannot delete it or its fields,
# which every caller of the cache uses, also the fields and types that the code that registered it adds to it later;
# it can still read the type, also where reading records something in the type, and copy it (qore#7728); see
# shared-type-cache.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider

# the result of a call, or its exception
string sub try_it(code c) {
    try {
        auto v = c();
        return exists v ? sprintf(\"%y\", v) : \"done\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# gets the type from the cache and tries to delete it and the first of its fields; it is copied first
list<string> sub probe(string path) {
    DataProvider::HashDataType t = cast<DataProvider::HashDataType>(DataProvider::DataProvider::getType(path));
    DataProvider::AbstractDataField f = t.getFields().firstValue();
    list<string> l = ();
    l += sprintf(\"type shared: %y\", SandboxManager::isSharedObject(t));
    l += sprintf(\"field shared: %y\", SandboxManager::isSharedObject(f));
    # the default value of a hash type is checked when it is first read, here by the tenant
    l += \"info: \" + try_it(auto sub () { return t.getInfo().default_value; });
    l += \"copy: \" + try_it(auto sub () {
        DataProvider::HashDataType c = t.copy();
        c.addField(new DataProvider::QoreDataField(\"tenant\", \"tenant\", Reflection::StringOrNothingType));
        return sprintf(\"shared: %y, fields: %d\", SandboxManager::isSharedObject(c), c.getFields().size());
    });
    l += \"delete field: \" + try_it(sub () { delete f; });
    l += \"delete type: \" + try_it(sub () { delete t; });
    return l;
}

# tries to delete a field of the type and the type of a hash type held by the type, which trusted code added to it
list<string> sub probe_added(string path, string field, string other_field) {
    DataProvider::HashDataType t = cast<DataProvider::HashDataType>(DataProvider::DataProvider::getType(path));
    DataProvider::AbstractDataField f = t.getFields(){field};
    DataProvider::AbstractDataProviderType ft = t.getFields(){other_field}.getType().getDefaultOtherFieldType();
    list<string> l = ();
    l += sprintf(\"added field shared: %y\", SandboxManager::isSharedObject(f));
    l += sprintf(\"added default other field type shared: %y\", SandboxManager::isSharedObject(ft));
    l += \"delete added field: \" + try_it(sub () { delete f; });
    l += \"delete added default other field type: \" + try_it(sub () { delete ft; });
    return l;
}

# tries to delete the fields and the union alternatives that trusted code set on registered types
list<string> sub probe_set(string hash_path, string union_path) {
    DataProvider::HashDataType t = cast<DataProvider::HashDataType>(DataProvider::DataProvider::getType(hash_path));
    DataProvider::AbstractDataField f = t.getFields().firstValue();
    DataProvider::QoreUnionDataType u = cast<DataProvider::QoreUnionDataType>(
        DataProvider::DataProvider::getType(union_path));
    DataProvider::AbstractDataProviderType alt = u.getUnionTypes()[0];
    list<string> l = ();
    l += sprintf(\"set field shared: %y\", SandboxManager::isSharedObject(f));
    l += sprintf(\"set union alternative shared: %y\", SandboxManager::isSharedObject(alt));
    l += \"delete set field: \" + try_it(sub () { delete f; });
    l += \"delete set union alternative: \" + try_it(sub () { delete alt; });
    return l;
}
";

# a type with a field that code no sandbox governs registers in the cache at run time
DataProvider::HashDataType runtime_type("runtime");
runtime_type.addField(new QoreDataField("value", "value", StringOrNothingType));
# a field with a default value: the default value of the type is checked when it is first read
runtime_type.addField(new QoreDataField("count", "count", IntType, 1));
DataProvider::registerType("test/qore-7728/runtime", runtime_type);

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# the event type of the FtpPollerUtil module, which the tenant's lookup loads, registers it when it is initialized;
# and the type registered at run time
foreach string path in (("qore/ftp/event", "test/qore-7728/runtime")) {
    printf("%s:\n", path);
    map printf("  %s\n", $1), tenant.callFunction("probe", path);
    # the type and its fields are still usable by every caller of the cache
    try {
        HashDataType t = cast<HashDataType>(DataProvider::getType(path));
        AbstractDataField f = t.getFields().firstValue();
        printf("  afterwards: %s, field %y of type %y\n", t.className(), f.getName(), f.getType().getName());
    } catch (hash<ExceptionInfo> ex) {
        printf("  afterwards: %s\n", ex.err);
    }
}

# code that no sandbox governs can still change a registered type; what it adds is shared as well
try {
    runtime_type.addField(new QoreDataField("trusted", "trusted", StringOrNothingType));
    HashDataType held("held");
    runtime_type.addField(new QoreDataField("held", "held", held));
    held.setDefaultOtherFieldType(new HashDataType("other"));
    printf("trusted change: fields %y\n", keys runtime_type.getFields());
} catch (hash<ExceptionInfo> ex) {
    printf("trusted change: %s\n", ex.err);
}
map printf("  %s\n", $1), tenant.callFunction("probe_added", "test/qore-7728/runtime", "trusted", "held");

# the fields and union alternatives that code no sandbox governs replaces on registered types are shared as well
{
    HashDataType set_type("set");
    DataProvider::registerType("test/qore-7728/set", set_type);
    set_type.setFields({"replaced": new QoreDataField("replaced", "replaced", StringOrNothingType)});
    QoreUnionDataType union_type((AbstractDataProviderType::get(StringType), AbstractDataProviderType::get(IntType)));
    DataProvider::registerType("test/qore-7728/union", union_type);
    union_type.setUnionTypes((new HashDataType("alt1"), new HashDataType("alt2")));
    printf("trusted set fields and union alternatives\n");
    map printf("  %s\n", $1), tenant.callFunction("probe_set", "test/qore-7728/set", "test/qore-7728/union");
}
