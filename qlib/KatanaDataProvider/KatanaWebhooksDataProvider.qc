# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaWebhooksDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "webhooks", "desc": "Katana webhooks collection",
            "type": "KatanaWebhooksDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Webhook ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Webhook ID to retrieve"},
            "enabled": <DataProviderOptionInfo>{
                "display_name": "Enabled", "short_desc": "Filter by enabled flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, only return enabled webhooks"},
            "url": <DataProviderOptionInfo>{
                "display_name": "URL", "short_desc": "Filter by URL",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by webhook target URL"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateWebhookDataProvider"),
            "update": Class::forName("KatanaUpdateWebhookDataProvider"),
            "delete": Class::forName("KatanaDeleteWebhookDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "webhooks"; }
    *string getDesc() { return "Katana webhooks collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaWebhookDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("webhooks", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
