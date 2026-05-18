# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyProjectsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyProjectsDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "projects",
            "desc": "Insightly projects data provider",
            "type": "InsightlyProjectsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Project ID",
                "short_desc": "Get a specific project by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The project ID to retrieve",
            },
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("InsightlyCreateProjectDataProvider"),
            "update": Class::forName("InsightlyUpdateProjectDataProvider"),
            "delete": Class::forName("InsightlyDeleteProjectDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "projects"; }
    *string getDesc() { return "Insightly projects data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyProjectDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Projects", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
