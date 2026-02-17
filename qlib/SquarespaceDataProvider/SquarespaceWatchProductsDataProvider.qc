# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceWatchProductsDataProvider.qc Squarespace watch products event data provider

/*  SquarespaceWatchProductsDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the SquarespaceDataProvider module
public namespace SquarespaceDataProvider {

#! Squarespace watch products event data provider
public class SquarespaceWatchProductsDataProvider inherits SquarespaceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_PRODUCT_UPDATED,
            "desc": "Squarespace product updated event provider - polls for new or updated products",
            "type": "SquarespaceWatchProductsDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(SquarespaceRestClient::SquarespaceRestClient rest, *hash<auto> options)
            : SquarespaceWatchDataProviderBase(rest, options) {
    }

    #! Returns the provider name
    string getName() {
        return EVENT_PRODUCT_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace product updated event provider - polls for new or updated products";
    }

    #! Returns the event name for notifications
    private string getEventName() {
        return EVENT_PRODUCT_UPDATED;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() {
        return new SquarespaceProductDataType();
    }

    #! Fetches records modified since the given date
    /** @param since the date to fetch records modified after
    */
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("v2/commerce/products", "products",
            {"modifiedAfter": formatDateTime(since)});
    }

    #! Returns fake example data for products
    private hash<auto> getFakeExampleData() {
        return SquarespaceWatchDataProviderBase::getFakeExampleData() + {
            "type": "PHYSICAL",
            "storePageId": "EXAMPLE_STORE_PAGE_1",
            "name": "Example Product",
            "description": "<p>A sample product description</p>",
            "url": "/store/example-product",
            "urlSlug": "example-product",
            "tags": ("example", "sample"),
            "isVisible": True,
            "variants": ({
                "id": "EXAMPLE_VARIANT_1",
                "sku": "EX-001",
                "pricing": {"basePrice": {"currency": "USD", "value": "29.99"}},
                "stock": {"quantity": 100, "unlimited": False},
            },),
            "images": (),
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
