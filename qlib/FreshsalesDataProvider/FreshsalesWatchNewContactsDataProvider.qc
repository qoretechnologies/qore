# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesWatchNewContactsDataProvider.qc Watch for new Freshsales contacts

/*  FreshsalesWatchNewContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Freshsales watch new contacts data provider
public class FreshsalesWatchNewContactsDataProvider inherits FreshsalesWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CONTACT,
            "desc": "Watch for new contacts created in Freshsales by polling",
            "type": "FreshsalesWatchNewContactsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : FreshsalesWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_CONTACT;
    }

    *string getDesc() {
        return "Watch for new contacts created in Freshsales by polling";
    }

    string getEventName() {
        return EVENT_NEW_CONTACT;
    }

    string getTimestampField() {
        return "created_at";
    }

    AbstractDataProviderType getRecordDataType() {
        return new FreshsalesContactDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        # Get contacts from the first available view, filter by created_at
        try {
            hash<auto> filters = doGet("contacts/filters");
            *list<hash<auto>> filter_list = filters.filters;
            if (filter_list) {
                return fetchAllPages(
                    "contacts/view/" + string(filter_list[0].id), "contacts");
            }
        } catch () {
        }
        return ();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 100001,
            "first_name": "Jane",
            "last_name": "Smith",
            "email": "jane.smith@example.com",
            "mobile_number": "+1-555-0199",
            "work_number": "+1-555-0100",
            "job_title": "VP of Engineering",
            "address": "123 Main Street",
            "city": "San Francisco",
            "state": "CA",
            "zipcode": "94105",
            "country": "US",
            "created_at": "2026-04-01T10:30:00Z",
            "updated_at": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

}
