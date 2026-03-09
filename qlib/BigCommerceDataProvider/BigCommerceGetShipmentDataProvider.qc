# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file BigCommerceGetShipmentDataProvider.qc BigCommerce get shipment action data provider

/*  BigCommerceGetShipmentDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Request type for getting a shipment
public class BigCommerceGetShipmentRequestDataType inherits HashDataType {
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
                "short_desc": "The shipment to retrieve",
                "type": IntType,
                "desc": "The ID of the shipment to retrieve",
                "required": True,
            },
        };
    }

    constructor() {
        addQoreFields(Fields);
    }
}

#! BigCommerce get shipment action data provider
public class BigCommerceGetShipmentDataProvider inherits BigCommerceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get",
            "desc": "Get a shipment from BigCommerce by order ID and shipment ID",
            "type": "BigCommerceGetShipmentDataProvider",
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
        return "get";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Get a shipment from BigCommerce by order ID and shipment ID";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new BigCommerceGetShipmentRequestDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new BigCommerceShipmentDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        return doGetV2(sprintf("orders/%d/shipments/%d", req.order_id, req.shipment_id));
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace BigCommerceDataProvider
