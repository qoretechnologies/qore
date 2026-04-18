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
            "skip": <DataProviderOptionInfo>{
                "display_name": "Skip", "short_desc": "Number of records to skip",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of records to skip for OData pagination", "preselected": True,
            },
            "top": <DataProviderOptionInfo>{
                "display_name": "Top", "short_desc": "Maximum number of records to return",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Maximum number of records to return (max " + MaxPageSize + ")",
                "preselected": True,
            },
            "brief": <DataProviderOptionInfo>{
                "display_name": "Brief", "short_desc": "Return brief results",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, returns only a subset of fields for each record",
            },
            "count_total": <DataProviderOptionInfo>{
                "display_name": "Count Total", "short_desc": "Include total count in response",
                "type": AbstractDataProviderType::get(SoftBoolOrNothingType),
                "desc": "If `True`, includes the total count of matching records",
            },
        };
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
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        if (search_options.id) {
            hash<auto> result = doGet("Tasks/" + string(search_options.id));
            return new DefaultRecordIterator(new ListHashIterator((result,)),
                where_cond, search_options);
        }
        hash<auto> params;
        if (search_options.skip) { params.skip = search_options.skip; }
        if (search_options.top) { params.top = search_options.top; }
        if (search_options.brief) { params.brief = search_options.brief ? "true" : "false"; }
        if (search_options.count_total) {
            params.count_total = search_options.count_total ? "true" : "false";
        }
        list<hash<auto>> records;
        if (search_options.top || search_options.skip) {
            records = doGetList("Tasks", params);
        } else {
            records = fetchAllPages("Tasks");
        }
        return new DefaultRecordIterator(new ListHashIterator(records),
            where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
