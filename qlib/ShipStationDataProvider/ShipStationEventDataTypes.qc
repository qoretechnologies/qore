# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationEventDataTypes.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Base event data type with common webhook fields
public class ShipStationBaseEventDataType inherits HashDataType {
    public {
        #! Common field definitions for all ShipStation webhook events
        const CommonFields = {
            "event": {
                "type": StringType,
                "display_name": "Event Type",
                "short_desc": "The type of event",
                "desc": "The type of event that occurred (`ORDER_NOTIFY`, `ITEM_ORDER_NOTIFY`, `SHIP_NOTIFY`, `ITEM_SHIP_NOTIFY`)",
            },
            "resource_url": {
                "type": StringOrNothingType,
                "display_name": "Resource URL",
                "short_desc": "URL to fetch full data",
                "desc": "The URL to fetch the full resource data from ShipStation",
            },
            "resource_type": {
                "type": StringOrNothingType,
                "display_name": "Resource Type",
                "short_desc": "Type of resource",
                "desc": "The type of resource that triggered the event",
            },
        };
    }

    #! Creates the type with common fields
    constructor() {
        addQoreFields(CommonFields);
    }
}

#! Order notify event data type - includes full order data fetched from resource_url
public class ShipStationOrderNotifyEventDataType inherits ShipStationBaseEventDataType {
    public {
        #! Additional fields for order notify events
        const OrderFields = {
            "orderId": {
                "type": IntOrNothingType,
                "display_name": "Order ID",
                "short_desc": "ShipStation order ID",
                "desc": "The unique ShipStation order ID",
            },
            "orderNumber": {
                "type": StringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Order number",
                "desc": "The order number (external reference)",
            },
            "orderKey": {
                "type": StringOrNothingType,
                "display_name": "Order Key",
                "short_desc": "Order key",
                "desc": "A unique identifier for the order (for external systems)",
            },
            "orderDate": {
                "type": DateOrNothingType,
                "display_name": "Order Date",
                "short_desc": "Order creation date",
                "desc": "The date the order was created",
            },
            "orderStatus": {
                "type": StringOrNothingType,
                "display_name": "Order Status",
                "short_desc": "Order status",
                "desc": "The current status of the order",
            },
            "customerEmail": {
                "type": StringOrNothingType,
                "display_name": "Customer Email",
                "short_desc": "Customer email address",
                "desc": "The customer's email address",
            },
            "shipTo": {
                "type": new ShipStationAddressDataType(AutoHashOrNothingType),
                "display_name": "Ship To",
                "short_desc": "Shipping address",
                "desc": "The shipping address for the order",
            },
            "billTo": {
                "type": new ShipStationAddressDataType(AutoHashOrNothingType),
                "display_name": "Bill To",
                "short_desc": "Billing address",
                "desc": "The billing address for the order",
            },
            "orderTotal": {
                "type": SoftFloatOrNothingType,
                "display_name": "Order Total",
                "short_desc": "Order total",
                "desc": "The total amount of the order",
            },
            "storeId": {
                "type": IntOrNothingType,
                "display_name": "Store ID",
                "short_desc": "Store ID",
                "desc": "The ShipStation store ID the order belongs to",
            },
        };
    }

    #! Creates the type
    constructor() : ShipStationBaseEventDataType() {
        addQoreFields(OrderFields);
    }
}

#! Item order notify event data type - includes order with item details
public class ShipStationItemOrderNotifyEventDataType inherits ShipStationOrderNotifyEventDataType {
    public {
        #! Additional fields for item order notify events
        const ItemFields = {
            "items": {
                "type": new ListDataType("OrderItems", new ShipStationOrderItemDataType(), True),
                "display_name": "Order Items",
                "short_desc": "Items in the order",
                "desc": "The list of items in the order with full details",
            },
        };
    }

    #! Creates the type
    constructor() : ShipStationOrderNotifyEventDataType() {
        addQoreFields(ItemFields);
    }
}

#! Ship notify event data type - includes shipment data fetched from resource_url
public class ShipStationShipNotifyEventDataType inherits ShipStationBaseEventDataType {
    public {
        #! Additional fields for ship notify events
        const ShipmentFields = {
            "shipmentId": {
                "type": IntOrNothingType,
                "display_name": "Shipment ID",
                "short_desc": "ShipStation shipment ID",
                "desc": "The unique ShipStation shipment ID",
            },
            "orderId": {
                "type": IntOrNothingType,
                "display_name": "Order ID",
                "short_desc": "Associated order ID",
                "desc": "The order ID this shipment is associated with",
            },
            "orderNumber": {
                "type": StringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "Associated order number",
                "desc": "The order number this shipment is associated with",
            },
            "trackingNumber": {
                "type": StringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "Shipment tracking number",
                "desc": "The tracking number for the shipment",
            },
            "carrierCode": {
                "type": StringOrNothingType,
                "display_name": "Carrier Code",
                "short_desc": "Carrier code",
                "desc": "The carrier code (e.g., ups, fedex, usps)",
            },
            "serviceCode": {
                "type": StringOrNothingType,
                "display_name": "Service Code",
                "short_desc": "Service code",
                "desc": "The service code for the shipment",
            },
            "shipDate": {
                "type": DateOrNothingType,
                "display_name": "Ship Date",
                "short_desc": "Ship date",
                "desc": "The date the shipment was shipped",
            },
            "createDate": {
                "type": DateOrNothingType,
                "display_name": "Create Date",
                "short_desc": "Label creation date",
                "desc": "The date the label was created",
            },
            "shipmentCost": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipment Cost",
                "short_desc": "Shipment cost",
                "desc": "The cost of the shipment",
            },
            "insuranceCost": {
                "type": SoftFloatOrNothingType,
                "display_name": "Insurance Cost",
                "short_desc": "Insurance cost",
                "desc": "The cost of insurance for the shipment",
            },
            "voided": {
                "type": SoftBoolOrNothingType,
                "display_name": "Voided",
                "short_desc": "Label voided",
                "desc": "Whether the shipment label has been voided",
            },
            "shipTo": {
                "type": new ShipStationAddressDataType(AutoHashOrNothingType),
                "display_name": "Ship To",
                "short_desc": "Shipping address",
                "desc": "The shipping address for the shipment",
            },
            "weight": {
                "type": new ShipStationWeightDataType(AutoHashOrNothingType),
                "display_name": "Weight",
                "short_desc": "Package weight",
                "desc": "The weight of the package",
            },
            "dimensions": {
                "type": new ShipStationDimensionsDataType(AutoHashOrNothingType),
                "display_name": "Dimensions",
                "short_desc": "Package dimensions",
                "desc": "The dimensions of the package",
            },
        };
    }

    #! Creates the type
    constructor() : ShipStationBaseEventDataType() {
        addQoreFields(ShipmentFields);
    }
}

#! Item ship notify event data type - includes shipment with item details
public class ShipStationItemShipNotifyEventDataType inherits ShipStationShipNotifyEventDataType {
    public {
        #! Additional fields for item ship notify events
        const ItemFields = {
            "shipmentItems": {
                "type": SoftListOrNothingType,
                "display_name": "Shipment Items",
                "short_desc": "Items in the shipment",
                "desc": "The list of items included in this shipment",
            },
        };
    }

    #! Creates the type
    constructor() : ShipStationShipNotifyEventDataType() {
        addQoreFields(ItemFields);
    }
}

} # namespace ShipStationDataProvider
