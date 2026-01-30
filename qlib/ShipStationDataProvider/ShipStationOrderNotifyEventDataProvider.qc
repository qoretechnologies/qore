# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationOrderNotifyEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ShipStationDataProvider module
public namespace ShipStationDataProvider {

#! ShipStation order notify event data provider - watches for new/updated order events
public class ShipStationOrderNotifyEventDataProvider inherits ShipStationBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ShipStationOrderNotifyEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = ShipStationDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        #! Event data type
        static ShipStationOrderNotifyEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest, *hash<auto> options)
            : ShipStationBaseEventDataProvider(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_ORDER_NOTIFY;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "ShipStation order notify event data provider - triggers when an order is created or updated";
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_ORDER_NOTIFY;
    }

    #! Returns the ShipStation webhook event type
    string getWebhookEventType() {
        return WEBHOOK_ORDER_NOTIFY;
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns example event data
    private hash<auto> getExampleEventData() {
        return {
            "event": EVENT_ORDER_NOTIFY,
            "resource_url": "https://ssapi.shipstation.com/orders?orderNumber=TEST-001",
            "resource_type": WEBHOOK_ORDER_NOTIFY,
            "orderId": 12345,
            "orderNumber": "TEST-001",
            "orderKey": "unique-key-001",
            "orderDate": "2026-01-15T10:30:00.000Z",
            "orderStatus": "awaiting_shipment",
            "customerEmail": "customer@example.com",
            "shipTo": {
                "name": "John Doe",
                "street1": "123 Main St",
                "city": "Austin",
                "state": "TX",
                "postalCode": "78701",
                "country": "US",
            },
            "orderTotal": 99.99,
            "storeId": 12345,
        };
    }

    #! Returns event-specific fields from the fetched order data
    private hash<auto> getEventSpecificFields(hash<auto> event_data) {
        return {
            "orderId": event_data.orderId,
            "orderNumber": event_data.orderNumber,
            "orderKey": event_data.orderKey,
            "orderDate": event_data.orderDate,
            "orderStatus": event_data.orderStatus,
            "customerEmail": event_data.customerEmail,
            "shipTo": event_data.shipTo,
            "billTo": event_data.billTo,
            "orderTotal": event_data.orderTotal,
            "storeId": event_data.storeId,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
