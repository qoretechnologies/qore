# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryDeleteSalesOrderDataProvider.qc Zoho Inventory delete sales order action data provider

/*  ZohoInventoryDeleteSalesOrderDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory delete sales order data provider
public class ZohoInventoryDeleteSalesOrderDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-sales-order",
            "desc": "Deletes a sales order from Zoho Inventory",
            "type": "ZohoInventoryDeleteSalesOrderDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "salesorder_id": <DataProviderOptionInfo>{
                "display_name": "Sales Order ID",
                "short_desc": "The sales order to delete",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the sales order to delete",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("ZohoInventoryDeleteSalesOrderRequest",
            ZohoInventoryDataProviderBase::OrganizationIdField + {
            "salesorder_id": {
                "display_name": "Sales Order ID",
                "type": SoftStringType,
                "desc": "The ID of the sales order to delete",
            },
        });

        #! Response type
        const ResponseType = new HashDataType("ZohoInventoryDeleteSalesOrderResponse", {
            "code": {
                "display_name": "Code",
                "type": SoftIntType,
                "desc": "Response code (0 for success)",
            },
            "message": {
                "display_name": "Message",
                "type": SoftStringType,
                "desc": "Response message",
            },
        });
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
        return "delete-sales-order";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deletes a sales order from Zoho Inventory";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return ResponseType;
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setOrganizationIdFromReq(req);
        string id = req.salesorder_id ?? request_options.salesorder_id ?? salesorder_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "salesorder_id is required for delete operation";
        }
        return doDelete(sprintf("/salesorders/%s", id));
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
