# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyWatchNewLeadsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyWatchNewLeadsDataProvider inherits InsightlyWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_LEAD,
            "desc": "Fires when a new lead is created in Insightly",
            "type": "InsightlyWatchNewLeadsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : InsightlyWatchDataProviderBase(rest, options) {
    }

    string getName() { return EVENT_NEW_LEAD; }
    *string getDesc() { return "Fires when a new lead is created in Insightly"; }
    string getEventName() { return EVENT_NEW_LEAD; }
    string getIdField() { return "LEAD_ID"; }

    AbstractDataProviderType getRecordDataType() {
        return new InsightlyLeadDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        return fetchRecordsUpdatedAfter("Leads", since);
    }

    hash<auto> getFakeExampleData() {
        return {
            "LEAD_ID": 100001,
            "FIRST_NAME": "Robert",
            "LAST_NAME": "Johnson",
            "TITLE": "VP Engineering",
            "EMAIL": "robert.johnson@example.com",
            "PHONE": "+1-555-0142",
            "MOBILE": "+1-555-0143",
            "ORGANISATION_NAME": "Example Corp",
            "WEBSITE": "https://example.com",
            "INDUSTRY": "Technology",
            "EMPLOYEE_COUNT": 250,
            "LEAD_RATING": 4,
            "LEAD_SOURCE_ID": 1,
            "LEAD_STATUS_ID": 1,
            "RESPONSIBLE_USER_ID": 12345,
            "OWNER_USER_ID": 12345,
            "VISIBLE_TO": "EVERYONE",
            "DATE_CREATED_UTC": "2026-04-01T10:30:00Z",
            "DATE_UPDATED_UTC": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
