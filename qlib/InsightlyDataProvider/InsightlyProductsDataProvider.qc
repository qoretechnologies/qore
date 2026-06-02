# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyProductsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyProductsDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "products", "desc": "Insightly products data provider",
            "type": "InsightlyProductsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Product ID", "short_desc": "Get a specific product by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The product ID to retrieve",
            },
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("InsightlyCreateProductDataProvider"),
            "update": Class::forName("InsightlyUpdateProductDataProvider"),
            "delete": Class::forName("InsightlyDeleteProductDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "products"; }
    *string getDesc() { return "Insightly products data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyProductDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Products", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
