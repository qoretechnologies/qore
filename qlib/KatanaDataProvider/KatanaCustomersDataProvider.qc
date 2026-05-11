# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaCustomersDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "customers", "desc": "Katana customers collection",
            "type": "KatanaCustomersDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Customer ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Customer ID to retrieve"},
            "name": <DataProviderOptionInfo>{
                "display_name": "Name", "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer name"},
            "first_name": <DataProviderOptionInfo>{
                "display_name": "First Name", "short_desc": "Filter by first name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer first name"},
            "last_name": <DataProviderOptionInfo>{
                "display_name": "Last Name", "short_desc": "Filter by last name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer last name"},
            "company": <DataProviderOptionInfo>{
                "display_name": "Company", "short_desc": "Filter by company",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by company name"},
            "email": <DataProviderOptionInfo>{
                "display_name": "Email", "short_desc": "Filter by email",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by email address"},
            "phone": <DataProviderOptionInfo>{
                "display_name": "Phone", "short_desc": "Filter by phone",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by phone number"},
            "category": <DataProviderOptionInfo>{
                "display_name": "Category", "short_desc": "Filter by category",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by customer category"},
            "reference_id": <DataProviderOptionInfo>{
                "display_name": "Reference ID",
                "short_desc": "Filter by external reference",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by external reference identifier"},
            "currency": <DataProviderOptionInfo>{
                "display_name": "Currency",
                "short_desc": "Filter by currency",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by ISO 4217 currency code (e.g. `USD`, `EUR`)"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateCustomerDataProvider"),
            "update": Class::forName("KatanaUpdateCustomerDataProvider"),
            "delete": Class::forName("KatanaDeleteCustomerDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "customers"; }
    *string getDesc() { return "Katana customers collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaCustomerDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Katana has no GET /customers/{id}; fetch a single record via the ids= filter
        return runListSearch("customers", where_cond, search_options, "id", True);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
