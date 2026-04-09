# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SystemeIoWatchContactsDataProvider.qc Watch for new Systeme.io contacts

/*  SystemeIoWatchContactsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SystemeIoDataProvider {

#! Systeme.io watch new contacts data provider
public class SystemeIoWatchContactsDataProvider inherits SystemeIoWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_CONTACT,
            "desc": "Watch for new contacts created in Systeme.io by polling",
            "type": "SystemeIoWatchContactsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SystemeIoWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_CONTACT;
    }

    *string getDesc() {
        return "Watch for new contacts created in Systeme.io by polling";
    }

    string getEventName() {
        return EVENT_NEW_CONTACT;
    }

    AbstractDataProviderType getRecordDataType() {
        return new SystemeIoContactDataType();
    }

    string getTimestampField() {
        return "registeredAt";
    }

    list<hash<auto>> fetchRecords(date since) {
        return doPaginatedGet("contacts", {
            "order": "desc",
            "registeredAfter": format_date("YYYY-MM-DDTHH:mm:ssZ", since),
        });
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 100001,
            "email": "jane.smith@example.com",
            "locale": "en",
            "registeredAt": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

}
