# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesWatchNewAppointmentsDataProvider.qc Watch for new Freshsales appointments

/*  FreshsalesWatchNewAppointmentsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Freshsales watch new appointments data provider
public class FreshsalesWatchNewAppointmentsDataProvider inherits FreshsalesWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_APPOINTMENT,
            "desc": "Watch for new appointments created in Freshsales by polling",
            "type": "FreshsalesWatchNewAppointmentsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : FreshsalesWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_APPOINTMENT;
    }

    *string getDesc() {
        return "Watch for new appointments created in Freshsales by polling";
    }

    string getEventName() {
        return EVENT_NEW_APPOINTMENT;
    }

    string getTimestampField() {
        return "created_at";
    }

    AbstractDataProviderType getRecordDataType() {
        return new FreshsalesAppointmentDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("appointments", "appointments", {"filter": "upcoming"});
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 500001,
            "title": "Product Demo",
            "description": "Demo of new features for the client",
            "from_date": "2026-04-15T14:00:00Z",
            "end_date": "2026-04-15T15:00:00Z",
            "time_zone": "America/New_York",
            "location": "Conference Room A",
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
