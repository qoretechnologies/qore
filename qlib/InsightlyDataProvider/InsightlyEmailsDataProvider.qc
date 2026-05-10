# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyEmailsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyEmailsDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "emails", "desc": "Insightly emails data provider",
            "type": "InsightlyEmailsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Email ID", "short_desc": "Get a specific email by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The email ID to retrieve",
            },
        } + CommonSearchOptions;
        # Insightly v3.1 API supports only delete for emails (no POST/PUT)
        const ChildMap = {
            "delete": Class::forName("InsightlyDeleteEmailDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "emails"; }
    *string getDesc() { return "Insightly emails data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyEmailDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Emails", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
