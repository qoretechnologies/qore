# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceWatchOrdersDataProvider.qc Squarespace watch orders event data provider

/*  SquarespaceWatchOrdersDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Squarespace watch orders event data provider
public class SquarespaceWatchOrdersDataProvider inherits SquarespaceWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": EVENT_ORDER_UPDATED,
            "desc": "Squarespace order updated event provider - polls for new or updated orders",
            "type": "SquarespaceWatchOrdersDataProvider",
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
        return EVENT_ORDER_UPDATED;
    }

    #! Returns the provider description
    *string getDesc() {
        return "Squarespace order updated event provider - polls for new or updated orders";
    }

    #! Returns the event name for notifications
    private string getEventName() {
        return EVENT_ORDER_UPDATED;
    }

    #! Returns the data type for event records
    private AbstractDataProviderType getRecordDataType() {
        return new SquarespaceOrderDataType();
    }

    #! Fetches records modified since the given date
    /** @param since the date to fetch records modified after

        @note The Squarespace orders API does not allow combining cursor with modifiedAfter.
        fetchAllPages() handles this correctly: the first request uses modifiedAfter,
        subsequent requests use only the cursor parameter.
    */
    private list<hash<auto>> fetchRecords(date since) {
        return fetchAllPages("1.0/commerce/orders", "result",
            {"modifiedAfter": formatDateTime(since)});
    }

    #! Returns fake example data for orders
    private hash<auto> getFakeExampleData() {
        return SquarespaceWatchDataProviderBase::getFakeExampleData() + {
            "orderNumber": "100001",
            "channel": "web",
            "channelName": "My Store",
            "customerEmail": "customer@example.com",
            "fulfillmentStatus": "PENDING",
            "testmode": False,
            "billingAddress": {
                "firstName": "John",
                "lastName": "Doe",
                "address1": "123 Main St",
                "city": "New York",
                "state": "NY",
                "countryCode": "US",
                "postalCode": "10001",
            },
            "shippingAddress": {
                "firstName": "John",
                "lastName": "Doe",
                "address1": "123 Main St",
                "city": "New York",
                "state": "NY",
                "countryCode": "US",
                "postalCode": "10001",
            },
            "lineItems": ({
                "id": "EXAMPLE_LINE_ITEM_1",
                "quantity": 1,
                "unitPricePaid": {"currency": "USD", "value": "29.99"},
            },),
            "subtotal": {"currency": "USD", "value": "29.99"},
            "shippingTotal": {"currency": "USD", "value": "5.99"},
            "taxTotal": {"currency": "USD", "value": "2.85"},
            "grandTotal": {"currency": "USD", "value": "38.83"},
            "priceTaxInterpretation": "EXCLUSIVE",
        };
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
