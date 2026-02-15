# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceWatchNewProductDataProvider.qc watch provider for new BigCommerce products

/*  BigCommerceWatchNewProductDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Watch provider that triggers when a new product is created in BigCommerce
public class BigCommerceWatchNewProductDataProvider inherits BigCommerceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRODUCT_CREATED,
            "desc": "Triggers when a new product is created in BigCommerce",
            "type": "BigCommerceWatchNewProductDataProvider",
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
        return EVENT_PRODUCT_CREATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Triggers when a new product is created in BigCommerce";
    }

    #! Fetches new product records created since the given date
    private list<hash<auto>> fetchRecords(date since) {
        hash<auto> params = {
            "sort": "date_created",
            "direction": "desc",
            "date_modified:min": formatDateTimeV3(since),
            "limit": string(DefaultPageSize),
        };
        hash<auto> response = doGetV3("catalog/products", params);
        if (response.data.typeCode() == NT_LIST) {
            return response.data;
        }
        return ();
    }

    #! Returns the event name
    private string getEventName() {
        return EVENT_PRODUCT_CREATED;
    }

    #! Returns the record data type
    private AbstractDataProviderType getRecordDataType() {
        return new BigCommerceProductDataType();
    }

    #! Returns data provider static info
    private hash<DataProvider::DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns fake example data for documentation/testing
    private hash<auto> getFakeExampleData() {
        return {
            "id": 500,
            "name": "Example Product",
            "type": "physical",
            "sku": "EXAMPLE-001",
            "price": 29.99,
            "weight": 1.0,
            "is_visible": True,
            "availability": "available",
            "date_created": "2026-01-30T10:30:00+00:00",
            "date_modified": "2026-01-30T10:30:00+00:00",
        };
    }
}

} # namespace BigCommerceDataProvider
