# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryUpdateSalesOrderDataProvider.qc Zoho Inventory update sales order action data provider

/*  ZohoInventoryUpdateSalesOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory update sales order data provider
public class ZohoInventoryUpdateSalesOrderDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-sales-order",
            "desc": "Updates an existing sales order in Zoho Inventory",
            "type": "ZohoInventoryUpdateSalesOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "salesorder_id": <DataProviderOptionInfo>{
                "display_name": "Sales Order ID",
                "short_desc": "The sales order to update",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the sales order to update",
                "required": True,
            },
        };
    }

    private {
        #! The sales order ID
        string salesorder_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.salesorder_id) {
            salesorder_id = options.salesorder_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "update-sales-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Updates an existing sales order in Zoho Inventory";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ZohoInventorySalesOrderUpdateDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInventorySalesOrderDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setOrganizationIdFromReq(req);
        string id = remove req.salesorder_id ?? request_options.salesorder_id ?? salesorder_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "salesorder_id is required for update operation";
        }
        hash<auto> body = req - "salesorder_id";
        hash<auto> response = doPut(sprintf("/salesorders/%s", id), body);
        return response.salesorder;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
