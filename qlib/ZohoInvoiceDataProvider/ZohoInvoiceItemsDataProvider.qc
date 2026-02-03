# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore ZohoInvoiceDataProvider module definition

/*  ZohoInvoiceItemsDataProvider.qc Copyright 2026 Qore Technologies, s.r.o.

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

#! Zoho Invoice items data provider - lists and searches items
public class ZohoInvoiceItemsDataProvider inherits ZohoInvoiceDataProviderBase {
    public {
        #! Provider info
        const ProviderInfo = <DataProviderInfo>{
            "name": "items",
            "desc": "Zoho Invoice items data provider",
            "type": "ZohoInvoiceItemsDataProvider",
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
            "item_id": <DataProviderOptionInfo>{
                "display_name": "Item ID",
                "short_desc": "Get specific item",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Get a specific item by ID (for DPAT_FIND_SINGLE)",
            },
            "name": <DataProviderOptionInfo>{
                "display_name": "Name",
                "short_desc": "Search by item name",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Search items by name (partial match)",
            },
            "tax_id": <DataProviderOptionInfo>{
                "display_name": "Tax ID",
                "short_desc": "Filter by tax",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter items by tax ID",
            },
            "account_id": <DataProviderOptionInfo>{
                "display_name": "Account ID",
                "short_desc": "Filter by account",
                "type": AbstractDataProviderType::get(SoftStringOrNothingType),
                "desc": "Filter items by account ID",
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
        return "items";
    }

    #! Returns the provider description
    *string getDesc() {
        return "Zoho Invoice items data provider for listing and searching items";
    }

    #! Returns the record type
    private *AbstractDataProviderType getRecordTypeImpl() {
        return new ZohoInvoiceItemDataType();
    }

    #! Returns an iterator for records matching the search criteria
    private AbstractDataProviderRecordIterator searchRecordsImpl(*hash<auto> where_cond,
            *hash<auto> search_options) {
        # If item_id is provided, fetch single item (for DPAT_FIND_SINGLE)
        if (search_options.item_id) {
            hash<auto> response = doGet(sprintf("/items/%s", search_options.item_id));
            return new DefaultRecordIterator(new ListHashIterator((response.item,)), where_cond, search_options);
        }

        # Build query parameters for listing items
        hash<auto> params;
        if (search_options.name) {
            params.name = search_options.name;
        }
        if (search_options.tax_id) {
            params.tax_id = search_options.tax_id;
        }
        if (search_options.account_id) {
            params.account_id = search_options.account_id;
        }
        if (search_options.page) {
            params.page = search_options.page;
        }
        if (search_options.per_page) {
            params.per_page = search_options.per_page;
        }

        hash<auto> response = doGet("/items", params);
        return new DefaultRecordIterator(new ListHashIterator(response.items ?? ()), where_cond, search_options);
    }

    #! Returns static provider info
    private hash<DataProviderInfo> getStaticInfoImpl() {
        return ProviderInfo;
    }
}

} # namespace ZohoInvoiceDataProvider
