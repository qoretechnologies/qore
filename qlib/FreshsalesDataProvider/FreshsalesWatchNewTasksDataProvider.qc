# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesWatchNewTasksDataProvider.qc Watch for new Freshsales tasks

/*  FreshsalesWatchNewTasksDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

public namespace FreshsalesDataProvider {

#! Freshsales watch new tasks data provider
public class FreshsalesWatchNewTasksDataProvider inherits FreshsalesWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_TASK,
            "desc": "Watch for new tasks created in Freshsales by polling",
            "type": "FreshsalesWatchNewTasksDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : FreshsalesWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_TASK;
    }

    *string getDesc() {
        return "Watch for new tasks created in Freshsales by polling";
    }

    string getEventName() {
        return EVENT_NEW_TASK;
    }

    string getTimestampField() {
        return "created_at";
    }

    AbstractDataProviderType getRecordDataType() {
        return new FreshsalesTaskDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        try {
            return fetchAllPages("tasks", "tasks", {"filter": "open"});
        } catch () {
        }
        return ();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 400001,
            "title": "Follow up with prospect",
            "description": "Schedule a demo call with the prospect",
            "due_date": "2026-04-15",
            "status": 0,
            "owner_id": 1,
            "targetable_id": 100001,
            "targetable_type": "Contact",
            "created_at": "2026-04-01T10:30:00Z",
            "updated_at": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

}
