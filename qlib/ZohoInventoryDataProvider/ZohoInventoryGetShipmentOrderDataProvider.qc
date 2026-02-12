# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryGetShipmentOrderDataProvider.qc Zoho Inventory get shipment order action data provider

/*  ZohoInventoryGetShipmentOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInventoryDataProvider module
public namespace ZohoInventoryDataProvider {

#! Zoho Inventory get shipment order data provider
public class ZohoInventoryGetShipmentOrderDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-shipment-order",
            "desc": "Gets details of a specific shipment order from Zoho Inventory",
            "type": "ZohoInventoryGetShipmentOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "shipment_id": <DataProviderOptionInfo>{
                "display_name": "Shipment ID",
                "short_desc": "The shipment order to retrieve",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the shipment order to retrieve",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("ZohoInventoryGetShipmentOrderRequest",
            ZohoInventoryDataProviderBase::OrganizationIdField + {
            "shipment_id": {
                "display_name": "Shipment ID",
                "type": SoftStringType,
                "desc": "The ID of the shipment order to retrieve",
            },
        });
    }

    private {
        #! The shipment ID
        string shipment_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.shipment_id) {
            shipment_id = options.shipment_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "get-shipment-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gets details of a specific shipment order from Zoho Inventory";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInventoryShipmentOrderDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setOrganizationIdFromReq(req);
        string id = req.shipment_id ?? request_options.shipment_id ?? shipment_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "shipment_id is required for get operation";
        }
        hash<auto> response = doGet(sprintf("/shipmentorders/%s", id));
        return response.shipment_order;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
