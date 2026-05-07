# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyCalendarEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyCalendarEventsDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "calendar-events", "desc": "Insightly calendar events data provider",
            "type": "InsightlyCalendarEventsDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Event ID", "short_desc": "Get a specific event by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The event ID to retrieve",
            },
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("InsightlyCreateCalendarEventDataProvider"),
            "update": Class::forName("InsightlyUpdateCalendarEventDataProvider"),
            "delete": Class::forName("InsightlyDeleteCalendarEventDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "calendar-events"; }
    *string getDesc() { return "Insightly calendar events data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyCalendarEventDataType().getFields();
    }
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Events", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
