# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file SquarespaceFulfillOrderDataProvider.qc Squarespace fulfill order action data provider

/*  SquarespaceFulfillOrderDataProvider.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Request type for fulfilling an order
public class SquarespaceFulfillOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "type": StringType,
                "display_name": "Order ID",
                "short_desc": "The order ID to fulfill",
                "desc": "The order ID to fulfill",
                "required": True,
                "ref_data": "orders",
            },
            "shouldSendNotification": {
                "type": SoftBoolType,
                "display_name": "Send Notification",
                "short_desc": "Send customer notification email",
                "desc": "Send customer notification email",
                "required": True,
            },
            "shipments": {
                "type": new ListDataType("ShipmentList", new SquarespaceShipmentDataType(), True),
                "display_name": "Shipments",
                "short_desc": "Shipment tracking details",
                "desc": "The list of shipment tracking details for the fulfillment",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Squarespace fulfill order action data provider
public class SquarespaceFulfillOrderDataProvider inherits SquarespaceDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "fulfill",
            "desc": "Fulfill an order in Squarespace",
            "type": "SquarespaceFulfillOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;
    }

    constructor(SquarespaceRestClient::SquarespaceRestClient rest) : SquarespaceDataProviderBase(rest) {
    }

    string getName() {
        return "fulfill";
    }

    *string getDesc() {
        return "Fulfill an order in Squarespace";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new SquarespaceFulfillOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return AbstractDataProviderType::get(AutoHashType);
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        string order_id = remove req.order_id;
        doPost(sprintf("1.0/commerce/orders/%s/fulfillments", order_id), req);
        return {};
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace SquarespaceDataProvider
