# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceCreateShipmentDataProvider.qc BigCommerce create shipment action data provider

/*  BigCommerceCreateShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for creating a shipment
public class BigCommerceCreateShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "The order to ship",
                "type": IntType,
                "desc": "The ID of the order to create a shipment for",
                "required": True,
                "ref_data": "orders",
            },
            "order_address_id": {
                "display_name": "Order Address ID",
                "short_desc": "Shipping address ID",
                "type": IntType,
                "desc": "ID of the shipping address for this shipment",
                "required": True,
            },
            "items": {
                "display_name": "Items",
                "short_desc": "Items to ship",
                "type": AutoListType,
                "desc": "Array of items to ship, each with `order_product_id` and `quantity`",
                "required": True,
            },
            "tracking_number": {
                "display_name": "Tracking Number",
                "short_desc": "Tracking number",
                "type": StringOrNothingType,
                "desc": "Tracking number for the shipment",
            },
            "shipping_method": {
                "display_name": "Shipping Method",
                "short_desc": "Shipping method",
                "type": StringOrNothingType,
                "desc": "Name of the shipping method",
            },
            "shipping_provider": {
                "display_name": "Shipping Provider",
                "short_desc": "Shipping provider",
                "type": StringOrNothingType,
                "desc": "Name of the shipping provider",
            },
            "tracking_carrier": {
                "display_name": "Tracking Carrier",
                "short_desc": "Tracking carrier",
                "type": StringOrNothingType,
                "desc": "Carrier name for tracking",
            },
            "comments": {
                "display_name": "Comments",
                "short_desc": "Shipment comments",
                "type": StringOrNothingType,
                "desc": "Comments about the shipment",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce create shipment action data provider
public class BigCommerceCreateShipmentDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "create",
            "desc": "Create a new shipment for an order in BigCommerce",
            "type": "BigCommerceCreateShipmentDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;
    }

    #! Creates the object with a REST client
    constructor(BigCommerceRestClient::BigCommerceRestClient rest) : BigCommerceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "create";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Create a new shipment for an order in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceCreateShipmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceShipmentDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int order_id = remove req.order_id;
        return doPostV2(sprintf("orders/%d/shipments", order_id), req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
