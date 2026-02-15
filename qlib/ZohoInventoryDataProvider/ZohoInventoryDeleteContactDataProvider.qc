# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryDeleteContactDataProvider.qc Zoho Inventory delete contact action data provider

/*  ZohoInventoryDeleteContactDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory delete contact data provider
public class ZohoInventoryDeleteContactDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "delete-contact",
            "desc": "Deletes a contact from Zoho Inventory",
            "type": "ZohoInventoryDeleteContactDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "The contact to delete",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the contact to delete",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("ZohoInventoryDeleteContactRequest",
            ZohoInventoryDataProviderBase::OrganizationIdField + {
            "contact_id": {
                "display_name": "Contact ID",
                "type": SoftStringType,
                "desc": "The ID of the contact to delete",
            },
        });

        #! Response type
        const ResponseType = new HashDataType("ZohoInventoryDeleteContactResponse", {
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
        #! The contact ID
        string contact_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.contact_id) {
            contact_id = options.contact_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "delete-contact";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Deletes a contact from Zoho Inventory";
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
        string id = req.contact_id ?? request_options.contact_id ?? contact_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "contact_id is required for delete operation";
        }
        return doDelete(sprintf("/contacts/%s", id));
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
