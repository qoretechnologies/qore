#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# Intentionally has no source prepend: qualification must use installed artifacts only.
%modern
%requires AzureDocumentIntelligenceDataProvider
%requires ProviderIndex
%requires FsUtil

const Actions = ("analyze-document", "authorize-classifier-copy", "authorize-model-copy", "build-classifier",
    "build-model", "compose-model", "copy-classifier", "copy-model", "delete-analysis-result", "delete-batch-result",
    "delete-classifier", "delete-model", "get-analysis-result", "get-batch-result", "get-classification-result",
    "get-classifier", "get-model", "get-operation", "get-resource-info", "get-result-figure", "get-result-pdf",
    "list-batch-results", "list-classifiers", "list-models", "list-operations", "make-api-call", "start-analysis",
    "start-analysis-stream", "start-batch-analysis", "start-classification", "start-classification-stream");
const App = "AzureDocumentIntelligence";

if (!ARGV[0] || ENV.QORE_MODULE_DIR_ONLY != "1") {
    throw "INSTALL-QUALIFICATION-ERROR", "pass the artifact root and exclude default module paths";
}
string stage = ARGV[0];
foreach string module_name in ("AzureDocumentIntelligenceDataProvider", "AzureDocumentIntelligenceRestClient",
        "RestSchemaActions", "RestSchemaDataProvider", "OpenApi3", "DataProvider", "Mime", "HttpClientIo", "logger_bin") {
    string loaded = get_module_hash(){module_name}.filename;
    if (index(loaded, stage + "/") != 0 || loaded !~ /\.qmod$/) {
        throw "INSTALL-QUALIFICATION-ERROR", sprintf("%s did not load its qualified AOT artifact: %s", module_name, loaded);
    }
}
for (int pass = 0; pass < 2; ++pass) {
    list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActionsEx(App);
    if (sort(map $1.action, actions) != sort(Actions)
            || DataProviderActionCatalog::getInitializationFailures((App,)).size()) {
        throw "INSTALL-QUALIFICATION-ERROR", "action inventory or initialization failures mismatch";
    }
    foreach hash<DataProviderActionInfo> action in (actions) {
        if (!action.display_name.val() || !action.short_desc.val() || action.short_desc.size() >= 80
                || !action.desc.val() || !action.options || !action.output_type || action.cls) {
            throw "INSTALL-QUALIFICATION-ERROR", "incomplete action presentation: " + action.action;
        }
        foreach AbstractDataProviderType type in ((action.output_type,) + (map $1.type, action.options.iterator())) {
            AbstractDataProviderType restored = Serializable::deserialize(type.serialize());
            restored = Serializable::deserialize(restored.serialize());
            if (sort(keys restored.getFields()) != sort(keys type.getFields())) {
                throw "INSTALL-QUALIFICATION-ERROR", "type serialization changed nested fields";
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
session = DataProviderActionCatalog::beginDiscovery(expected +
    <DataProviderDiscoveryExpectedIdentityInfo>{"app": App, "action": "deliberately-missing"}, (App,));
bool rejected = False;
try { DataProviderActionCatalog::sealDiscovery(session); }
catch (hash<ExceptionInfo> ex) { rejected = ex.err == "DATA-PROVIDER-DISCOVERY-ERROR" && !ex.arg.complete; }
if (!rejected) { throw "INSTALL-QUALIFICATION-ERROR", "missing action did not prevent qualification"; }

string index_dir = make_tmp_dir("azure-di-installed-index-");
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
hash<auto> features = readback.actioninfomap{App}."start-analysis".options.features;
if ((map $1.value, features.element_allowed_values) !=
        ("ocrHighResolution", "languages", "barcodes", "formulas", "keyValuePairs", "styleFont", "queryFields")) {
    throw "INSTALL-QUALIFICATION-ERROR", "published structured choices changed wire values";
}
AbstractDataProviderType tags = readback.actioninfomap{App}."build-model".options.tags.type;
if (tags.acceptsValue({"value": ""}) != {"value": ""}) {
    throw "INSTALL-QUALIFICATION-ERROR", "published map type changed an ordinary value key";
}
printf("installed=%s actions=31 declarations=34 failures=0 qualification=complete index=verified\n",
    get_module_hash().AzureDocumentIntelligenceDataProvider.filename);
