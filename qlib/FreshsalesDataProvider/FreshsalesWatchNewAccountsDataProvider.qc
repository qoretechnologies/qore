# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file FreshsalesWatchNewAccountsDataProvider.qc Watch for new Freshsales accounts

/*  FreshsalesWatchNewAccountsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Freshsales watch new accounts data provider
public class FreshsalesWatchNewAccountsDataProvider inherits FreshsalesWatchDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_NEW_ACCOUNT,
            "desc": "Watch for new accounts created in Freshsales by polling",
            "type": "FreshsalesWatchNewAccountsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    constructor(RestClientIo::RestClientIo rest, *hash<auto> options)
            : FreshsalesWatchDataProviderBase(rest, options) {
    }

    string getName() {
        return EVENT_NEW_ACCOUNT;
    }

    *string getDesc() {
        return "Watch for new accounts created in Freshsales by polling";
    }

    string getEventName() {
        return EVENT_NEW_ACCOUNT;
    }

    string getTimestampField() {
        return "created_at";
    }

    AbstractDataProviderType getRecordDataType() {
        return new FreshsalesAccountDataType();
    }

    list<hash<auto>> fetchRecords(date since) {
        hash<auto> filters = doGet("sales_accounts/filters");
        *list<hash<auto>> filter_list = filters.filters;
        if (filter_list) {
            return fetchAllPages(
                "sales_accounts/view/" + string(filter_list[0].id), "sales_accounts");
        }
        return ();
    }

    hash<auto> getFakeExampleData() {
        return {
            "id": 300001,
            "name": "Acme Corporation",
            "address": "456 Enterprise Ave",
            "city": "New York",
            "state": "NY",
            "zipcode": "10001",
            "country": "US",
            "phone": "+1-555-0200",
            "website": "https://acme.example.com",
            "number_of_employees": 250,
            "annual_revenue": 5000000.00,
            "created_at": "2026-04-01T10:30:00Z",
            "updated_at": "2026-04-01T10:30:00Z",
        };
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

}
