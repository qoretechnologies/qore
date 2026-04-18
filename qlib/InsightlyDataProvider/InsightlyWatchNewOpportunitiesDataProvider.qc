# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyWatchNewOpportunitiesDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyWatchNewOpportunitiesDataProvider inherits InsightlyWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_OPPORTUNITY,
            "desc": "Fires when a new opportunity is created in Insightly",
            "type": "InsightlyWatchNewOpportunitiesDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : InsightlyWatchDataProviderBase(rest, options) {
    }

    string getName() { return EVENT_NEW_OPPORTUNITY; }
    *string getDesc() { return "Fires when a new opportunity is created in Insightly"; }
    string getEventName() { return EVENT_NEW_OPPORTUNITY; }
    string getIdField() { return "OPPORTUNITY_ID"; }

    AbstractDataProviderType getRecordDataType() {
        return new InsightlyOpportunityDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("Opportunities");
    }

    hash<auto> getFakeExampleData() {
        return {
            "OPPORTUNITY_ID": 100001,
            "OPPORTUNITY_NAME": "New Enterprise Deal",
            "OPPORTUNITY_STATE": "Open",
            "DATE_CREATED_UTC": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
