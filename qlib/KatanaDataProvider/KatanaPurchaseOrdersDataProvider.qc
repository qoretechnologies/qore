# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaPurchaseOrdersDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "purchase_orders", "desc": "Katana purchase orders collection",
            "type": "KatanaPurchaseOrdersDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Order ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Purchase order ID to retrieve"},
            "status": <DataProviderOptionInfo>{
                "display_name": "Status", "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by purchase order status",
                "allowed_values": PoStatusUpdateAllowed},
            "supplier_id": <DataProviderOptionInfo>{
                "display_name": "Supplier ID", "short_desc": "Filter by supplier",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by supplier ID", "ref_data": "suppliers"},
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by location ID", "ref_data": "locations"},
            "order_no": <DataProviderOptionInfo>{
                "display_name": "Order Number", "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by order number"},
            "entity_type": <DataProviderOptionInfo>{
                "display_name": "Entity Type",
                "short_desc": "Entity type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by purchase order entity type (`regular` or `outsourced`)",
                "allowed_values": PoEntityTypeAllowed},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreatePurchaseOrderDataProvider"),
            "update": Class::forName("KatanaUpdatePurchaseOrderDataProvider"),
            "delete": Class::forName("KatanaDeletePurchaseOrderDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "purchase_orders"; }
    *string getDesc() { return "Katana purchase orders collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaPurchaseOrderDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("purchase_orders", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
