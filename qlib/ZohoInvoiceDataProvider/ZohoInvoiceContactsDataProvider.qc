# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceContactsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Contains all public definitions in the ZohoInvoiceDataProvider module
public namespace ZohoInvoiceDataProvider {

#! Zoho Invoice contacts data provider - lists and searches contacts
public class ZohoInvoiceContactsDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "contacts",
            "desc": "Zoho Invoice contacts data provider",
            "type": "ZohoInvoiceContactsDataProvider",
            "supports_read": True,
            "supports_native_search": True,
            "constructor_options": ConstructorOptions,
            "search_options": SearchOptions,
        };

        #! Constructor options
        const ConstructorOptions = BaseConstructorOptions;

        #! Search options
        const SearchOptions = {
            "organization_id": <DataProviderOptionInfo>{
                "display_name": "Organization",
                "short_desc": "Zoho Invoice organization",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "The Zoho Invoice organization ID (handled automatically by REST client)",
            },
            "contact_id": <DataProviderOptionInfo>{
                "display_name": "Contact ID",
                "short_desc": "Get specific contact",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific contact by ID (for DPAT_FIND_SINGLE)",
            },
            "status": <DataProviderOptionInfo>{
                "display_name": "Status",
                "short_desc": "Filter by status",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter contacts by status (active, inactive, duplicate, all)",
            },
            "contact_name": <DataProviderOptionInfo>{
                "display_name": "Contact Name",
                "short_desc": "Search by name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search contacts by name (partial match)",
            },
            "company_name": <DataProviderOptionInfo>{
                "display_name": "Company Name",
                "short_desc": "Search by company",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search contacts by company name (partial match)",
            },
            "email": <DataProviderOptionInfo>{
                "display_name": "Email",
                "short_desc": "Search by email",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search contacts by email address",
            },
            "page": <DataProviderOptionInfo>{
                "display_name": "Page",
                "short_desc": "Page number",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Page number for pagination",
            },
            "per_page": <DataProviderOptionInfo>{
                "display_name": "Per Page",
                "short_desc": "Results per page",
                "type": AbstractDataProviderType::get(SoftIntOrNothingType),
                "desc": "Number of results per page (max 200)",
            },
        };
    }

    #! Creates the object with a REST client
    constructor(ZohoInvoiceRestClient::ZohoInvoiceRestClient rest) : ZohoInvoiceDataProviderBase(rest) {
    }

    #! Returns the provider name
    string getName() {
        return "contacts";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Invoice contacts data provider for listing and searching contacts";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ZohoInvoiceContactDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If contact_id is provided, fetch single contact (for DPAT_FIND_SINGLE)
        if (search_options.contact_id) {
            hash<auto> response = doGet(sprintf("/contacts/%s", search_options.contact_id));
            return new DefaultRecordIterator(new ListHashIterator((response.contact,)), where_cond, search_options);
        }

        # Build query parameters for listing contacts
        hash<auto> params;
        if (search_options.status) {
            params.status = search_options.status;
        }
        if (search_options.contact_name) {
            params.contact_name = search_options.contact_name;
        }
        if (search_options.company_name) {
            params.company_name = search_options.company_name;
        }
        if (search_options.email) {
            params.email = search_options.email;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.per_page) {
            params.per_page = search_options.per_page;
        }

        hash<auto> response = doGet("/contacts", params);
        return new DefaultRecordIterator(new ListHashIterator(response.contacts ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
