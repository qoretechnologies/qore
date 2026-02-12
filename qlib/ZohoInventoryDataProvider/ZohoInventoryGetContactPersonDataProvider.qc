# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file ZohoInventoryGetContactPersonDataProvider.qc Zoho Inventory get contact person action data provider

/*  ZohoInventoryGetContactPersonDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory get contact person data provider
public class ZohoInventoryGetContactPersonDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "get-contact-person",
            "desc": "Gets details of a specific contact person from Zoho Inventory",
            "type": "ZohoInventoryGetContactPersonDataProvider",
            "supports_request": True,
            "constructor_options": ConstructorOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions + {
            "contact_person_id": <DataProviderOptionInfo>{
                "display_name": "Contact Person ID",
                "short_desc": "The contact person to retrieve",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The ID of the contact person to retrieve",
                "required": True,
            },
        };

        #! Request type
        const RequestType = new HashDataType("ZohoInventoryGetContactPersonRequest",
            ZohoInventoryDataProviderBase::OrganizationIdField + {
            "contact_person_id": {
                "display_name": "Contact Person ID",
                "type": SoftStringType,
                "desc": "The ID of the contact person to retrieve",
            },
        });
    }

    private {
        #! The contact person ID
        string contact_person_id;
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Creates the object with options
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest, *hash<auto> options) : ZohoInventoryDataProviderBase(rest) {
        if (options.contact_person_id) {
            contact_person_id = options.contact_person_id;
        }
    }

    #! Returns the provider name
    string getName() {
        return "get-contact-person";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Gets details of a specific contact person from Zoho Inventory";
    }

    #! Returns the request type
    private *AbstractDataProviderType getRequestTypeImpl() {
        return RequestType;
    }

    #! Returns the response type
    private *AbstractDataProviderType getResponseTypeImpl() {
        return new ZohoInventoryContactPersonDataType();
    }

    #! Makes a request and returns the response
    private auto doRequestImpl(auto req, *hash<auto> request_options) {
        req = setOrganizationIdFromReq(req);
        string id = req.contact_person_id ?? request_options.contact_person_id ?? contact_person_id;
        if (!id) {
            throw "ZOHOINVENTORY-ERROR", "contact_person_id is required for get operation";
        }
        hash<auto> response = doGet(sprintf("/contacts/contactpersons/%s", id));
        return response.contact_person;
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
