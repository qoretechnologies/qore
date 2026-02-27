# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelWatchTimesheetReviewedDataProvider.qc Deel watch for reviewed timesheets data provider

/*  DeelWatchTimesheetReviewedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for reviewed (approved/rejected) timesheets in Deel
public class DeelWatchTimesheetReviewedDataProvider inherits DeelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TIMESHEET_REVIEWED,
            "desc": "Watch for reviewed (approved/rejected) timesheets in Deel",
            "type": "DeelWatchTimesheetReviewedDataProvider",
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
        return EVENT_TIMESHEET_REVIEWED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for reviewed (approved/rejected) timesheets in Deel";
    }

    #! Fetches recently reviewed timesheets since the given date (uses dateFrom API filter)
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("timesheets", "timesheets",
            {"dateFrom": formatDate(since)});
    }

    #! Returns the timestamp from a record (use updated_at for reviews)
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updated_at;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_TIMESHEET_REVIEWED;
    }

    #! Returns the data type for timesheet event records
    private AbstractDataProviderType getRecordDataType() {
        return new DeelTimesheetDataType();
    }

    #! Returns fake example data for reviewed timesheets
    private hash<auto> getFakeExampleData() {
        return DeelWatchDataProviderBase::getFakeExampleData() + {
            "quantity": 8.0,
            "description": "Developed new features",
            "status": "approved",
            "date_submitted": "2026-02-24",
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
