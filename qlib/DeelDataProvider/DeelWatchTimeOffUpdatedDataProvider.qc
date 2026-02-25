# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file DeelWatchTimeOffUpdatedDataProvider.qc Deel watch for updated time-off requests data provider

/*  DeelWatchTimeOffUpdatedDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Watch for updated time-off requests in Deel
public class DeelWatchTimeOffUpdatedDataProvider inherits DeelWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_TIME_OFF_UPDATED,
            "desc": "Watch for updated time-off requests in Deel",
            "type": "DeelWatchTimeOffUpdatedDataProvider",
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
        return EVENT_TIME_OFF_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Watch for updated time-off requests in Deel";
    }

    #! Fetches recently updated time-off requests with full cursor-based pagination
    private list<hash<auto>> fetchRecords(date since) {
        # time_offs uses pageSize and cursor-based pagination with has_next_page/next
        list<hash<auto>> all_records = ();
        hash<auto> params = {"pageSize": DefaultPageSize};
        while (True) {
            hash<auto> response = doGet("time_offs", params);
            list<auto> page_data = response{"data"} ?? ();
            if (!page_data) {
                break;
            }
            all_records += page_data;
            if (!response{"has_next_page"}) {
                break;
            }
            *string next_cursor = response{"next"};
            if (!next_cursor) {
                break;
            }
            params{"next"} = next_cursor;
        }
        return all_records;
    }

    #! Returns the timestamp from a record (use updated_at for updates)
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.updated_at ?? record.created_at;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_TIME_OFF_UPDATED;
    }

    #! Returns the data type for time-off request event records
    private AbstractDataProviderType getRecordDataType() {
        return new DeelTimeOffRequestDataType();
    }

    #! Returns fake example data for updated time-off requests
    private hash<auto> getFakeExampleData() {
        return DeelWatchDataProviderBase::getFakeExampleData() + {
            "start_date": "2026-04-01",
            "end_date": "2026-04-05",
            "status": "pending",
            "notes": "Changed dates for annual leave",
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace DeelDataProvider
