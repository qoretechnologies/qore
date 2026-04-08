# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetWatchSheetUpdatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Smartsheet watch provider for sheet-updated events
public class SmartSheetWatchSheetUpdatedDataProvider inherits SmartSheetWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_SHEET_UPDATED,
            "desc": "Watches for sheet update events in the Smartsheet account",
            "type": "SmartSheetWatchSheetUpdatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SmartSheetWatchDataProviderBase(rest, EVENT_SHEET_UPDATED, options) {
    }

    string getName() { return EVENT_SHEET_UPDATED; }
    *string getDesc() { return ProviderInfo.desc; }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_SHEET_UPDATED: <DataProviderMessageInfo>{
                "desc": "Fires when a sheet is updated",
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
                    if (event.objectType == "SHEET" && event.action == "UPDATE") {
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
            "objectType": "SHEET",
            "action": "UPDATE",
            "objectId": 1234567890,
            "eventTimestamp": format_date("YYYY-MM-DDTHH:mm:ssZ", now_us()),
            "userId": 9876543210,
            "requestUserId": 9876543210,
            "source": "WEB_APP",
            "additionalDetails": {},
        };
    }

    private bool matchesEvent(hash<auto> event) {
        return event.objectType == "SHEET" && event.action == "UPDATE"
            && SmartSheetWatchDataProviderBase::matchesEvent(event);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
