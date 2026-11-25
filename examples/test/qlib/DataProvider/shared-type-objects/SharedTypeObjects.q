# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# A data type, field or REST schema that code no sandbox governs created, which every Program of the process can
# share, cannot be changed by sandboxed code; a type that sandboxed code created can be (qore#6843); see
# shared-type-objects.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires DataProvider
%requires StripeDataProvider
%requires GoogleDataProvider

# the code of a tenant: a sandboxed Program
const TenantCode = "
%modern
%requires DataProvider
%requires StripeDataProvider
%requires GoogleDataProvider

# the result of a change, or its exception
string sub try_change(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# changes a type and a field that the tenant did not create
string sub change_shared(HashDataType t, AbstractDataField f) {
    list<string> l = ();
    l += try_change(sub () { t.addField(new QoreDataField(\"injected\", \"injected\", StringOrNothingType)); });
    l += try_change(sub () { t.setPath(\"x\"); });
    l += try_change(sub () { t.setOptions({}); });
    l += try_change(sub () { f.setDefaultValue(\"injected\"); });
    l += try_change(sub () { f.setAllowedValues((\"x\",)); });
    return join(\" \", l);
}

# creates and changes a type of the tenant's own
string sub change_own() {
    HashDataType t();
    QoreDataField f(\"f\", \"f\", StringOrNothingType);
    list<string> l = ();
    l += try_change(sub () { t.addField(f); });
    l += try_change(sub () { t.setPath(\"x\"); });
    l += try_change(sub () { f.setDefaultValue(\"own\"); });
    return join(\" \", l);
}

# builds the Stripe action set, if no one has built it yet, and returns it
object sub get_action_set() {
    return StripeSchema::getActionSet();
}

# changes a request field of a Stripe action and a type of the Stripe schema
string sub change_stripe() {
    object set = StripeSchema::getActionSet();
    string action = set.getActionNames()[0];
    AbstractDataField f = set.getOperationEx(action).getRequestFields().firstValue();
    list<string> l = ();
    l += try_change(sub () { f.setDefaultValue(\"injected\"); });
    l += try_change(sub () {
        object schema = StripeSchema::getSchema();
        schema.replaceTypeForReference(\"customer\", AbstractDataProviderType::anyDataType);
    });
    return join(\" \", l);
}

# gets a type of the Google Calendar API, whose types the module caches for every caller
object sub get_google_type() {
    return GoogleDataProviderBase::getTypeForSchema(\"calendar\", \"Event\");
}

# changes a cached Google type and its field (qore#6863)
string sub change_google() {
    HashDataType t = cast<HashDataType>(GoogleDataProviderBase::getTypeForSchema(\"calendar\", \"Event\"));
    AbstractDataField f = t.getFields().firstValue();
    list<string> l = ();
    l += try_change(sub () { t.addField(new QoreDataField(\"injected\", \"injected\", StringOrNothingType)); });
    l += try_change(sub () { f.setAttributes({}); });
    return join(\" \", l);
}

# tries to register a lazy value outside a module load
string sub register_lazy() {
    return try_change(sub () { DataProviderActionCatalog::registerLazyValue(int sub () { return 1; }); });
}
";

# a tenant
Program sub tenant(string name) {
    Program p(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
    p.setSandboxManager(new SandboxManager());
    p.parse(TenantCode, name);
    return p;
}

Program a = tenant("tenant-a");
Program b = tenant("tenant-b");

# a type and a field that code no sandbox governs creates
HashDataType shared_type();
QoreDataField shared_field("f", "f", StringOrNothingType);
shared_type.addField(shared_field);

printf("shared type and field: %s\n", a.callFunction("change_shared", shared_type, shared_field));
printf("the tenant's own type and field: %s\n", a.callFunction("change_own"));
printf("outside a sandbox: %s\n", (string sub () {
    try {
        shared_type.addField(new QoreDataField("g", "g", StringOrNothingType));
        shared_field.setDefaultValue("outside");
        return "changed";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
# the Stripe action set, which the module builds on first use, is used first by tenant A, then tenant B changes it
printf("a module's lazily built schema, first used by a tenant: %s\n", (string sub () {
    try {
        a.callFunction("get_action_set");
        return "built";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
})());
StripeSchema::getActionSet();
printf("a module's lazily built schema: %s\n", b.callFunction("change_stripe"));
# the Google types are built in tenant A's call, then tenant B changes them
a.callFunction("get_google_type");
printf("a module's cached types built in a tenant's call: %s\n", b.callFunction("change_google"));
printf("a tenant registers a lazy value: %s\n", b.callFunction("register_lazy"));
