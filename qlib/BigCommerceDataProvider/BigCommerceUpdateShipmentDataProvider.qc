# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceUpdateShipmentDataProvider.qc BigCommerce update shipment action data provider

/*  BigCommerceUpdateShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for updating a shipment
public class BigCommerceUpdateShipmentRequestDataType inherits HashDataType {
    public {
        const Fields = {
            "order_id": {
                "display_name": "Order ID",
                "short_desc": "The order the shipment belongs to",
                "type": IntType,
                "desc": "The ID of the order the shipment belongs to",
                "required": True,
                "ref_data": "orders",
            },
            "shipment_id": {
                "display_name": "Shipment ID",
                "short_desc": "The shipment to update",
                "type": IntType,
                "desc": "The ID of the shipment to update",
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
            "order_address_id": {
                "display_name": "Order Address ID",
                "short_desc": "Shipping address ID",
                "type": IntOrNothingType,
                "desc": "ID of the shipping address for this shipment",
            },
            "items": {
                "display_name": "Items",
                "short_desc": "Shipment items",
                "type": AutoListOrNothingType,
                "desc": "Array of items in the shipment with `order_product_id` and `quantity`",
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce update shipment action data provider
public class BigCommerceUpdateShipmentDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update",
            "desc": "Update an existing shipment in BigCommerce",
            "type": "BigCommerceUpdateShipmentDataProvider",
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
        return "update";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Update an existing shipment in BigCommerce";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceUpdateShipmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceShipmentDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        int order_id = remove req.order_id;
        int shipment_id = remove req.shipment_id;
        return doPutV2(sprintf("orders/%d/shipments/%d", order_id, shipment_id), req);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
