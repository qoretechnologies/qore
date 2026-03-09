# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file KeapCreateOrderDataProvider.qc Keap create order action data provider

/*  KeapCreateOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the KeapDataProvider module
public namespace KeapDataProvider {

#! Request type for creating an order
public class KeapCreateOrderRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "contact_id": {
                "type": StringType,
                "display_name": "Contact ID",
                "short_desc": "The contact for the order",
                "desc": "The unique identifier of the contact associated with the order",
                "required": True,
            },
            "order_title": {
                "type": StringType,
                "display_name": "Order Title",
                "short_desc": "The order title",
                "desc": "The title of the order",
                "required": True,
                "example_value": "New Order",
            },
            "order_items": {
                "type": new ListDataType(new KeapOrderItemDataType(AutoHashOrNothingType), True),
                "display_name": "Order Items",
                "short_desc": "The order items",
                "desc": "List of order items to include in the order. Each item requires "
                    "`product_id` and `quantity`",
            },
            "order_time": {
                "type": DateOrNothingType,
                "display_name": "Order Time",
                "short_desc": "The order date/time",
                "desc": "The date/time of the order in ISO-8601 format",
            },
            "order_type": {
                "type": StringOrNothingType,
                "display_name": "Order Type",
                "short_desc": "The order type",
                "desc": "The type of the order",
                "allowed_values": (
                    <AllowedValueInfo>{"value": "OFFLINE", "desc": "Offline order"},
                    <AllowedValueInfo>{"value": "ONLINE", "desc": "Online order"},
                ),
            },
            "shipping_address": {
                "type": new KeapShippingInfoDataType(AutoHashOrNothingType),
                "display_name": "Shipping Address",
                "short_desc": "The shipping address",
                "desc": "The shipping address for the order",
            },
            "notes": {
                "type": StringOrNothingType,
                "display_name": "Notes",
                "short_desc": "Order notes",
                "desc": "Notes about the order",
            },
            "terms": {
                "type": StringOrNothingType,
                "display_name": "Terms",
                "short_desc": "Order terms",
                "desc": "The terms for the order",
            },
            "promo_codes": {
                "type": AutoListOrNothingType,
                "display_name": "Promo Codes",
                "short_desc": "Promotional codes",
                "desc": "List of promotional codes to apply to the order",
            },
            "lead_affiliate_id": {
                "type": StringOrNothingType,
                "display_name": "Lead Affiliate ID",
                "short_desc": "The lead affiliate",
                "desc": "The ID of the lead affiliate for the order",
            },
            "sales_affiliate_id": {
                "type": StringOrNothingType,
                "display_name": "Sales Affiliate ID",
                "short_desc": "The sales affiliate",
                "desc": "The ID of the sales affiliate for the order",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! Keap create order action data provider
public class KeapCreateOrderDataProvider inherits KeapDataProviderBase {
    public {
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new order in Keap",
            "type": "KeapCreateOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        const ConstructorOptions = BaseConstructorOptions;

        static *AbstractDataProviderType ResponseType = new KeapOrderDataType();
        static *AbstractDataProviderType RequestType = new KeapCreateOrderRequestDataType();
    }

    constructor(KeapRestClient::KeapRestClient rest) : KeapDataProviderBase(rest) {
    }

    string getName() {
        return "create";
    }

    *string getDesc() {
        return "Create a new order in Keap";
    }

    private *AbstractDataProviderType getRequestTypeImpl() {
        return new KeapCreateOrderRequestDataType();
    }

    private *AbstractDataProviderType getResponseTypeImpl() {
        return new KeapOrderDataType();
    }

    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doPost("orders", req);
    }

    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace KeapDataProvider
