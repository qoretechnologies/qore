# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyWatchNewContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyWatchNewContactsDataProvider inherits InsightlyWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CONTACT,
            "desc": "Fires when a new contact is created in Insightly",
            "type": "InsightlyWatchNewContactsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : InsightlyWatchDataProviderBase(rest, options) {
    }

    string getName() { return EVENT_NEW_CONTACT; }
    *string getDesc() { return "Fires when a new contact is created in Insightly"; }
    string getEventName() { return EVENT_NEW_CONTACT; }
    string getIdField() { return "CONTACT_ID"; }

    AbstractDataProviderType getRecordDataType() {
        return new InsightlyContactDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        return fetchRecordsUpdatedAfter("Contacts", since);
    }

    hash<auto> getFakeExampleData() {
        return {
            "CONTACT_ID": 100001,
            "FIRST_NAME": "Jane",
            "LAST_NAME": "Smith",
            "EMAIL_ADDRESS": "jane.smith@example.com",
            "PHONE": "+1-555-0100",
            "DATE_CREATED_UTC": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
