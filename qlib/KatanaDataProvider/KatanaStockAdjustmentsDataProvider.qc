# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaStockAdjustmentsDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "stock_adjustments", "desc": "Katana stock adjustments collection",
            "type": "KatanaStockAdjustmentsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Stock Adjustment ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Stock adjustment ID to retrieve"},
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by location ID", "ref_data": "locations"},
            "stock_adjustment_number": <DataProviderOptionInfo>{
                "display_name": "Adjustment Number",
                "short_desc": "Filter by adjustment number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by stock adjustment number"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateStockAdjustmentDataProvider"),
            "update": Class::forName("KatanaUpdateStockAdjustmentDataProvider"),
            "delete": Class::forName("KatanaDeleteStockAdjustmentDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "stock_adjustments"; }
    *string getDesc() { return "Katana stock adjustments collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaStockAdjustmentDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Katana has no GET /stock_adjustments/{id}; fetch a single record via the ids= filter
        return runListSearch("stock_adjustments", where_cond, search_options, "id", True);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
