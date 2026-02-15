# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryDeleteItemGroupDataProvider.qc Zoho Inventory delete item group action data provider

/*  ZohoInventoryDeleteItemGroupDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory delete item group data provider
public class ZohoInventoryDeleteItemGroupDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-item-group",
            "desc": "Deletes an item group from Zoho Inventory",
            "type": "ZohoInventoryDeleteItemGroupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "group_id": <DataProviderOptionInfo>{
                "display_name": "Item Group ID",
                "short_desc": "The item group to delete",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the item group to delete",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("ZohoInventoryDeleteItemGroupRequest",
            ZohoInventoryDataProviderBase::OrganizationIdField + {
            "group_id": {
                "display_name": "Item Group ID",
                "type": SoftStringType,
                "desc": "The ID of the item group to delete",
            },
        });

        #! Response type
        const ResponseType = new HashDataType("ZohoInventoryDeleteItemGroupResponse", {
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
        #! The item group ID
        string group_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.group_id) {
            group_id = options.group_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "delete-item-group";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deletes an item group from Zoho Inventory";
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
        string id = req.group_id ?? request_options.group_id ?? group_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "group_id is required for delete operation";
        }
        return doDelete(sprintf("/itemgroups/%s", id));
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
