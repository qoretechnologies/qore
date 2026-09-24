#!/usr/bin/env qore
# Copyright 2026 Qore Technologies, s.r.o. SPDX-License-Identifier: MIT
# No source prepend: this helper qualifies fresh installed artifacts only.
%modern
%requires BflDataProvider
%requires ProviderIndex
%requires FsUtil
%requires json

const App = "BlackForestLabs";
# Independent reviewed inventory; never obtained from the provider manifest.
const Actions = ("generate-image", "generate-video", "get-routed-result", "wait-for-result",
            "download-result", "make-api-call", "get-finetune", "list-finetunes", "delete-finetune", "get-credits",
            "get-result", "start-flux-pro-1-1", "start-flux-dev", "start-flux-pro-1-1-ultra", "start-fill",
            "start-expand", "start-finetuned-fill", "start-finetuned-ultra", "start-flux-2-flex",
            "start-flux-2-klein-9b", "start-flux-2-klein-9b-preview", "start-flux-2-klein-4b", "start-flux-2-max",
            "start-flux-2-pro-preview", "start-flux-2-pro", "start-video", "start-video-upscale", "start-video-edit",
            "start-outpaint", "start-try-on-v1", "start-try-on-v2", "start-erase", "start-deblur",
            "start-flux-kontext-max", "start-flux-kontext-pro", "report-license-usage", "start-flux-pro",
            "start-lora-4b", "start-lora-9b", "start-lora-9b-kv", "start-lora-9b-kv-bf16",
            "start-lora-base-4b", "start-lora-base-9b");

if (!ARGV[0] || ENV.QORE_MODULE_DIR_ONLY != "1") {
    throw "INSTALL-QUALIFICATION-ERROR", "pass the empty-prefix installation root and exclude default module paths";
}
string stage = ARGV[0];
hash<string, string> artifacts = {};
foreach string name in ("BflDataProvider", "BflRestClient", "QoreMediaUtils", "RestSchemaActions",
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
                || (!action.options && !inlist(action.action, ("get-credits", "list-finetunes")))) {
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

string index_dir = make_tmp_dir("bfl-installed-index-");
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
hash<auto> mode_choice = readback.actioninfomap{App}."start-video".options.mode;
if (!inlist("t2v", map $1.value, mode_choice.allowed_values)
        || readback.actioninfomap{App}."start-video".options.generate_audio.type.acceptsValue(False) !== False) {
    throw "INSTALL-QUALIFICATION-ERROR", "published choices or boolean false were lost";
}
# output types are not indexed; check the registered type's serialization directly
AbstractDataProviderType extras = Serializable::deserialize(
    DataProviderActionCatalog::getAppActionEx(App, "get-finetune").output_type.serialize()).getFields()
    .finetune_details.getType();
hash<auto> ordinary = {"value": {"value": "literal"}, "enabled": False, "count": 0};
if (extras.acceptsValue(ordinary) != ordinary) {
    throw "INSTALL-QUALIFICATION-ERROR", "published object type interpreted an ordinary value member as a choice";
}
AbstractDataProviderType keyframes = readback.actioninfomap{App}."start-video".options.keyframes.type;
for (int cycle = 0; cycle < 3; ++cycle) { keyframes = Serializable::deserialize(keyframes.serialize()); }
if (keyframes.acceptsValue((0.0, "https://example.org/image.png")) != (0.0, "https://example.org/image.png")) {
    throw "INSTALL-QUALIFICATION-ERROR", "published positional keyframes lost their structure";
}
AbstractDataProviderType media = Serializable::deserialize(
    DataProviderActionCatalog::getAppActionEx(App, "generate-image").output_type.serialize());
hash<auto> completed = {"status": "Ready", "id": "installed-fixture", "cost": number("1.5"),
    "result": {"sample": "https://delivery.eu1.bfl.ai/fixture.png", "seed": 0},
    "media": {"data": binary("typed fixture"), "content_type": "image/png", "width": 64, "height": 64}};
if (media.acceptsValue(completed) != completed
        || media.acceptsValue({"status": "Pending", "id": "installed-fixture", "progress": number(0)}).status != "Pending") {
    throw "INSTALL-QUALIFICATION-ERROR", "restored output lost media or pending job variant";
}

string domain = "data-provider.QmxhY2tGb3Jlc3RMYWJz";
string catalog_root = stage + "/usr/share/qore/i18n";
hash<auto> root_catalog = parse_json(File::readTextFile(catalog_root + "/" + domain
    + "/root/BflDataProvider.json"));
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
if (inspection.issues.size() || inspection.translation_count != 4993 * 12
        || inspection.translation_message_ids_by_locale.size() != 12) {
    throw "INSTALL-QUALIFICATION-ERROR", "installed locale catalogs are incomplete or stale";
}
hash<auto> wait_action;
foreach hash<auto> action in (actions) { if (action.action == "wait-for-result") { wait_action = action; } }
foreach hash<auto> sample in (({"locale": "cs", "label": "Počkat na výsledek"},
        {"locale": "de", "label": "Auf Ergebnis warten"},
        {"locale": "ja", "label": "結果を待機"},
        {"locale": "zh-TW", "label": "等待結果"})) {
    DataProviderPresentationCatalog catalog(domain, sample.locale, catalog_options);
    hash<auto> localized = catalog.projectAction(app, wait_action);
    if (localized.display_name != sample.label || localized.action != "wait-for-result") {
        throw "INSTALL-QUALIFICATION-ERROR", "installed localization did not preserve presentation and identity";
    }
}
printf("actions=43 failures=0 registration=complete presentation=complete messages=4993 locales=12 "
    "missing-action=rejected index=verified artifacts=%s\n", make_json(artifacts));
