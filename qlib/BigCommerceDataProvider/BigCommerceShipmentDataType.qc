# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceShipmentDataType.qc defines the BigCommerceShipmentDataType class

/*  BigCommerceShipmentDataType.qc Copyright (C) 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the BigCommerceDataProvider module
public namespace BigCommerceDataProvider {

#! Data type for BigCommerce shipment records (V2 API format)
public class BigCommerceShipmentDataType inherits HashDataType {
    public {
        #! Field definitions
        const Fields = {
            "id": {
                "display_name": "ID",
                "type": IntType,
                "desc": "Unique numeric ID of the shipment",
            },
            "order_id": {
                "display_name": "Order ID",
                "type": IntType,
                "desc": "ID of the order this shipment belongs to",
            },
            "customer_id": {
                "display_name": "Customer ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the customer",
            },
            "order_address_id": {
                "display_name": "Order Address ID",
                "type": SoftIntOrNothingType,
                "desc": "ID of the shipping address for the order",
            },
            "date_created": {
                "display_name": "Date Created",
                "type": SoftDateOrNothingType,
                "desc": "Date/time the shipment was created",
            },
            "tracking_number": {
                "display_name": "Tracking Number",
                "type": SoftStringOrNothingType,
                "desc": "Tracking number for the shipment",
            },
            "shipping_method": {
                "display_name": "Shipping Method",
                "type": SoftStringOrNothingType,
                "desc": "Name of the shipping method",
            },
            "shipping_provider": {
                "display_name": "Shipping Provider",
                "type": SoftStringOrNothingType,
                "desc": "Name of the shipping provider",
            },
            "tracking_carrier": {
                "display_name": "Tracking Carrier",
                "type": SoftStringOrNothingType,
                "desc": "Carrier name for tracking",
            },
            "tracking_link": {
                "display_name": "Tracking Link",
                "type": SoftStringOrNothingType,
                "desc": "Custom tracking URL",
            },
            "comments": {
                "display_name": "Comments",
                "type": SoftStringOrNothingType,
                "desc": "Comments about the shipment",
            },
            "items": {
                "display_name": "Items",
                "type": AutoListOrNothingType,
                "desc": "Array of items in the shipment with order_product_id and quantity",
            },
        };
    }

    #! Creates the type
    constructor() : HashDataType("BigCommerceShipment") {
        addQoreFields(Fields);
    }
}

} # namespace BigCommerceDataProvider
