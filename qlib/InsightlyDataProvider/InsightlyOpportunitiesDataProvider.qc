# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyOpportunitiesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/

public namespace InsightlyDataProvider {

public class InsightlyOpportunitiesDataProvider inherits InsightlyDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "opportunities",
            "desc": "Insightly opportunities data provider",
            "type": "InsightlyOpportunitiesDataProvider",
            "has_record": True,
            "supports_read": True,
            "supports_native_search": True,
            "supports_children": True,
            "children_can_support_apis": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        const SearchOptions = {
            "id": <DataProviderOptionInfo>{
                "display_name": "Opportunity ID",
                "short_desc": "Get a specific opportunity by ID",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "The opportunity ID to retrieve",
            },
        } + CommonSearchOptions;

        const ChildMap = {
            "create": Class::forName("InsightlyCreateOpportunityDataProvider"),
            "update": Class::forName("InsightlyUpdateOpportunityDataProvider"),
            "delete": Class::forName("InsightlyDeleteOpportunityDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : InsightlyDataProviderBase(rest) {
    }

    string getName() { return "opportunities"; }
    *string getDesc() { return "Insightly opportunities data provider"; }

    private *list<string> getChildProviderNamesImpl() { return keys ChildMap; }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) { return cls.newObject(rest); }
    }

    private *hash<string, AbstractDataField> getRecordTypeImpl(*hash<auto> search_options) {
        return new InsightlyOpportunityDataType().getFields();
    }

    private AbstractDataProviderRecordIterator<hash<auto>> searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        return runListSearch("Opportunities", where_cond, search_options);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
