# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SystemeIoWatchTagsDataProvider.qc Watch for new Systeme.io tags

/*  SystemeIoWatchTagsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Systeme.io watch new tags data provider
public class SystemeIoWatchTagsDataProvider inherits SystemeIoWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_TAG,
            "desc": "Watch for new tags created in Systeme.io by polling",
            "type": "SystemeIoWatchTagsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SystemeIoWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_TAG;
    }

    *string getDesc() {
        return "Watch for new tags created in Systeme.io by polling";
    }

    string getEventName() {
        return EVENT_NEW_TAG;
    }

    AbstractDataProviderType getRecordDataType() {
        return new SystemeIoTagDataType();
    }

    string getTimestampField() {
        return "createdAt";
    }

    list<hash<auto>> fetchRecords(date since) {
        # Tags API has no date filter; fetch one page sorted newest-first
        # and let the base class filter by createdAt client-side
        return doPaginatedGet("tags", {"order": "desc"}, DefaultPageSize);
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 10001,
            "name": "VIP Customer",
            "createdAt": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

}
