# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInventoryDataProvider module definition

/*  ZohoInventoryContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Inventory contacts data provider
public class ZohoInventoryContactsDataProvider inherits ZohoInventoryDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Zoho Inventory contacts",
            "type": "ZohoInventoryContactsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "search_options": SearchOptions,
        };

        #! Search options
        const SearchOptions = {
            "organization_id": <DataProviderOptionInfo>{
                "display_name": "Organization ID",
                "short_desc": "Zoho Inventory organization ID",
                "type": AbstractDataProviderType::get(SoftStringType),
                "desc": "The organization ID",
                "required": True,
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Filter by contact ID",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The contact ID for retrieving a single contact",
            },
            "contact_name": <DataProviderOptionInfo>{
                "display_name": "Contact Name",
                "short_desc": "Filter by contact name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by name",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Filter by email address",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by email address",
            },
            "phone": <DataProviderOptionInfo>{
                "display_name": "Phone",
                "short_desc": "Filter by phone number",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by phone number",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by status",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "active",
                        "desc": "Active contacts",
                    },
                    <AllowedValueInfo>{
                        "value": "inactive",
                        "desc": "Inactive contacts",
                    },
                ),
            },
            "contact_type": <DataProviderOptionInfo>{
                "display_name": "Contact Type",
                "short_desc": "Filter by contact type",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by type",
                "allowed_values": (
                    <AllowedValueInfo>{
                        "value": "customer",
                        "desc": "Customer contacts",
                    },
                    <AllowedValueInfo>{
                        "value": "vendor",
                        "desc": "Vendor contacts",
                    },
                ),
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInventoryRestClient::ZohoInventoryRestClient rest) : ZohoInventoryDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ZohoInventoryContactDataType();
    }

    #! Searches for contacts
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond, *hash<auto> search_options) {
        # Support DPAT_FIND_SINGLE: get a single contact by ID
        if (search_options.contact_id) {
            hash<auto> params;
            if (search_options.organization_id) {
                params.organization_id = search_options.organization_id;
            }
            hash<auto> response = doGet(sprintf("/contacts/%s", search_options.contact_id), params);
            list<auto> records = response.contact ? (response.contact,) : ();
            return new DefaultRecordIterator(new ListHashIterator(records), where_cond, search_options);
        }

        hash<auto> params;
        if (search_options.organization_id) {
            params.organization_id = search_options.organization_id;
        }
        if (search_options.contact_name) {
            params.contact_name = search_options.contact_name;
        }
        if (search_options.email) {
            params.email = search_options.email;
        }
        if (search_options.phone) {
            params.phone = search_options.phone;
        }
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.contact_type) {
            params.contact_type = search_options.contact_type;
        }
        hash<auto> response = doGet("/contacts", params);
        return new DefaultRecordIterator(new ListHashIterator(response.contacts ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInventoryDataProvider
