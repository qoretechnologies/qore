# -*- mode: qore; indent-tabs-mode: nil -*-
# Copyright (C) 2026 Qore Technologies, s.r.o.
# The data provider modules that load their vendored API schemas on first use build them as trusted code whatever
# code needs them first: a sandboxed first caller gets schemas and action sets marked as shared, which it cannot
# change (qore#7684); see lazy-shared-values.qtest

%modern
%enable-all-warnings
%no-child-restrictions

# loaded before the provider modules, so that the AWS and Azure clients find no credentials of the host (the test
# makes no call to a service)
%requires ../../../LiveProviderTest.qm
%requires AssemblyAiDataProvider
%requires AzureOpenAiDataProvider
%requires BedrockDataProvider
%requires FireworksDataProvider
%requires TogetherDataProvider

# the code of a tenant: a sandboxed Program; each module's schemas are first needed here
const TenantCode = "
%modern
%requires AssemblyAiDataProvider
%requires AzureOpenAiDataProvider
%requires BedrockDataProvider
%requires FireworksDataProvider
%requires TogetherDataProvider

# the shared state of a schema and of an action set that the tenant gets first
string sub describe(object schema, object set) {
    string change;
    try {
        schema.info = {};
        change = \"changed\";
    } catch (hash<ExceptionInfo> ex) {
        change = ex.err;
    }
    return sprintf(\"schema shared: %y, action set shared: %y, the tenant changes the schema: %s\",
        SandboxManager::isSharedObject(schema), SandboxManager::isSharedObject(set), change);
}

# the state of the schema and action set that a call gives, or the exception
string sub try_describe(code get) {
    try {
        list<object> l = get();
        return describe(l[0], l[1]);
    } catch (hash<ExceptionInfo> ex) {
        return ex.err;
    }
}

hash<string, string> sub first_use() {
    return {
        \"AssemblyAi\": try_describe(list<object> sub () {
            RestSchemaActions::RestSchemaActionSet set
                = AssemblyAiDataProvider::AssemblyAiSchema::getActionSets().firstValue();
            return (set.getSchema(), set);
        }),
        \"AzureOpenAi\": try_describe(list<object> sub () {
            return (AzureOpenAiDataProvider::AzureOpenAiSchema::getSchema(),
                AzureOpenAiDataProvider::AzureOpenAiSchema::getActionSet());
        }),
        \"Bedrock\": try_describe(list<object> sub () {
            return (BedrockDataProvider::BedrockSchema::getControlSchema(),
                BedrockDataProvider::BedrockSchema::getActionSets()[0]);
        }),
        \"Fireworks\": try_describe(list<object> sub () {
            RestSchemaActions::RestSchemaActionSet set = FireworksDataProvider::FireworksSchema::getActionSets()[0];
            return (set.getSchema(), set);
        }),
        \"Together\": try_describe(list<object> sub () {
            return (TogetherDataProvider::TogetherSchema::getSchema(),
                TogetherDataProvider::TogetherSchema::getActionSet());
        }),
    };
}
";

Program tenant(PO_NEW_STYLE | PO_REQUIRE_TYPES | PO_STRICT_ARGS | PO_STRONG_ENCAPSULATION);
tenant.setSandboxManager(new SandboxManager());
tenant.parse(TenantCode, "tenant");

foreach hash<auto> i in (tenant.callFunction("first_use").pairIterator()) {
    printf("%s: %s\n", i.key, i.value);
}
