# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaManufacturingOrdersDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "manufacturing-orders",
            "desc": "Katana manufacturing orders collection",
            "type": "KatanaManufacturingOrdersDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Order ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Manufacturing order ID to retrieve"},
            "status": <DataProviderOptionInfo>{
                "display_name": "Status", "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by manufacturing order status",
                "allowed_values": MoStatusUpdateAllowed},
            "location_id": <DataProviderOptionInfo>{
                "display_name": "Location ID", "short_desc": "Filter by location",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by location ID", "ref_data": "locations"},
            "order_no": <DataProviderOptionInfo>{
                "display_name": "Order Number", "short_desc": "Filter by order number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by order number"},
            "is_linked_to_sales_order": <DataProviderOptionInfo>{
                "display_name": "Linked To Sales Order",
                "short_desc": "Filter MOs linked to a sales order",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, returns only MOs created from a sales order"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateManufacturingOrderDataProvider"),
            "update": Class::forName("KatanaUpdateManufacturingOrderDataProvider"),
            "delete": Class::forName("KatanaDeleteManufacturingOrderDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "manufacturing-orders"; }
    *string getDesc() { return "Katana manufacturing orders collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaManufacturingOrderDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("manufacturing_orders", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
