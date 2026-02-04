# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoNewOrderWatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShippoDataProvider module
public namespace ShippoDataProvider {

#! Shippo new order watch data provider - detects new orders
public class ShippoNewOrderWatchDataProvider inherits ShippoWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-order",
            "desc": "Detects new orders in Shippo via polling",
            "type": "ShippoNewOrderWatchDataProvider",
            "supports_observable": True,
            "constructor_options": WatchConstructorOptions,
        };
    }

    #! Creates the object with a REST client
    constructor(ShippoRestClient::ShippoRestClient rest, *hash<auto> options)
            : ShippoWatchDataProviderBase(rest, options) {
    }

    #! Creates the object with a connection
    constructor(ShippoRestClient::ShippoRestConnection conn, *hash<auto> options)
            : ShippoWatchDataProviderBase(conn, options) {
    }

    #! Returns the provider name
    string getName() {
        return "new-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Detects new orders in Shippo via polling. Triggers when a new order is created.";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the API path for this resource
    private string getResourcePath() {
        return "/orders";
    }

    #! Returns the field name for the record ID
    private string getIdField() {
        return "object_id";
    }

    #! Returns the event name for notifications
    private string getEventName() {
        return EVENT_NEW_ORDER;
    }

    #! Returns a human-readable description of the resource
    private string getResourceDescription() {
        return "order";
    }

    #! Returns the data type for event data
    private AbstractDataProviderType getEventDataType() {
        return new ShippoOrderDataType();
    }

    #! Returns fake example data when no real records are available
    private hash<auto> getFakeExampleData() {
        return {
            "object_id": "d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a3",
            "object_created": "2026-01-30T10:30:00Z",
            "object_updated": "2026-01-30T10:30:00Z",
            "object_owner": "orders@example.com",
            "order_number": "ORDER-12345",
            "order_status": "PAID",
            "placed_at": "2026-01-30T10:00:00Z",
            "to_address": {
                "name": "Jane Customer",
                "street1": "456 Customer Lane",
                "city": "Los Angeles",
                "state": "CA",
                "zip": "90001",
                "country": "US",
            },
            "line_items": ({
                "title": "Example Product",
                "quantity": 2,
                "sku": "SKU-001",
                "total_price": "59.98",
                "currency": "USD",
            },),
            "total_price": "59.98",
            "currency": "USD",
            "weight": "2.5",
            "weight_unit": "lb",
        };
    }
}

} # namespace ShippoDataProvider
