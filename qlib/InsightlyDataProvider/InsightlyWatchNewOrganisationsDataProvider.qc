# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyWatchNewOrganisationsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyWatchNewOrganisationsDataProvider inherits InsightlyWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_ORGANISATION,
            "desc": "Fires when a new organisation is created in Insightly",
            "type": "InsightlyWatchNewOrganisationsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : InsightlyWatchDataProviderBase(rest, options) {
    }

    string getName() { return EVENT_NEW_ORGANISATION; }
    *string getDesc() { return "Fires when a new organisation is created in Insightly"; }
    string getEventName() { return EVENT_NEW_ORGANISATION; }
    string getIdField() { return "ORGANISATION_ID"; }

    AbstractDataProviderType getRecordDataType() {
        return new InsightlyOrganisationDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("Organisations");
    }

    hash<auto> getFakeExampleData() {
        return {
            "ORGANISATION_ID": 100001,
            "ORGANISATION_NAME": "Acme Corp",
            "DATE_CREATED_UTC": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
