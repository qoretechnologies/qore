# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryUpdatePurchaseOrderDataProvider.qc Zoho Inventory update purchase order action data provider

/*  ZohoInventoryUpdatePurchaseOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory update purchase order data provider
public class ZohoInventoryUpdatePurchaseOrderDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-purchase-order",
            "desc": "Updates an existing purchase order in Zoho Inventory",
            "type": "ZohoInventoryUpdatePurchaseOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "purchaseorder_id": <DataProviderOptionInfo>{
                "display_name": "Purchase Order ID",
                "short_desc": "The purchase order to update",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the purchase order to update",
                "required": True,
            },
        };
    }

    private {
        #! The purchase order ID
        string purchaseorder_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.purchaseorder_id) {
            purchaseorder_id = options.purchaseorder_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "update-purchase-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Updates an existing purchase order in Zoho Inventory";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ZohoInventoryPurchaseOrderUpdateDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInventoryPurchaseOrderDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setOrganizationIdFromReq(req);
        string id = remove req.purchaseorder_id ?? request_options.purchaseorder_id ?? purchaseorder_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "purchaseorder_id is required for update operation";
        }
        hash<auto> body = req - "purchaseorder_id";
        hash<auto> response = doPut(sprintf("/purchaseorders/%s", id), body);
        return response.purchaseorder;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
