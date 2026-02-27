# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelWatchTimesheetCreatedDataProvider.qc Deel watch for new timesheets data provider

/*  DeelWatchTimesheetCreatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the DeelDataProvider module
public namespace DeelDataProvider {

#! Watch for new (submitted) timesheets in Deel
public class DeelWatchTimesheetCreatedDataProvider inherits DeelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TIMESHEET_CREATED,
            "desc": "Watch for newly submitted timesheets in Deel",
            "type": "DeelWatchTimesheetCreatedDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(DeelRestClient::DeelRestClient rest, *hash<auto> options)
            : DeelWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_TIMESHEET_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for newly submitted timesheets in Deel";
    }

    #! Fetches new timesheets since the given date (uses dateFrom API filter)
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("timesheets", "timesheets",
            {"dateFrom": formatDate(since)});
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_TIMESHEET_CREATED;
    }

    #! Returns the data type for timesheet event records
    private AbstractDataProviderType getRecordDataType() {
        return new DeelTimesheetDataType();
    }

    #! Returns fake example data for timesheets
    private hash<auto> getFakeExampleData() {
        return DeelWatchDataProviderBase::getFakeExampleData() + {
            "quantity": 8.0,
            "description": "Developed new features",
            "status": "pending",
            "date_submitted": "2026-02-24",
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
