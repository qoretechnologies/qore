# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetWatchRowDeletedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

public namespace SmartSheetDataProvider {

#! Smartsheet watch provider for row-deleted events
public class SmartSheetWatchRowDeletedDataProvider inherits SmartSheetWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ROW_DELETED,
            "desc": "Watches for row deletion events in Smartsheet",
            "type": "SmartSheetWatchRowDeletedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SmartSheetWatchDataProviderBase(rest, EVENT_ROW_DELETED, options) {
    }

    string getName() { return EVENT_ROW_DELETED; }
    *string getDesc() { return ProviderInfo.desc; }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_ROW_DELETED: <DataProviderMessageInfo>{
                "desc": "Fires when a row is deleted from a sheet",
                "type": new SmartSheetEventDataType(),
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        try {
            hash<auto> response = doGet("events", {
                "since": format_date("YYYY-MM-DDTHH:mm:ss.000Z", now_us() - 24h),
                "maxCount": 100,
            });
            if (response.data) {
                foreach hash<auto> event in (response.data) {
                    if (event.objectType == "ROW" && event.action == "DELETE") {
                        return event;
                    }
                }
            }
        } catch () {}
        return getFakeExampleData();
    }

    private hash<auto> getFakeExampleData() {
        return {
            "eventId": "2.1.0abcDEFghijKLMnopQRStuv",
            "objectType": "ROW",
            "action": "DELETE",
            "objectId": 1234567890,
            "eventTimestamp": format_date("YYYY-MM-DDTHH:mm:ssZ", now_us()),
            "userId": 1111111111,
            "requestUserId": 1111111111,
            "source": "WEB_APP",
            "additionalDetails": {
                "sheetId": 9876543210,
            },
        };
    }

    private bool matchesEvent(hash<auto> event) {
        return event.objectType == "ROW" && event.action == "DELETE"
            && SmartSheetWatchDataProviderBase::matchesEvent(event);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
