# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationItemShipNotifyEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation item ship notify event data provider - watches for shipment events with item details
public class ShipStationItemShipNotifyEventDataProvider inherits ShipStationBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ShipStationItemShipNotifyEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = ShipStationDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        #! Event data type
        static ShipStationItemShipNotifyEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest, *hash<auto> options)
            : ShipStationBaseEventDataProvider(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_ITEM_SHIP_NOTIFY;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "ShipStation item ship notify event data provider - triggers when a shipping label is generated, "
            "includes full item details";
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_ITEM_SHIP_NOTIFY;
    }

    #! Returns the ShipStation webhook event type
    string getWebhookEventType() {
        return WEBHOOK_ITEM_SHIP_NOTIFY;
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns example event data
    private hash<auto> getExampleEventData() {
        return {
            "event": EVENT_ITEM_SHIP_NOTIFY,
            "resource_url": "https://ssapi.shipstation.com/shipments?shipmentId=12345",
            "resource_type": WEBHOOK_ITEM_SHIP_NOTIFY,
            "shipmentId": 12345,
            "orderId": 54321,
            "orderNumber": "TEST-001",
            "trackingNumber": "1Z999AA10123456784",
            "carrierCode": "ups",
            "serviceCode": "ups_ground",
            "shipDate": "2026-01-15",
            "createDate": "2026-01-15T10:30:00.000Z",
            "shipmentCost": 12.50,
            "insuranceCost": 0.00,
            "voided": False,
            "shipTo": {
                "name": "John Doe",
                "street1": "123 Main St",
                "city": "Austin",
                "state": "TX",
                "postalCode": "78701",
                "country": "US",
            },
            "weight": {
                "value": 2.5,
                "units": "pounds",
            },
            "dimensions": {
                "length": 10,
                "width": 8,
                "height": 4,
                "units": "inches",
            },
            "shipmentItems": (
                {
                    "orderItemId": 67890,
                    "lineItemKey": "item-001",
                    "sku": "WIDGET-001",
                    "name": "Widget",
                    "quantity": 2,
                },
            ),
        };
    }

    #! Returns event-specific fields from the fetched shipment data
    private hash<auto> getEventSpecificFields(hash<auto> event_data) {
        return {
            "shipmentId": event_data.shipmentId,
            "orderId": event_data.orderId,
            "orderNumber": event_data.orderNumber,
            "trackingNumber": event_data.trackingNumber,
            "carrierCode": event_data.carrierCode,
            "serviceCode": event_data.serviceCode,
            "shipDate": event_data.shipDate,
            "createDate": event_data.createDate,
            "shipmentCost": event_data.shipmentCost,
            "insuranceCost": event_data.insuranceCost,
            "voided": event_data.voided,
            "shipTo": event_data.shipTo,
            "weight": event_data.weight,
            "dimensions": event_data.dimensions,
            "shipmentItems": event_data.shipmentItems,
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
