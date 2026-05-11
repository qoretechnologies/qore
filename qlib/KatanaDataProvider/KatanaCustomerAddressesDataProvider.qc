# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCustomerAddressesDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "customer_addresses", "desc": "Katana customer addresses collection",
            "type": "KatanaCustomerAddressesDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Address ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Customer address ID to retrieve"},
            "customer_id": <DataProviderOptionInfo>{
                "display_name": "Customer ID", "short_desc": "Filter by customer",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Filter by parent customer ID",
                "ref_data": "customers"},
            "entity_type": <DataProviderOptionInfo>{
                "display_name": "Entity Type", "short_desc": "Address type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by address type (`billing` or `shipping`)",
                "allowed_values": AddressEntityTypeAllowed},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateCustomerAddressDataProvider"),
            "update": Class::forName("KatanaUpdateCustomerAddressDataProvider"),
            "delete": Class::forName("KatanaDeleteCustomerAddressDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "customer_addresses"; }
    *string getDesc() { return "Katana customer addresses collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaCustomerAddressDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Katana has no GET /customer_addresses/{id}; fetch a single record via ids= filter.
        # Also rename customer_id to customer_ids (the API filter name).
        *hash<auto> opts = search_options;
        if (opts && exists opts.customer_id) {
            opts = opts - "customer_id" + {"customer_ids": string(opts.customer_id)};
        }
        return runListSearch("customer_addresses", where_cond, opts, "id", True);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
