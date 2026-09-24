#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o.
# Run with QORE_MODULE_DIR_ONLY=1 and an installed-only module path; pass the stage prefix.
%modern
%requires AssemblyAiDataProvider
%requires ProviderIndex
%requires FsUtil

const Actions = ("transcribe", "make-api-call", "upload-audio", "create-transcript", "list-transcripts",
    "get-transcript", "delete-transcript", "get-subtitles", "get-sentences", "get-paragraphs",
    "search-transcript-words", "get-redacted-audio", "create-chat-completion", "understand-speech", "list-models",
    "create-streaming-token", "create-voice-agent-token", "create-agent", "list-agents", "get-agent",
    "update-agent", "delete-agent", "list-sessions", "get-session", "delete-session", "create-webhook",
    "list-webhooks", "get-webhook", "update-webhook", "delete-webhook", "transcribe-short-audio",
    "transcribe-live-upload", "transcribe-dictation");

if (!ARGV[0] || ENV.QORE_MODULE_DIR_ONLY != "1") {
    throw "INSTALL-QUALIFICATION-ERROR", "pass an empty-prefix install stage and exclude default module paths";
}
string stage = ARGV[0];
foreach string module_name in ("AssemblyAiDataProvider", "AssemblyAiRestClient", "OpenApi3", "RestSchemaActions", "logger_bin") {
    string loaded = get_module_hash(){module_name}.filename;
    if (index(loaded, stage + "/") != 0 || loaded !~ /\.qmod$/) {
        throw "INSTALL-QUALIFICATION-ERROR", sprintf("%s did not load its installed AOT artifact: %s", module_name, loaded);
    }
}
const App = AssemblyAiDataProvider::AppName;
list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActionsEx(App);
if (sort(map $1.action, actions) != sort(Actions)
        || DataProviderActionCatalog::getInitializationFailures((App,)).size()) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed action inventory or initialization failures mismatch";
}
hash<string, RestSchemaActionSet> sets = AssemblyAiSchema::getActionSets();
if (sets.size() != 8 || (foldl $1 + $2, (map $1.getActionNames().size(), sets.iterator())) != 31) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed schema resources are incomplete";
}
foreach RestSchemaActionSet set in (sets.iterator()) {
    foreach string name in (set.getActionNames()) {
        RestSchemaActionOperation op = set.getOperationEx(name);
        foreach *AbstractDataProviderType type in (op.getRequestType(), op.getResponseType()) {
            if (type) {
                AbstractDataProviderType restored = Serializable::deserialize(type.serialize());
                restored = Serializable::deserialize(restored.serialize());
                if (restored.getBaseTypeName() != type.getBaseTypeName()) {
                    throw "INSTALL-QUALIFICATION-ERROR", "type serialization changed its contract";
                }
            }
        }
    }
}
list<hash<DataProviderDiscoveryExpectedIdentityInfo>> expected = (
    <DataProviderDiscoveryExpectedIdentityInfo>{"app": App},
) + (map <DataProviderDiscoveryExpectedIdentityInfo>{"app": App, "action": $1}, Actions);
DataProviderDiscoverySession session = DataProviderActionCatalog::beginDiscovery(expected, (App,));
DataProviderActionCatalog::collectDiscoveryExpectedInventory(session);
DataProviderQualifiedDiscovery qualification = DataProviderActionCatalog::sealDiscovery(session);
if (!qualification.getReport().complete || qualification.getReport().failures.size()) {
    throw "INSTALL-QUALIFICATION-ERROR", qualification.getReport();
}
DataProviderActionCatalog::discardQualifiedDiscovery(qualification);

string index_dir = make_tmp_dir("assemblyai-installed-index-");
on_exit remove_tree(index_dir);
hash<auto> published = ProviderIndex::ProviderIndex::createDataProviderIndex(
    <ProviderIndexUtil::DataProviderIndexInfo>{}, index_dir, <ProviderIndex::ProviderIndexCreateOptions>{
        "load_known_factories": False, "load_environment": False, "load_typescript_actions": False,
        "apps": (App,), "expected_inventory": expected,
    });
hash<auto> readback = ProviderIndexUtil::ProviderIndexUtil::readDataProviderIndex();
if (!published.summary.qualification.complete || published.summary.qualification.failures.size()
        || sort(keys readback.actioninfomap{App}) != sort(Actions)) {
    throw "INSTALL-QUALIFICATION-ERROR", "published index did not retain the qualified inventory";
}
hash<auto> model_option = readback.actioninfomap{App}."create-transcript".options.speech_models;
if ((map $1.value, model_option.element_allowed_values) != ("universal-3-5-pro", "universal-2")
        || model_option.element_allowed_values[0].display_name != "Universal-3.5 Pro") {
    throw "INSTALL-QUALIFICATION-ERROR", "published structured choices changed wire values or labels";
}
AbstractDataProviderType format_type = AbstractDataProviderType::get(
    readback.actioninfomap{App}."create-chat-completion".options.response_format.type_info);
foreach hash<auto> value in ({}, {"value": {"enum": (False, 0, "", {}, ())}}) {
    hash<auto> format = {"type": "json_schema", "json_schema": {"name": "fixture", "schema": value}};
    if (format_type.acceptsValue(format) != format) {
        throw "INSTALL-QUALIFICATION-ERROR", "published JSON-schema type changed an opaque document";
    }
}
printf("installed=%s actions=33 documents=8 operations=31 failures=0 qualification=complete index=verified\n",
    get_module_hash().AssemblyAiDataProvider.filename);
