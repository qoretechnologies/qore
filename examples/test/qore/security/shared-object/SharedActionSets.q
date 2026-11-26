# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The process-wide REST schema action sets that data provider modules build from their vendored schemas, and their
# operations and types, are shared objects: a tenant (a sandboxed Program) that gets one cannot change or delete it,
# whether the module builds it in a lazy shared value or in a lazy value of the data provider action catalog, and it
# still uses it (qore#7714); see shared-action-sets.qtest

%modern
%enable-all-warnings
%no-child-restrictions

%requires TogetherDataProvider
%requires ExaDataProvider

# the code of a tenant: a sandboxed Program, which is the first to need the modules' action sets
const TenantCode = "
%modern
%requires TogetherDataProvider
%requires ExaDataProvider

# the result of a change, or its exception
string sub try_it(code c) {
    try {
        c();
        return \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

# what the tenant can do with an action set that a module keeps for every caller
string sub check_set(code get_set) {
    object set = get_set();
    list<hash<auto>> actions = set.getActionInfoList(\"Test\");
    # the first action that has options
    string action = (select actions, set.getOperation($1.action).getRequestType())[0].action;
    object op = set.getOperation(action);
    object request_type = op.getRequestType();
    object copy = set.copy();
    return sprintf(\"set shared: %y, operation shared: %y, request type shared: %y, actions: %y; \"
        \"the tenant changes the request type: %s; copies the set: shared: %y, its operation shared: %y; \"
        \"deletes the operation: %s; deletes the set: %s\",
        SandboxManager::isSharedObject(set), SandboxManager::isSharedObject(op),
        SandboxManager::isSharedObject(request_type), actions.size() > 0,
        try_it(sub () { request_type.addField(new QoreDataField(\"tenant\", NOTHING, StringType)); }),
        SandboxManager::isSharedObject(copy), SandboxManager::isSharedObject(copy.getOperation(action)),
        try_it(sub () { delete op; }), try_it(sub () { delete set; }));
}

string sub together() {
    return check_set(object sub () { return TogetherDataProvider::TogetherSchema::getActionSet(); });
}

string sub exa() {
    return check_set(object sub () { return ExaDataProvider::ExaSchema::getActionSet(); });
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
tenant.setSandboxManager(new SandboxManager());
tenant.loadModule("DataProvider");
tenant.parse(TenantCode, "tenant");

printf("a lazy shared value (TogetherDataProvider): %s\n", tenant.callFunction("together"));
printf("a lazy value of the action catalog (ExaDataProvider): %s\n", tenant.callFunction("exa"));
# the sets still work for every caller
printf("trusted code uses the sets: %y, %y\n",
    TogetherDataProvider::TogetherSchema::getActionSet().getActionInfoList("Test").size() > 0,
    ExaDataProvider::ExaSchema::getActionSet().getActionInfoList("Test").size() > 0);
