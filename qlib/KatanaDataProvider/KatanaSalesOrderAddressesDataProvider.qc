# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaSalesOrderAddressesDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "sales_order_addresses",
            "desc": "Katana sales order addresses collection",
            "type": "KatanaSalesOrderAddressesDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Address ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Sales order address ID to retrieve"},
            "sales_order_id": <DataProviderOptionInfo>{
                "display_name": "Sales Order ID",
                "short_desc": "Filter by sales order",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by parent sales order ID",
                "ref_data": "sales_orders"},
            "entity_type": <DataProviderOptionInfo>{
                "display_name": "Entity Type",
                "short_desc": "Address type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by address type (`billing` or `shipping`)",
                "allowed_values": AddressEntityTypeAllowed},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateSalesOrderAddressDataProvider"),
            "update": Class::forName("KatanaUpdateSalesOrderAddressDataProvider"),
            "delete": Class::forName("KatanaDeleteSalesOrderAddressDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "sales_order_addresses"; }
    *string getDesc() { return "Katana sales order addresses collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaSalesOrderAddressDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Katana has no GET /sales_order_addresses/{id}; fetch a single record via ids= filter.
        # Also rename sales_order_id to sales_order_ids (the API filter name).
        *hash<auto> opts = search_options;
        if (opts && exists opts.sales_order_id) {
            opts = opts - "sales_order_id" + {"sales_order_ids": string(opts.sales_order_id)};
        }
        return runListSearch("sales_order_addresses", where_cond, opts, "id", True);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
