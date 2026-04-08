# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesWatchNewDealsDataProvider.qc Watch for new Freshsales deals

/*  FreshsalesWatchNewDealsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Freshsales watch new deals data provider
public class FreshsalesWatchNewDealsDataProvider inherits FreshsalesWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_DEAL,
            "desc": "Watch for new deals created in Freshsales by polling",
            "type": "FreshsalesWatchNewDealsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : FreshsalesWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_DEAL;
    }

    *string getDesc() {
        return "Watch for new deals created in Freshsales by polling";
    }

    string getEventName() {
        return EVENT_NEW_DEAL;
    }

    string getTimestampField() {
        return "created_at";
    }

    AbstractDataProviderType getRecordDataType() {
        return new FreshsalesDealDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> filters = doGet("deals/filters");
        *list<hash<auto>> filter_list = filters.filters;
        if (filter_list) {
            return fetchAllPages(
                "deals/view/" + string(filter_list[0].id), "deals");
        }
        return ();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 200001,
            "name": "Enterprise Software License",
            "amount": 50000.00,
            "sales_account_id": 300001,
            "deal_stage_id": 1,
            "probability": 60,
            "expected_close": "2026-06-30",
            "created_at": "2026-04-01T10:30:00Z",
            "updated_at": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

}
