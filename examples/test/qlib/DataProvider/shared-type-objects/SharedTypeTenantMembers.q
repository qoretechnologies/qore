# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A type that code no sandbox governs registers in the process-wide type cache of the DataProvider module cannot hold
# a field or a type that sandboxed code created, which could not be marked as a shared object, and which the tenant
# that created it could delete from the cache: the registration is refused, and so is adding such a member to a
# registered type (qore#7740); see shared-type-tenant-members.qtest

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
        c();
        return \"done\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# a field of the tenant's own, with a type of its own
DataProvider::QoreDataField sub make_field(string name) {
    return new DataProvider::QoreDataField(name, name, new DataProvider::HashDataType(name + \"_type\"));
}

# a type of the tenant's own
DataProvider::HashDataType sub make_type(string name) {
    return new DataProvider::HashDataType(name);
}

# a field of a class of the tenant's own, which does not freeze itself
class TenantField inherits DataProvider::QoreDataField {
    constructor(string name) : DataProvider::QoreDataField(name, name, Reflection::StringOrNothingType) {
    }

    freeze() {
    }
}

DataProvider::QoreDataField sub make_subclass_field(string name) {
    return new TenantField(name);
}

# whether a field and its type are shared objects, and the tenant's deletes of them
list<string> sub probe(DataProvider::AbstractDataField f) {
    DataProvider::AbstractDataProviderType ft = f.getType();
    return (
        sprintf(\"field shared: %y, field type shared: %y\", SandboxManager::isSharedObject(f),
            SandboxManager::isSharedObject(ft)),
        \"delete field type: \" + try_it(sub () { delete ft; }),
        \"delete field: \" + try_it(sub () { delete f; }),
    );
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# the result of a call, or its exception
string sub try_it(code c) {
    try {
        c();
        return "done";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# the cache's type at the path, as every caller of the cache gets it
string sub cached(string path) {
    try {
        *AbstractDataProviderType t = DataProvider::getType(path);
        if (!t) {
            return "not registered";
        }
        return sprintf("%s, fields: %y", t.getName(), keys cast<HashDataType>(t).getFields());
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# a type that holds a field that the tenant created is registered
{
    HashDataType t("holds-tenant-field");
    AbstractDataField f = tenant.callFunction("make_field", "tenant_field");
    t.addField(f);
    printf("register a type holding a tenant's field: %s\n", try_it(sub () {
        DataProvider::registerType("test/qore-7740/field", t);
    }));
    if (DataProvider::getType("test/qore-7740/field")) {
        map printf("  %s\n", $1), tenant.callFunction("probe", f);
    }
    printf("  cached: %s\n", cached("test/qore-7740/field"));
}

# a type whose undeclared fields have a type that the tenant created is registered
{
    HashDataType t("holds-tenant-type");
    t.setDefaultOtherFieldType(tenant.callFunction("make_type", "tenant_other"));
    printf("register a type holding a tenant's type: %s\n", try_it(sub () {
        DataProvider::registerType("test/qore-7740/other", t);
    }));
    printf("  cached: %s\n", cached("test/qore-7740/other"));
}

# a field that the tenant created is added to a registered type
{
    HashDataType t("registered");
    t.addField(new QoreDataField("own", "own", StringOrNothingType));
    DataProvider::registerType("test/qore-7740/added", t);
    AbstractDataField f = tenant.callFunction("make_field", "added_field");
    printf("add a tenant's field to a registered type: %s\n", try_it(sub () { t.addField(f); }));
    if (t.getFields().added_field) {
        map printf("  %s\n", $1), tenant.callFunction("probe", f);
    }
    printf("  cached: %s\n", cached("test/qore-7740/added"));
}

# the default type of the undeclared fields of a registered type is set to a type that the tenant created
{
    HashDataType t("registered-other");
    DataProvider::registerType("test/qore-7740/registered-other", t);
    printf("set a tenant's type as the default type of a registered type: %s\n", try_it(sub () {
        t.setDefaultOtherFieldType(tenant.callFunction("make_type", "tenant_default"));
    }));
    printf("  cached default type: %y\n", t.getDefaultOtherFieldType() ? t.getDefaultOtherFieldType().getName() : NOTHING);
}

# registering the type again after a refusal is refused again
{
    HashDataType t("retried");
    t.addField(tenant.callFunction("make_field", "retried_field"));
    printf("register a type holding a tenant's field: %s\n", try_it(sub () {
        DataProvider::registerType("test/qore-7740/retried", t);
    }));
    printf("  again: %s\n", try_it(sub () { DataProvider::registerType("test/qore-7740/retried", t); }));
    printf("  cached: %s\n", cached("test/qore-7740/retried"));
}

# a type that trusted code froze before it registers it
{
    HashDataType t("prefrozen");
    t.addField(tenant.callFunction("make_field", "prefrozen_field"));
    printf("freeze a type holding a tenant's field: %s\n", try_it(\t.freeze()));
    printf("  register it: %s\n", try_it(sub () { DataProvider::registerType("test/qore-7740/prefrozen", t); }));
    printf("  cached: %s\n", cached("test/qore-7740/prefrozen"));
}

# a field of a tenant's class that does not freeze itself
{
    HashDataType t("subclass");
    t.addField(tenant.callFunction("make_subclass_field", "subclass_field"));
    printf("register a type holding a tenant's field that does not freeze itself: %s\n", try_it(sub () {
        DataProvider::registerType("test/qore-7740/subclass", t);
    }));
    printf("  cached: %s\n", cached("test/qore-7740/subclass"));
}

# a union whose alternative the tenant created
{
    QoreUnionDataType u((AbstractDataProviderType::get(StringType),
        cast<AbstractDataProviderType>(tenant.callFunction("make_type", "tenant_alt"))));
    HashDataType t("holds-union");
    t.addField(new QoreDataField("u", "u", u));
    printf("register a type holding a union with a tenant's alternative: %s\n", try_it(sub () {
        DataProvider::registerType("test/qore-7740/union", t);
    }));
    printf("  cached: %s\n", cached("test/qore-7740/union"));
}

# a type that holds only what trusted code created is registered as before
{
    HashDataType t("trusted-only");
    t.addField(new QoreDataField("own", "own", new HashDataType("own_type")));
    printf("register a type of trusted code: %s\n", try_it(sub () {
        DataProvider::registerType("test/qore-7740/trusted", t);
    }));
    printf("  cached: %s\n", cached("test/qore-7740/trusted"));
}
