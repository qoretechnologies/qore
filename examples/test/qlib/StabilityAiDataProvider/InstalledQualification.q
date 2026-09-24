#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# No source prepend: this helper qualifies fresh installed artifacts only.
%modern
%requires StabilityAiDataProvider
%requires ProviderIndex
%requires FsUtil
%requires json

const App = "StabilityAi";
# Independent reviewed inventory; never obtained from the provider manifest.
const Actions = ("generate-image", "make-api-call", "generate-ultra", "generate-core", "generate-sd3",
    "upscale-fast", "upscale-conservative", "start-creative-upscale", "get-upscale-result",
    "erase-object", "inpaint-image", "outpaint-image", "search-and-replace", "search-and-recolor",
    "remove-background", "start-background-relight", "get-image-result", "control-sketch",
    "control-structure", "control-style", "transfer-style", "generate-fast-3d", "generate-point-aware-3d",
    "generate-audio-2", "transform-audio-2", "inpaint-audio-2", "start-audio-3", "start-audio-3-transform",
    "start-audio-3-inpaint", "get-audio-result", "get-account", "get-balance", "list-engines",
    "legacy-text-to-image", "legacy-image-to-image", "legacy-mask-image", "wait-for-image", "wait-for-upscale", "wait-for-audio");

if (!ARGV[0] || ENV.QORE_MODULE_DIR_ONLY != "1") {
    throw "INSTALL-QUALIFICATION-ERROR", "pass the empty-prefix installation root and exclude default module paths";
}
string stage = ARGV[0];
hash<string, string> artifacts = {};
foreach string name in ("StabilityAiDataProvider", "StabilityAiRestClient", "QoreMediaUtils", "RestSchemaActions",
        "RestSchemaValidator", "RestSchemaDataProvider", "OpenApi3", "DataProvider", "QoreAsyncJobUtils",
        "ConnectionProvider", "RestClient", "RestClientIo", "HttpClientIo", "ProviderIndex", "ProviderIndexUtil",
        "json", "i18n", "logger_bin") {
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
                || (!action.options && !inlist(action.action, ("get-account", "get-balance", "list-engines")))) {
            throw "INSTALL-QUALIFICATION-ERROR", "incomplete action presentation: " + action.action;
        }
        foreach AbstractDataProviderType type in ((action.output_type,) + (map $1.type, action.options.iterator())) {
            AbstractDataProviderType restored = type;
            for (int cycle = 0; cycle < 3; ++cycle) { restored = Serializable::deserialize(restored.serialize()); }
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

string index_dir = make_tmp_dir("stability-ai-installed-index-");
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
hash<auto> background_choice = readback.actioninfomap{App}."start-background-relight".options.keep_original_background;
if (!inlist(False, map $1.value, background_choice.allowed_values)
        || background_choice.type.acceptsValue(False) !== False) {
    throw "INSTALL-QUALIFICATION-ERROR", "published choices lost boolean false";
}
AbstractDataProviderType extras = readback.actioninfomap{App}."legacy-text-to-image".options.extras.type;
hash<auto> ordinary = {"value": {"value": "literal"}, "enabled": False, "count": 0};
if (extras.acceptsValue(ordinary) != ordinary) {
    throw "INSTALL-QUALIFICATION-ERROR", "published object type interpreted an ordinary value member as a choice";
}
# output types are not indexed; check the registered type's serialization directly
AbstractDataProviderType media = Serializable::deserialize(
    DataProviderActionCatalog::getAppActionEx(App, "get-image-result").output_type.serialize());
hash<auto> completed = {"status": "succeeded", "job_id": strmul("a", 64),
    "artifacts": ({"data": binary("typed fixture"), "content_type": "image/png", "seed": 0, "finish_reason": "SUCCESS"},)};
if (media.acceptsValue(completed) != completed
        || media.acceptsValue({"status": "pending", "job_id": strmul("a", 64)}).status != "pending") {
    throw "INSTALL-QUALIFICATION-ERROR", "restored output lost media or pending job variant";
}

string domain = "data-provider.U3RhYmlsaXR5QWk";
string catalog_root = stage + "/usr/share/qore/i18n";
hash<auto> root_catalog = parse_json(File::readTextFile(catalog_root + "/" + domain
    + "/root/StabilityAiDataProvider.json"));
hash<auto> app = DataProviderActionCatalog::getApp(App);
auto actions = DataProviderActionCatalog::getActionsEx(App);
hash<auto> fresh = DataProviderPresentation::buildSourceCatalogs(app, actions){domain};
if (fresh.locales.root.messages != root_catalog.locales.root.messages
        || DataProviderPresentation::inspectSourceLocales(app, actions).size()) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed recursive presentation or source locale differs";
}
hash<auto> catalog_options = {"search_path": (catalog_root,)};
hash<auto> inspection = DataProviderPresentation::inspectTranslationCatalogs(domain,
    root_catalog.locales.root.messages, catalog_options);
if (inspection.issues.size() || inspection.translation_count != 2416 * 12
        || inspection.translation_message_ids_by_locale.size() != 12) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed locale catalogs are incomplete or stale";
}
hash<auto> wait_action;
foreach hash<auto> action in (actions) { if (action.action == "wait-for-image") { wait_action = action; } }
foreach hash<auto> sample in (({"locale": "cs", "label": "Počkat na dokončení obrazové úlohy"},
        {"locale": "de", "label": "Auf Abschluss des Bildauftrags warten"},
        {"locale": "ja", "label": "画像ジョブの完了を待機"},
        {"locale": "zh-TW", "label": "等待影像作業完成"})) {
    DataProviderPresentationCatalog catalog(domain, sample.locale, catalog_options);
    hash<auto> localized = catalog.projectAction(app, wait_action);
    if (localized.display_name != sample.label || localized.action != "wait-for-image") {
        throw "INSTALL-QUALIFICATION-ERROR", "installed localization did not preserve presentation and identity";
    }
}
printf("actions=39 failures=0 registration=complete presentation=complete messages=2416 locales=12 "
    "missing-action=rejected index=verified artifacts=%s\n", make_json(artifacts));
