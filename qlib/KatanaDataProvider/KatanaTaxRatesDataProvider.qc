# -*- mode: qore; indent-tabs-mode: nil -*-
/*  Copyright (C) 2026 Qore Technologies, s.r.o.  MIT License - see COPYING. */

public namespace KatanaDataProvider {

public class KatanaTaxRatesDataProvider inherits KatanaDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "tax_rates", "desc": "Katana tax rates collection",
            "type": "KatanaTaxRatesDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Tax Rate ID", "short_desc": "Get by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Tax rate ID to retrieve"},
            "name": <DataProviderOptionInfo>{
                "display_name": "Name", "short_desc": "Filter by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter by tax rate name"},
            "is_default_sales": <DataProviderOptionInfo>{
                "display_name": "Is Default Sales",
                "short_desc": "Filter default sales tax",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, only return the default sales tax rate"},
            "is_default_purchases": <DataProviderOptionInfo>{
                "display_name": "Is Default Purchases",
                "short_desc": "Filter default purchase tax",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, only return the default purchase tax rate"},
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("KatanaCreateTaxRateDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : KatanaDataProviderBase(rest) {}
    string getName() { return "tax_rates"; }
    *string getDesc() { return "Katana tax rates collection"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new KatanaTaxRateDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # Katana has no GET /tax_rates/{id}; fetch a single record via the ids= filter
        return runListSearch("tax_rates", where_cond, search_options, "id", True);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace KatanaDataProvider
