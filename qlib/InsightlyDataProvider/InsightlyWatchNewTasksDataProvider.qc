# -*- mode: qore; indent-tabs-mode: nil -*-
/*  InsightlyWatchNewTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.
    MIT License - see COPYING for details
*/
public namespace InsightlyDataProvider {

public class InsightlyWatchNewTasksDataProvider inherits InsightlyWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_TASK,
            "desc": "Fires when a new task is created in Insightly",
            "type": "InsightlyWatchNewTasksDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : InsightlyWatchDataProviderBase(rest, options) {
    }

    string getName() { return EVENT_NEW_TASK; }
    *string getDesc() { return "Fires when a new task is created in Insightly"; }
    string getEventName() { return EVENT_NEW_TASK; }
    string getIdField() { return "TASK_ID"; }

    AbstractDataProviderType getRecordDataType() {
        return new InsightlyTaskDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        return fetchRecordsUpdatedAfter("Tasks", since);
    }

    hash<auto> getFakeExampleData() {
        return {
            "TASK_ID": 100001,
            "TITLE": "Follow up with client",
            "DETAILS": "Confirm proposal acceptance and next steps",
            "STATUS": "Not Started",
            "PRIORITY": 2,
            "PERCENT_COMPLETE": 0,
            "DUE_DATE": "2026-05-15",
            "START_DATE": "2026-05-01",
            "REMINDER_DATE_UTC": "2026-05-15T09:00:00Z",
            "COMPLETED": False,
            "PUBLICLY_VISIBLE": True,
            "CATEGORY_ID": 1,
            "RESPONSIBLE_USER_ID": 12345,
            "OWNER_USER_ID": 12345,
            "DATE_CREATED_UTC": "2026-04-01T10:30:00Z",
            "DATE_UPDATED_UTC": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace InsightlyDataProvider
