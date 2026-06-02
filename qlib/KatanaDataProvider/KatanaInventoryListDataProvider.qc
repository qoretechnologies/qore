# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaInventoryListDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "inventory", "desc": "Katana inventory collection",
            "type": "KatanaInventoryListDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "variant_id": <DataProviderOptionInfo>{
                "display_name": "Variant ID", "short_desc": "Filter by variant",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by variant ID", "ref_data": "variants"},
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by location ID", "ref_data": "locations"},
            "include_archived": <DataProviderOptionInfo>{
                "display_name": "Include Archived",
                "short_desc": "Include archived inventory rows",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, include archived inventory rows in the response"},
        } + CommonSearchOptions;
        const ChildMap = {
            "set-safety-stock-level":
                Class::forName("KatanaSetInventorySafetyStockLevelDataProvider"),
            "set-reorder-point":
                Class::forName("KatanaSetInventoryReorderPointDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "inventory"; }
    *string getDesc() { return "Katana inventory collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaInventoryDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("inventory", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
