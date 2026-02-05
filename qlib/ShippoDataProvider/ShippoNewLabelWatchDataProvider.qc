# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShippoDataProvider module definition

/*  ShippoNewLabelWatchDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Shippo new label watch data provider - detects new shipping labels (transactions)
public class ShippoNewLabelWatchDataProvider inherits ShippoWatchDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "new-label",
            "desc": "Detects new shipping labels (transactions) in Shippo via polling",
            "type": "ShippoNewLabelWatchDataProvider",
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
        return "new-label";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Detects new shipping labels (transactions) in Shippo via polling. "
            "Triggers when a new shipping label is created.";
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }

    #! Returns the API path for this resource
    private string getResourcePath() {
        return "/transactions";
    }

    #! Returns the field name for the record ID
    private string getIdField() {
        return "object_id";
    }

    #! Returns the event name for notifications
    private string getEventName() {
        return EVENT_NEW_LABEL;
    }

    #! Returns a human-readable description of the resource
    private string getResourceDescription() {
        return "shipping label";
    }

    #! Returns the data type for event data
    private AbstractDataProviderType getEventDataType() {
        return new ShippoTransactionDataType();
    }

    #! Returns fake example data when no real records are available
    private hash<auto> getFakeExampleData() {
        return {
            "object_id": "c1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a6",
            "object_created": "2026-01-30T10:30:00Z",
            "object_updated": "2026-01-30T10:30:00Z",
            "object_owner": "shipping@example.com",
            "object_status": "SUCCESS",
            "rate": "7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2a",
            "tracking_number": "9400111899223456789123",
            "tracking_status": "UNKNOWN",
            "tracking_url_provider": "https://tracking.usps.com/tracking/9400111899223456789123",
            "label_url": "https://shippo-delivery.s3.amazonaws.com/example-label.pdf",
            "commercial_invoice_url": NOTHING,
            "eta": NOTHING,
            "metadata": "",
            "messages": (),
        };
    }
}

} # namespace ShippoDataProvider
