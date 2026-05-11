# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaLocationsDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "locations", "desc": "Katana locations collection",
            "type": "KatanaLocationsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Location ID to retrieve"},
            "name": <DataProviderOptionInfo>{
                "display_name": "Name", "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by location name"},
            "is_primary": <DataProviderOptionInfo>{
                "display_name": "Is Primary", "short_desc": "Filter by primary flag",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, only return the primary location"},
        } + CommonSearchOptions;
        const ChildMap = {};
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "locations"; }
    *string getDesc() { return "Katana locations collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaLocationDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("locations", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
