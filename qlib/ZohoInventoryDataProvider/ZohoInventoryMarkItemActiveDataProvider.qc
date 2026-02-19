# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryMarkItemActiveDataProvider.qc Zoho Inventory mark item active/inactive action data provider

/*  ZohoInventoryMarkItemActiveDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory mark item active/inactive data provider
public class ZohoInventoryMarkItemActiveDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "mark-item-active",
            "desc": "Marks an item as active or inactive in Zoho Inventory",
            "type": "ZohoInventoryMarkItemActiveDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "item_id": <DataProviderOptionInfo>{
                "display_name": "Item ID",
                "short_desc": "The item to mark",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the item to mark as active or inactive",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("ZohoInventoryMarkItemActiveRequest",
            ZohoInventoryDataProviderBase::OrganizationIdField + {
            "item_id": {
                "display_name": "Item ID",
                "type": SoftStringType,
                "desc": "The ID of the item to mark as active or inactive",
                "required": True,
            },
            "active": {
                "display_name": "Active",
                "type": SoftBoolType,
                "desc": "If `True`, marks the item as active; if `False`, marks as inactive",
                "required": True,
            },
        });

        #! Response type
        const ResponseType = new HashDataType("ZohoInventoryMarkItemActiveResponse", {
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
        #! The item ID
        string item_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.item_id) {
            item_id = options.item_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "mark-item-active";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Marks an item as active or inactive in Zoho Inventory";
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
        string id = req.item_id ?? request_options.item_id ?? item_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "item_id is required for mark-item-active operation";
        }
        if (!exists req.active) {
            throw "ZOHOINVENTORY-ERROR", "active is required for mark-item-active operation";
        }

        string status = req.active ? "active" : "inactive";
        return doPost(sprintf("/items/%s/%s", id, status));
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
