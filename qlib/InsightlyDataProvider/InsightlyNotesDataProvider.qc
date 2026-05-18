# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyNotesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {
public class InsightlyNotesDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "notes", "desc": "Insightly notes data provider",
            "type": "InsightlyNotesDataProvider",
            "has_record": True, "supports_read": True, "supports_native_search": True,
            "supports_children": True, "children_can_support_apis": True,
            "constructor_options": ConstructorOptions, "search_options": SearchOptions,
        };
        const ConstructorOptions = BaseConstructorOptions;
        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Note ID", "short_desc": "Get a specific note by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The note ID to retrieve",
            },
        } + CommonSearchOptions;
        const ChildMap = {
            "create": Class::forName("InsightlyCreateNoteDataProvider"),
            "update": Class::forName("InsightlyUpdateNoteDataProvider"),
            "delete": Class::forName("InsightlyDeleteNoteDataProvider"),
        };
    }
    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {}
    string getName() { return "notes"; }
    *string getDesc() { return "Insightly notes data provider"; }
    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }
    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name}; if (cls) { return cls.newObject(rest); }
    }
    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyNoteDataType().getFields();
    }
    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Notes", where_cond, search_options);
    }
    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}
} # namespace InsightlyDataProvider
