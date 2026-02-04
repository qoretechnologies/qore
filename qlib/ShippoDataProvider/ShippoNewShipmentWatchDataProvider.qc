# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoNewShipmentWatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo new shipment watch data provider - detects new shipments
public class ShippoNewShipmentWatchDataProvider inherits ShippoWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-shipment",
            "desc": "Detects new shipments in Shippo via polling",
            "type": "ShippoNewShipmentWatchDataProvider",
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
        return "new-shipment";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Detects new shipments in Shippo via polling. Triggers when a new shipment is created.";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the API path for this resource
    private string getResourcePath() {
        return "/shipments";
    }

    #! Returns the field name for the record ID
    private string getIdField() {
        return "object_id";
    }

    #! Returns the event name for notifications
    private string getEventName() {
        return EVENT_NEW_SHIPMENT;
    }

    #! Returns a human-readable description of the resource
    private string getResourceDescription() {
        return "shipment";
    }

    #! Returns the data type for event data
    private AbstractDataProviderType getEventDataType() {
        return new ShippoShipmentDataType();
    }

    #! Returns fake example data when no real records are available
    private hash<auto> getFakeExampleData() {
        return {
            "object_id": "5e5de50f3f1a4e28a6b91e9a4c9e2b8c",
            "object_created": "2026-01-30T10:30:00Z",
            "object_updated": "2026-01-30T10:30:00Z",
            "object_owner": "shipping@example.com",
            "status": "SUCCESS",
            "address_from": {
                "name": "John Sender",
                "street1": "123 Sender St",
                "city": "San Francisco",
                "state": "CA",
                "zip": "94103",
                "country": "US",
            },
            "address_to": {
                "name": "Jane Recipient",
                "street1": "456 Recipient Ave",
                "city": "New York",
                "state": "NY",
                "zip": "10001",
                "country": "US",
            },
            "parcels": ({
                "length": "10",
                "width": "8",
                "height": "6",
                "distance_unit": "in",
                "weight": "1.5",
                "mass_unit": "lb",
            },),
            "shipment_date": "2026-01-30T10:30:00Z",
            "metadata": "",
            "rates": (),
        };
    }
}

} # namespace ShippoDataProvider
