# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaPurchaseOrderRowsDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "purchase_order_rows",
            "desc": "Katana purchase order rows collection",
            "type": "KatanaPurchaseOrderRowsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Row ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Purchase order row ID to retrieve"},
            "purchase_order_id": <DataProviderOptionInfo>{
                "display_name": "Purchase Order ID",
                "short_desc": "Filter by purchase order",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by parent purchase order ID",
                "ref_data": "purchase_orders"},
            "variant_id": <DataProviderOptionInfo>{
                "display_name": "Variant ID", "short_desc": "Filter by variant",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by variant ID", "ref_data": "variants"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreatePurchaseOrderRowDataProvider"),
            "update": Class::forName("KatanaUpdatePurchaseOrderRowDataProvider"),
            "delete": Class::forName("KatanaDeletePurchaseOrderRowDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "purchase_order_rows"; }
    *string getDesc() { return "Katana purchase order rows collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaPurchaseOrderRowRecordDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("purchase_order_rows", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
