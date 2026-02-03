# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationCreateOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating an order
public class ShipStationCreateOrderRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "orderNumber": {
                "type": StringType,
                "display_name": "Order Number",
                "short_desc": "Your internal order number",
                "desc": "A unique identifier for the order in your system",
                "required": True,
            },
            "orderKey": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Key",
                "short_desc": "Unique order key for updates",
                "desc": "A unique identifier for the order. Used to match orders for updates via the createorder endpoint.",
            },
            "orderDate": {
                "type": DateType,
                "display_name": "Order Date",
                "short_desc": "Date the order was placed",
                "desc": "The date the order was placed",
                "required": True,
            },
            "orderStatus": {
                "type": StringType,
                "display_name": "Order Status",
                "short_desc": "Initial order status",
                "desc": "The initial status of the order",
                "required": True,
                "allowed_values": (
                    <AllowedValueInfo>{"value": "awaiting_payment", "desc": "Awaiting payment"},
                    <AllowedValueInfo>{"value": "awaiting_shipment", "desc": "Awaiting shipment"},
                    <AllowedValueInfo>{"value": "on_hold", "desc": "On hold"},
                    <AllowedValueInfo>{"value": "cancelled", "desc": "Cancelled"},
                ),
            },
            "billTo": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Bill To",
                "short_desc": "Billing address",
                "desc": "The billing address for the order",
                "required": True,
            },
            "shipTo": {
                "type": new ShipStationAddressDataType(),
                "display_name": "Ship To",
                "short_desc": "Shipping address",
                "desc": "The shipping destination address",
                "required": True,
            },
            "items": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Items",
                "short_desc": "Order line items",
                "desc": "Array of order items with sku, name, quantity, unitPrice, etc.",
            },
            "customerUsername": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Username",
                "short_desc": "Customer's marketplace username",
                "desc": "The customer's username on the selling channel",
            },
            "customerEmail": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Email",
                "short_desc": "Customer's email address",
                "desc": "The customer's email address",
            },
            "amountPaid": {
                "type": SoftFloatOrNothingType,
                "display_name": "Amount Paid",
                "short_desc": "Total amount paid",
                "desc": "The total amount paid by the customer",
            },
            "taxAmount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Tax Amount",
                "short_desc": "Tax amount",
                "desc": "The tax amount for the order",
            },
            "shippingAmount": {
                "type": SoftFloatOrNothingType,
                "display_name": "Shipping Amount",
                "short_desc": "Shipping cost",
                "desc": "The shipping cost charged to the customer",
            },
            "requestedShippingService": {
                "type": SoftStringOrNothingType,
                "display_name": "Requested Shipping Service",
                "short_desc": "Requested shipping method",
                "desc": "The shipping service requested by the customer",
            },
            "carrierCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Carrier Code",
                "short_desc": "Preferred carrier",
                "desc": "The preferred carrier code",
            },
            "serviceCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Service Code",
                "short_desc": "Preferred service",
                "desc": "The preferred service code",
            },
            "packageCode": {
                "type": SoftStringOrNothingType,
                "display_name": "Package Code",
                "short_desc": "Preferred package type",
                "desc": "The preferred package code",
            },
            "confirmation": {
                "type": SoftStringOrNothingType,
                "display_name": "Confirmation",
                "short_desc": "Delivery confirmation type",
                "desc": "The delivery confirmation type",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "none", "desc": "No confirmation"},
                    <AllowedValueInfo>{"value": "delivery", "desc": "Delivery confirmation"},
                    <AllowedValueInfo>{"value": "signature", "desc": "Signature required"},
                    <AllowedValueInfo>{"value": "adult_signature", "desc": "Adult signature required"},
                    <AllowedValueInfo>{"value": "direct_signature", "desc": "Direct signature required"},
                ),
            },
            "weight": {
                "type": new ShipStationWeightDataType(),
                "display_name": "Weight",
                "short_desc": "Package weight",
                "desc": "The weight of the shipment",
            },
            "dimensions": {
                "type": new ShipStationDimensionsDataType(),
                "display_name": "Dimensions",
                "short_desc": "Package dimensions",
                "desc": "The dimensions of the package",
            },
            "giftMessage": {
                "type": SoftStringOrNothingType,
                "display_name": "Gift Message",
                "short_desc": "Gift message",
                "desc": "A gift message to include with the order",
            },
            "gift": {
                "type": SoftBoolOrNothingType,
                "display_name": "Gift",
                "short_desc": "Is gift order",
                "desc": "Whether this is a gift order",
            },
            "internalNotes": {
                "type": SoftStringOrNothingType,
                "display_name": "Internal Notes",
                "short_desc": "Internal notes",
                "desc": "Internal notes (not visible to customer)",
            },
            "customerNotes": {
                "type": SoftStringOrNothingType,
                "display_name": "Customer Notes",
                "short_desc": "Customer notes",
                "desc": "Notes from the customer",
            },
            "advancedOptions": {
                "type": AutoHashOrNothingType,
                "display_name": "Advanced Options",
                "short_desc": "Advanced order options",
                "desc": "Advanced options including storeId, warehouseId, customField1-3, etc.",
            },
            "tagIds": {
                "type": SoftAutoListOrNothingType,
                "display_name": "Tag IDs",
                "short_desc": "Tags to apply",
                "desc": "Array of tag IDs to apply to the order",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for creating a new order
public class ShipStationCreateOrderDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create-order",
            "desc": "Create a new order in ShipStation",
            "type": "ShipStationCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationCreateOrderRequestDataType RequestType();

        #! Response type
        static ShipStationOrderDataType ResponseType();
    }

    #! Creates the object with a REST client
    constructor(ShipStationRestClient::ShipStationRestClient rest)
            : ShipStationDataProviderBase(rest) {
    }

    #! Creates the object with a connection
    constructor(ShipStationRestClient::ShipStationRestConnection conn) : ShipStationDataProviderBase(conn) {
    }

    #! Returns the provider name
    string getName() {
        return "create-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new order in ShipStation";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: creating order %s", req.orderNumber));

        # Use createorder endpoint (POST creates, but also updates if orderId provided)
        hash<auto> response = doPost("/orders/createorder", req);
        return response;
    }

    #! Returns the request type
    private *DataProvider::AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *DataProvider::AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ShipStationDataProvider
