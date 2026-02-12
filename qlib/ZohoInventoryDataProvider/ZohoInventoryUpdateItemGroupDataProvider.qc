# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryUpdateItemGroupDataProvider.qc Zoho Inventory update item group action data provider

/*  ZohoInventoryUpdateItemGroupDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory update item group data provider
public class ZohoInventoryUpdateItemGroupDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "update-item-group",
            "desc": "Updates an existing item group in Zoho Inventory",
            "type": "ZohoInventoryUpdateItemGroupDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "group_id": <DataProviderOptionInfo>{
                "display_name": "Item Group ID",
                "short_desc": "The item group to update",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the item group to update",
                "required": True,
            },
        };
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
        return "update-item-group";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Updates an existing item group in Zoho Inventory";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return new ZohoInventoryItemGroupUpdateDataType();
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInventoryItemGroupDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setOrganizationIdFromReq(req);
        string id = remove req.group_id ?? request_options.group_id ?? group_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "group_id is required for update operation";
        }
        hash<auto> body = req - "group_id";
        hash<auto> response = doPut(sprintf("/itemgroups/%s", id), body);
        return response.item_group;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
