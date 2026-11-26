# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A type that sandboxed code froze, which could not be marked as a shared object then, is marked with the types and
# fields that it holds when code that may share objects freezes it again, as when such code registers it in the
# process-wide type cache (qore#7735); see shared-type-refreeze.qtest.  The type is created by code that no sandbox
# governs, which gives it to the tenant before registering it.

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

# freezes a type that it was given
sub freeze_it(DataProvider::HashDataType t) {
    t.freeze();
}

# whether the type, its field and the field's type are shared objects
string sub describe(DataProvider::HashDataType t) {
    DataProvider::AbstractDataField f = t.getFields().items;
    return sprintf(\"type: %y, field: %y, field type: %y\", SandboxManager::isSharedObject(t),
        SandboxManager::isSharedObject(f), SandboxManager::isSharedObject(f.getType()));
}

# tries to delete the field's type, the field and the type
list<string> sub try_deletes(DataProvider::HashDataType t) {
    DataProvider::AbstractDataField f = t.getFields().items;
    DataProvider::AbstractDataProviderType ft = f.getType();
    return (
        \"delete field type: \" + try_it(sub () { delete ft; }),
        \"delete field: \" + try_it(sub () { delete f; }),
        \"delete type: \" + try_it(sub () { delete t; }),
    );
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

# a hash type with a field whose type is a list
HashDataType sub make_type() {
    HashDataType t("refrozen");
    t.addField(new QoreDataField("items", "items", new ListDataType(StringType)));
    return t;
}

# a type that the tenant froze: it cannot be marked as a shared object then
HashDataType t = make_type();
tenant.callFunction("freeze_it", t);
printf("frozen by the tenant: %s\n", tenant.callFunction("describe", t));

# code that no sandbox governs registers it in the process-wide cache, which freezes it again
DataProvider::registerType("test/qore-7735/tenant", t);
printf("registered: %s\n", tenant.callFunction("describe", t));
map printf("  %s\n", $1), tenant.callFunction("try_deletes", cast<HashDataType>(DataProvider::getType(
    "test/qore-7735/tenant")));
try {
    HashDataType rt = cast<HashDataType>(DataProvider::getType("test/qore-7735/tenant"));
    printf("afterwards: %s, field type %y\n", rt.getName(), rt.getFields().items.getType().getName());
} catch (hash<ExceptionInfo> ex) {
    printf("afterwards: %s\n", ex.err);
}

# a type that the tenant froze is marked when code that no sandbox governs freezes it again
t = make_type();
tenant.callFunction("freeze_it", t);
t.freeze();
printf("frozen again by trusted code: %s\n", tenant.callFunction("describe", t));
