#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o.
# Run with QORE_MODULE_DIR_ONLY=1 and an installed-only module path; pass the stage prefix.
%modern
%requires AwsTextractDataProvider
%requires ProviderIndex
%requires FsUtil

const Actions = ("analyze-document", "analyze-expense", "analyze-id", "create-adapter",
            "create-adapter-version", "delete-adapter", "delete-adapter-version", "detect-document-text",
            "get-adapter", "get-adapter-version", "get-document-analysis", "get-document-text-detection",
            "get-expense-analysis", "get-lending-analysis", "get-lending-analysis-summary", "list-adapter-versions",
            "list-adapters", "list-tags-for-resource", "start-document-analysis", "start-document-text-detection",
            "start-expense-analysis", "start-lending-analysis", "tag-resource", "untag-resource", "update-adapter",
            "make-api-call");

if (!ARGV[0] || ENV.QORE_MODULE_DIR_ONLY != "1") {
    throw "INSTALL-QUALIFICATION-ERROR", "pass an empty-prefix install stage and exclude default module paths";
}
string stage = ARGV[0];
foreach string module_name in ("AwsTextractDataProvider", "AwsTextractRestClient", "OpenApi3", "AwsRestClient", "Mime", "HttpClientIo", "logger_bin") {
    string loaded = get_module_hash(){module_name}.filename;
    if (index(loaded, stage + "/") != 0 || loaded !~ /\.qmod$/) {
        throw "INSTALL-QUALIFICATION-ERROR", sprintf("%s did not load its installed AOT artifact: %s", module_name, loaded);
    }
}
const App = AwsTextractDataProvider::AppName;
list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActionsEx(App);
if (sort(map $1.action, actions) != sort(Actions)
        || DataProviderActionCatalog::getInitializationFailures((App,)).size()) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed action inventory or initialization failures mismatch";
}
foreach hash<auto> op in (AwsTextractManifest::Operations.iterator()) {
    foreach string name in ((op.input, op.output)) {
        AbstractDataProviderType type = AwsTextractSchema::getType(name);
        AbstractDataProviderType restored = Serializable::deserialize(type.serialize());
        restored = Serializable::deserialize(restored.serialize());
        if (sort(keys restored.getFields()) != sort(keys type.getFields())) {
            throw "INSTALL-QUALIFICATION-ERROR", "type serialization changed nested fields";
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

string index_dir = make_tmp_dir("textract-installed-index-");
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
hash<auto> features = readback.actioninfomap{App}."analyze-document".options.feature_types;
if ((map $1.value, features.element_allowed_values) != ("TABLES", "FORMS", "SIGNATURES", "LAYOUT", "QUERIES")) {
    throw "INSTALL-QUALIFICATION-ERROR", "published structured choices changed wire values";
}
AbstractDataProviderType tags = AbstractDataProviderType::get(
    readback.actioninfomap{App}."tag-resource".options.Tags.type_info);
if (tags.acceptsValue({"value": ""}) != {"value": ""}) {
    throw "INSTALL-QUALIFICATION-ERROR", "published map type changed an ordinary value key";
}
printf("installed=%s actions=26 operations=25 failures=0 qualification=complete index=verified\n",
    get_module_hash().AwsTextractDataProvider.filename);
