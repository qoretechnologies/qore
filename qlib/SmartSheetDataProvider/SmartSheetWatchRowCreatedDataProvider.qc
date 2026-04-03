# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetWatchRowCreatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Smartsheet watch provider for row-created events
public class SmartSheetWatchRowCreatedDataProvider inherits SmartSheetWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ROW_CREATED,
            "desc": "Watches for new row creation events in Smartsheet",
            "type": "SmartSheetWatchRowCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : SmartSheetWatchDataProviderBase(rest, EVENT_ROW_CREATED, options) {
    }

    string getName() { return EVENT_ROW_CREATED; }
    *string getDesc() { return ProviderInfo.desc; }

    private hash<string, hash<DataProviderMessageInfo>> getEventTypesImpl() {
        return {
            EVENT_ROW_CREATED: <DataProviderMessageInfo>{
                "desc": "Fires when a new row is created in a sheet",
                "type": AbstractDataProviderType::get(AutoHashType),
            },
        };
    }

    private auto getExampleEventDataImpl(string event_id) {
        return {
            "objectType": "ROW",
            "action": "CREATE",
            "id": 1234567890,
            "additionalDetails": {"sheetId": 9876543210},
            "userId": 1111111111,
            "timestamp": now_us().format("YYYY-MM-DDTHH:mm:SSZ"),
        };
    }

    private bool matchesEvent(hash<auto> event) {
        return event.objectType == "ROW" && event.action == "CREATE";
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
