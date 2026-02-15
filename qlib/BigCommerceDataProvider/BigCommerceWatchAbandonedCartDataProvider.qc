# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchAbandonedCartDataProvider.qc watch provider for abandoned carts in BigCommerce

/*  BigCommerceWatchAbandonedCartDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Watch provider that triggers when an abandoned cart is detected in BigCommerce
/** Uses the V2 Orders API to poll for orders with status_id=0 (Incomplete) which
    serves as a proxy for abandoned carts/checkout sessions that were not completed.
*/
public class BigCommerceWatchAbandonedCartDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CART_ABANDONED,
            "desc": "Triggers when an abandoned cart is detected in BigCommerce",
            "type": "BigCommerceWatchAbandonedCartDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest, *hash<auto> options)
            : BigCommerceWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_CART_ABANDONED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when an abandoned cart is detected in BigCommerce";
    }

    #! Fetches incomplete orders as a proxy for abandoned carts
    /** Polls V2 orders with status_id=0 (Incomplete) and min_date_modified since the
        last check time
    */
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "status_id": "0",
            "min_date_modified": formatDateTimeV2(since),
            "sort": "date_modified:desc",
            "limit": string(DefaultPageSize),
        };
        auto response = doGetV2("orders", params);
        # V2 returns a flat array directly
        if (response.typeCode() == NT_LIST) {
            return response;
        }
        return ();
    }

    #! Returns the timestamp from a record, preferring date_modified
    private *date getRecordTimestamp(hash<auto> record) {
        *string ts = record.date_modified ?? record.date_created;
        if (ts) {
            return date(ts);
        }
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_CART_ABANDONED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return new BigCommerceOrderDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns fake example data for documentation/testing
    private hash<auto> getFakeExampleData() {
        return {
            "id": 200,
            "status": "Incomplete",
            "status_id": 0,
            "total_inc_tax": "49.99",
            "total_ex_tax": "45.00",
            "currency_code": "USD",
            "date_created": "Thu, 30 Jan 2026 10:30:00 +0000",
            "date_modified": "Thu, 30 Jan 2026 11:00:00 +0000",
        };
    }
}

} # namespace BigCommerceDataProvider
