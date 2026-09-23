#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# No source prepend: this helper qualifies fresh installed artifacts only.
%modern
%requires GoogleDocumentAiDataProvider
%requires ProviderIndex
%requires FsUtil
%requires json

const App = "GoogleDocumentAi";
# Independent reviewed inventory; never obtained from the provider manifest.
const Actions = ("process-document", "wait-for-operation", "make-api-call", "list-locations", "get-location",
    "list-processor-types", "get-processor-type", "list-processors", "get-processor", "create-processor",
    "delete-processor", "enable-processor", "disable-processor", "set-default-version", "start-batch-processing",
    "start-version-batch-processing", "list-processor-versions", "get-processor-version", "delete-processor-version",
    "train-processor-version", "deploy-processor-version", "undeploy-processor-version", "evaluate-processor-version",
    "list-evaluations", "get-evaluation", "list-operations", "get-operation", "cancel-operation", "list-schemas",
    "get-schema", "create-schema", "update-schema", "delete-schema", "list-schema-versions", "get-schema-version",
    "create-schema-version", "update-schema-version", "delete-schema-version", "generate-schema-version",
    "beta-update-dataset", "beta-get-dataset-schema", "beta-update-dataset-schema", "beta-get-dataset-document",
    "beta-list-dataset-documents", "beta-import-dataset-documents", "beta-delete-dataset-documents",
    "beta-import-processor-version");

if (!ARGV[0] || ENV.QORE_MODULE_DIR_ONLY != "1") {
    throw "INSTALL-QUALIFICATION-ERROR", "pass the empty-prefix installation root and exclude default module paths";
}
string stage = ARGV[0];
hash<string, string> artifacts = {};
foreach string name in ("GoogleDocumentAiDataProvider", "GoogleDocumentAiRestClient", "GcpAuth", "RestSchemaActions",
        "RestSchemaDataProvider", "OpenApi3", "DataProvider", "QoreAsyncJobUtils", "RestClient", "RestClientIo",
        "HttpClientIo", "ProviderIndex", "ProviderIndexUtil", "json", "i18n", "logger_bin") {
    string loaded = get_module_hash(){name}.filename;
    if (loaded.find(stage + "/") != 0 || loaded !~ /\.qmod$/) {
        throw "INSTALL-QUALIFICATION-ERROR", sprintf("%s did not load its installed AOT/native artifact: %s", name, loaded);
    }
    artifacts{name} = loaded;
}

for (int pass = 0; pass < 2; ++pass) {
    list<hash<DataProviderActionInfo>> actions = DataProviderActionCatalog::getActionsEx(App);
    if (sort(map $1.action, actions) != sort(Actions)
            || DataProviderActionCatalog::getInitializationFailures((App,)).size()) {
        throw "INSTALL-QUALIFICATION-ERROR", "action inventory or structured initialization failures mismatch";
    }
    foreach hash<DataProviderActionInfo> action in (actions) {
        if (!action.display_name.val() || !action.short_desc.val() || action.short_desc.size() >= 80
                || !action.desc.val() || !action.output_type || action.cls
                || (!action.options && action.action != "get-location")) {
            throw "INSTALL-QUALIFICATION-ERROR", "incomplete action presentation: " + action.action;
        }
        if (action.options.hasKey("project_id") || action.options.hasKey("location")) {
            throw "INSTALL-QUALIFICATION-ERROR", "connection scope leaked into an action form";
        }
        foreach AbstractDataProviderType type in ((action.output_type,) + (map $1.type, action.options.iterator())) {
            AbstractDataProviderType restored = Serializable::deserialize(type.serialize());
            restored = Serializable::deserialize(restored.serialize());
            if (sort(keys restored.getFields()) != sort(keys type.getFields())) {
                throw "INSTALL-QUALIFICATION-ERROR", "type serialization changed fields";
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
if (!rejected) { throw "INSTALL-QUALIFICATION-ERROR", "a missing action did not prevent qualification"; }

string index_dir = make_tmp_dir("google-document-ai-installed-index-");
on_exit remove_tree(index_dir);
hash<auto> published = ProviderIndex::ProviderIndex::createDataProviderIndex(
    <ProviderIndexUtil::DataProviderIndexInfo>{}, index_dir, <ProviderIndex::ProviderIndexCreateOptions>{
        "load_known_factories": False, "load_environment": False, "load_typescript_actions": False,
        "apps": (App,), "expected_inventory": expected,
    });
hash<auto> readback = ProviderIndexUtil::ProviderIndexUtil::readDataProviderIndex();
if (!published.summary.qualification.complete || published.summary.qualification.failures.size()
        || sort(keys readback.actioninfomap{App}) != sort(Actions)) {
    throw "INSTALL-QUALIFICATION-ERROR", "published index did not retain complete qualification and inventory";
}
hash<auto> mime = readback.actioninfomap{App}."process-document".options.mime_type;
if (mime.allowed_values[0].value != "application/pdf" || mime.allowed_values[0].display_name != "PDF") {
    throw "INSTALL-QUALIFICATION-ERROR", "published choices changed wire values or labels";
}
AbstractDataProviderType labels = readback.actioninfomap{App}."process-document".options.labels.type;
if (labels.acceptsValue({"value": "literal"}) != {"value": "literal"}) {
    throw "INSTALL-QUALIFICATION-ERROR", "published map type changed an ordinary value member";
}
AbstractDataProviderType result = Serializable::deserialize(
    GoogleDocumentAiProcessDocumentDataProvider::ResponseType.serialize());
auto typed = result.acceptsValue({"document": {"text": "Synthetic 12.34", "entities": ({"type": "amount",
    "normalizedValue": {"moneyValue": {"currencyCode": "USD", "units": "12", "nanos": 340000000}}},)}});
if (typed.document.entities[0].normalizedValue.moneyValue.units !== "12") {
    throw "INSTALL-QUALIFICATION-ERROR", "restored document type lost string-encoded int64 values";
}
string domain = "data-provider.R29vZ2xlRG9jdW1lbnRBaQ";
string catalog_root = stage + "/usr/share/qore/i18n";
hash<auto> root_catalog = parse_json(File::readTextFile(catalog_root + "/" + domain
    + "/root/GoogleDocumentAiDataProvider.json"));
hash<auto> catalog_options = {"search_path": (catalog_root,)};
hash<auto> inspection = DataProviderPresentation::inspectTranslationCatalogs(domain,
    root_catalog.locales.root.messages, catalog_options);
if (inspection.issues.size() || inspection.translation_count != 4963 * 12
        || inspection.translation_message_ids_by_locale.size() != 12) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed locale catalogs are incomplete or stale";
}
hash<auto> wait_action;
foreach hash<auto> action in (DataProviderActionCatalog::getActionsEx(App)) {
    if (action.action == "wait-for-operation") { wait_action = action; }
}
foreach hash<auto> sample in (({"locale": "cs", "label": "Počkat na dokončení operace"},
        {"locale": "de", "label": "Auf Abschluss des Vorgangs warten"},
        {"locale": "ja", "label": "処理の完了を待機"},
        {"locale": "zh-TW", "label": "等待作業完成"})) {
    DataProviderPresentationCatalog catalog(domain, sample.locale, catalog_options);
    hash<auto> localized = catalog.projectAction(DataProviderActionCatalog::getApp(App), wait_action);
    if (localized.display_name != sample.label || localized.action != "wait-for-operation") {
        throw "INSTALL-QUALIFICATION-ERROR", "installed localization did not preserve presentation and identity";
    }
}
printf("actions=47 failures=0 qualification=complete missing-action=rejected index=verified artifacts=%s\n", make_json(artifacts));
