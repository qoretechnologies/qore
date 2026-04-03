# -*- mode: qore; indent-tabs-mode: nil -*-

/*  SmartSheetEventsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Smartsheet events container data provider
public class SmartSheetEventsDataProvider inherits SmartSheetDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "events",
            "desc": "Smartsheet events container for polling-based triggers that watch for "
                "changes to sheets, rows, and other objects",
            "type": "SmartSheetEventsDataProvider",
            "supports_children": True,
            "children_can_support_observers": True,
        };

        const ChildMap = {
            EVENT_SHEET_UPDATED: Class::forName("SmartSheetWatchSheetUpdatedDataProvider"),
            EVENT_ROW_CREATED: Class::forName("SmartSheetWatchRowCreatedDataProvider"),
            EVENT_ROW_UPDATED: Class::forName("SmartSheetWatchRowUpdatedDataProvider"),
            EVENT_ROW_DELETED: Class::forName("SmartSheetWatchRowDeletedDataProvider"),
        };
    }

    constructor(RestClientIo::RestClientIo rest) : SmartSheetDataProviderBase(rest) {
    }

    string getName() { return "events"; }
    *string getDesc() { return ProviderInfo.desc; }

    private *list<string> getChildProviderNamesImpl() {
        return keys ChildMap;
    }

    private *AbstractDataProvider getChildProviderImpl(string name) {
        *Class cls = ChildMap{name};
        if (cls) {
            return cls.newObject(rest);
        }
    }

    private hash<DataProviderInfo> getStaticInfoImpl() { return ProviderInfo; }
}

} # namespace SmartSheetDataProvider
