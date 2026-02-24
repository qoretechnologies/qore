# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchNewCartDataProvider.qc watch provider for new carts in BigCommerce

/*  BigCommerceWatchNewCartDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider that triggers when a new cart/checkout session is created in BigCommerce
/** Since BigCommerce has no list-carts endpoint, this provider polls V2 orders with
    status_id=0 (Incomplete) and min_date_created as a proxy for new carts/checkout
    sessions.
*/
public class BigCommerceWatchNewCartDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_CART_CREATED,
            "desc": "Triggers when a new cart/checkout session is created in BigCommerce",
            "type": "BigCommerceWatchNewCartDataProvider",
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
        return EVENT_CART_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a new cart/checkout session is created in BigCommerce";
    }

    #! Fetches new incomplete orders as a proxy for new carts
    /** Polls V2 orders with status_id=0 (Incomplete) and min_date_created since the
        last check time
    */
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "status_id": "0",
            "min_date_created": formatDateTimeV2(since),
            "sort": "date_created:desc",
            "limit": string(DefaultPageSize),
        };
        auto response = doGetV2("orders", params);
        # V2 returns a flat array directly
        if (response.typeCode() == NT_LIST) {
            return response;
        }
        return ();
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_CART_CREATED;
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
            "id": 300,
            "status": "Incomplete",
            "status_id": 0,
            "total_inc_tax": "75.50",
            "total_ex_tax": "68.00",
            "currency_code": "USD",
            "date_created": "Thu, 30 Jan 2026 10:30:00 +0000",
            "date_modified": "Thu, 30 Jan 2026 10:30:00 +0000",
        };
    }
}

} # namespace BigCommerceDataProvider
