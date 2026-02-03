# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationShipNotifyEventDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! ShipStation ship notify event data provider - watches for shipment events (when labels are generated)
public class ShipStationShipNotifyEventDataProvider inherits ShipStationBaseEventDataProvider {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "type": "ShipStationShipNotifyEventDataProvider",
            "constructor_options": ConstructorOptions,
            "supports_observable": True,
        };

        #! Constructor options
        const ConstructorOptions = ShipStationDataProviderBase::BaseConstructorOptions + LocalOptions;
    }

    private {
        #! Event data type
        static ShipStationShipNotifyEventDataType eventDataType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest, *hash<auto> options)
            : ShipStationBaseEventDataProvider(rest, options) {
    }

    #! Returns the data provider name
    string getName() {
        return EVENT_SHIP_NOTIFY;
    }

    #! Returns the data provider description
    *string getDesc() {
        return "ShipStation ship notify event data provider - triggers when a shipping label is generated";
    }

    #! Returns the event type
    string getEventType() {
        return EVENT_SHIP_NOTIFY;
    }

    #! Returns the ShipStation webhook event type
    string getWebhookEventType() {
        return WEBHOOK_SHIP_NOTIFY;
    }

    #! Returns the event data type
    AbstractDataProviderType getEventDataType() {
        return eventDataType;
    }

    #! Returns example event data - tries real API data first, falls back to fake data
    private hash<auto> getExampleEventData() {
        # Try to fetch real shipment data from the API
        if (*hash<auto> shipment = tryFetchRealShipment()) {
            return mapShipmentToEventData(shipment, EVENT_SHIP_NOTIFY, WEBHOOK_SHIP_NOTIFY);
        }
        # Fall back to fake example data
        return getFakeShipmentExampleData();
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
        };
    }

    #! Returns data provider static info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
