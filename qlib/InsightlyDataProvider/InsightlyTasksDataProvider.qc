# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyTasksDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "tasks", "desc": "Insightly tasks data provider",
            "type": "InsightlyTasksDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Task ID", "short_desc": "Get a specific task by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The task ID to retrieve",
            },
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("InsightlyCreateTaskDataProvider"),
            "update": Class::forName("InsightlyUpdateTaskDataProvider"),
            "delete": Class::forName("InsightlyDeleteTaskDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "tasks"; }
    *string getDesc() { return "Insightly tasks data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyTaskDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Tasks", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
