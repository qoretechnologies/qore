# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ShipStationDataProvider module definition

/*  ShipStationMarkAsShippedDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for marking an order as shipped
public class ShipStationMarkAsShippedRequestDataType inherits HashDataType {
    private {
        const Fields = {
            "orderId": {
                "type": IntType,
                "display_name": "Order ID",
                "short_desc": "The order ID to mark as shipped",
                "desc": "The ShipStation order ID to mark as shipped",
                "required": True,
            },
            "carrierCode": {
                "type": StringType,
                "display_name": "Carrier Code",
                "short_desc": "The shipping carrier code",
                "desc": "The carrier code (e.g., 'ups', 'fedex', 'stamps_com')",
                "required": True,
            },
            "shipDate": {
                "type": SoftStringOrNothingType,
                "display_name": "Ship Date",
                "short_desc": "The date the order was shipped",
                "desc": "The ship date (ISO 8601 format). Defaults to current date if not specified.",
            },
            "trackingNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Tracking Number",
                "short_desc": "The shipment tracking number",
                "desc": "The tracking number for the shipment",
            },
            "notifyCustomer": {
                "type": SoftBoolOrNothingType,
                "display_name": "Notify Customer",
                "short_desc": "Send shipment notification email",
                "desc": "Whether to send a shipment notification email to the customer",
            },
            "notifySalesChannel": {
                "type": SoftBoolOrNothingType,
                "display_name": "Notify Sales Channel",
                "short_desc": "Notify the selling channel",
                "desc": "Whether to notify the sales channel (marketplace) of the shipment",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Response type for mark as shipped
public class ShipStationMarkAsShippedResponseDataType inherits HashDataType {
    private {
        const Fields = {
            "orderId": {
                "type": IntType,
                "display_name": "Order ID",
                "short_desc": "The order ID",
                "desc": "The order ID that was marked as shipped",
            },
            "orderNumber": {
                "type": SoftStringOrNothingType,
                "display_name": "Order Number",
                "short_desc": "The order number",
                "desc": "The order number",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Data provider for marking an order as shipped
public class ShipStationMarkAsShippedDataProvider inherits ShipStationDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-order-as-shipped",
            "desc": "Mark a ShipStation order as shipped without creating a label",
            "type": "ShipStationMarkAsShippedDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Request type
        static ShipStationMarkAsShippedRequestDataType RequestType();

        #! Response type
        static ShipStationMarkAsShippedResponseDataType ResponseType();
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
        return "mark-order-as-shipped";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Mark a ShipStation order as shipped without creating a label";
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        @debug(log(LoggerLevel::DEBUG, "doRequestImpl: marking order %d as shipped", req.orderId));

        hash<auto> response = doPost("/orders/markasshipped", req);
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
